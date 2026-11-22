#define	__MODULE__	"CFG_MSG"
#define	__IDENT__	"X.01-01"
#define	__REV__		"1.1.0"
#define	__FAC__		"CFG"

/*
**++
**
**  FACILITY:	yaLIBCFG - configuration trees in the libconfig syntax
**
**  MODULE:	cfg_msg.c
**
**  ABSTRACT:	The message catalogue of the CFG facility; the record of the diagnostics of a
**		parse in the tree and their output.
**
**  DESCRIPTION: The library puts nothing out by itself.  A routine that meets an error in a
**		file records it in the tree (CFG$$ERROR) with the position and returns the
**		condition value; the caller decides whether to show the record (CFG$PUTERRS).
**
**  AUTHOR:	StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
**
**  CREATION DATE:  22-NOV-2026
**
**  MODIFICATION HISTORY:
**
**	X.01-01		22-NOV-2026	RRL
**		First edition.
**
**--
*/

#include	<string.h>
#include	<errno.h>

#include	"cfg_int.h"

/*
**  Positional messages take: the detail (length, address), the file (length, address), the
**  line and the column.  OPENIN, READERR, WRITERR take: the file (length, address), errno.
*/
#define	$CFGREC(a_sts, a_txt)	{ .sts = (int) (a_sts), .textl = (unsigned char) (sizeof(a_txt) - 1), .text = a_txt }
#define	$CFGPOS			", !AD line !UL column !UL"

static	EMSG_RECORD	s_msgtab [] = {
	$CFGREC(CFG$_NORMAL,	"NORMAL, normal successful completion"),
	$CFGREC(CFG$_SYNTAX,	"SYNTAX, syntax error: !AD" $CFGPOS),
	$CFGREC(CFG$_UNTERM,	"UNTERM, unterminated !AD" $CFGPOS),
	$CFGREC(CFG$_BADNUM,	"BADNUM, bad number !AD" $CFGPOS),
	$CFGREC(CFG$_BADESC,	"BADESC, bad escape sequence !AD" $CFGPOS),
	$CFGREC(CFG$_DUPNAME,	"DUPNAME, duplicate setting !AD" $CFGPOS),
	$CFGREC(CFG$_MIXARRAY,	"MIXARRAY, array element !AD is not of the type of the first" $CFGPOS),
	$CFGREC(CFG$_INCLUDE,	"INCLUDE, cannot include !AD" $CFGPOS),
	$CFGREC(CFG$_INCDEPTH,	"INCDEPTH, include files nested too deep: !AD" $CFGPOS),
	$CFGREC(CFG$_INCLOOP,	"INCLOOP, file includes itself: !AD" $CFGPOS),
	$CFGREC(CFG$_OPENIN,	"OPENIN, cannot open !AD, errno=!SL"),
	$CFGREC(CFG$_READERR,	"READERR, error reading !AD, errno=!SL"),
	$CFGREC(CFG$_WRITERR,	"WRITERR, error writing !AD, errno=!SL"),
	$CFGREC(CFG$_NOTFOUND,	"NOTFOUND, no setting on the path"),
	$CFGREC(CFG$_BADPATH,	"BADPATH, malformed path"),
	$CFGREC(CFG$_TYPE,	"TYPE, the setting is of another type"),
	$CFGREC(CFG$_EXISTS,	"EXISTS, a setting of that name exists already"),
	$CFGREC(CFG$_BADNAME,	"BADNAME, not a valid setting name"),
	$CFGREC(CFG$_BADARG,	"BADARG, bad argument"),
	$CFGREC(CFG$_NOMEM,	"NOMEM, memory allocation failure"),
	$CFGREC(CFG$_TOOBIG,	"TOOBIG, !AD is too big" $CFGPOS),
	$CFGREC(CFG$_INCTWICE,	"INCTWICE, file included a second time: !AD" $CFGPOS),
	$CFGREC(CFG$_BROKEN,	"BROKEN, the tree comes from a failed parse")
	};

static	EMSG_RECORD_DESC	s_msgdsc = {
	.msgnr		= (unsigned) $ARRSZ(s_msgtab),
	.facno		= CFG$K_FACNO,
	.msgrec		= s_msgtab,
	.facname	= __FAC__
	};



/*
**  DESCRIPTION: Register the message catalogue of the facility.
**
**  RETURNS:
**	STS$K_SUCCESS	- registered;
**	STS$K_WARN	- registered already (not an error)
*/
unsigned	cfg$inimsg	(void)
{
	return	__util$inimsg(&s_msgdsc);
}



/*
**  DESCRIPTION: Record a diagnostic of a source file in the tree.
**
**  INPUTS:
**	a_tree:		the tree
**	a_sts:		condition value, CFG$_*
**	a_srcno:	source file
**	a_line,a_col:	position, from 1
**	a_text,a_len:	the detail, cut to the size of the record
**
**  IMPLICIT OUTPUTS:
**	a record in ERR of the tree; past CFG$K_MAXERRS the diagnostic is dropped
**
**  RETURNS:
**	a_sts - so that the caller may return it at once
*/
unsigned	cfg$$error	(
		CFG$TREE	*a_tree,
		unsigned	a_sts,
		uint32_t	a_srcno,
		uint32_t	a_line,
		uint32_t	a_col,
	const	char		*a_text,
		size_t		a_len
			)
{
CFG$ERR	*l_err;

	/* No tree - a scan of a value of a path (CFG_TREE.C): nothing to record */
	if ( !a_tree || (a_tree->nerr >= CFG$K_MAXERRS) )
		return	a_sts;

	l_err = &a_tree->err[a_tree->nerr++];
	memset(l_err, 0, sizeof(CFG$ERR));

	l_err->sts = a_sts;
	l_err->srcno = (uint16_t) a_srcno;
	l_err->haspos = 1;
	l_err->line = a_line;
	l_err->col = a_col;
	l_err->len = (uint32_t) ((a_len < sizeof(l_err->text)) ? a_len : sizeof(l_err->text));
	memcpy(l_err->text, a_text, l_err->len);

	return	a_sts;
}



/*
**  DESCRIPTION: Record a failure of a system call on a file (OPENIN, READERR, WRITERR).
**
**  INPUTS:
**	a_tree:		the tree
**	a_sts:		condition value
**	a_what:		file name, zero-terminated
**	a_err:		errno
**
**  RETURNS:
**	a_sts
*/
unsigned	cfg$$syserr	(
		CFG$TREE	*a_tree,
		unsigned	a_sts,
	const	char		*a_what,
		int		a_err
			)
{
CFG$ERR	*l_err;
size_t	l_len = strnlen(a_what, sizeof(l_err->text));

	if ( a_tree->nerr >= CFG$K_MAXERRS )
		return	a_sts;

	l_err = &a_tree->err[a_tree->nerr++];
	memset(l_err, 0, sizeof(CFG$ERR));

	l_err->sts = a_sts;
	l_err->err = a_err;
	l_err->len = (uint32_t) l_len;
	memcpy(l_err->text, a_what, l_len);

	return	a_sts;
}



/*
**  DESCRIPTION: Put out the diagnostics recorded in the tree, in the order they were met.
**
**  INPUTS:
**	a_tree:	the tree; may be NULL - nothing to do
**
**  RETURNS:
**	STS$K_SUCCESS	- always
*/
unsigned	cfg$puterrs	(
		CFG$TREE	*a_tree
			)
{
CFG$ERR		*l_err;
const	char	*l_file;

	if ( !a_tree )
		return	STS$K_SUCCESS;

	for ( uint32_t i = 0; i < a_tree->nerr; i++ )
		{
		l_err = &a_tree->err[i];

		if ( !l_err->haspos )
			{
			$PUTMSG_FAO(l_err->sts, l_err->len, l_err->text, l_err->err);
			continue;
			}

		l_file = (l_err->srcno < a_tree->nsrc) ? a_tree->src[l_err->srcno]->path : "?";

		$PUTMSG_FAO(l_err->sts, l_err->len, l_err->text, strlen(l_file), l_file, l_err->line, l_err->col);
		}

	return	STS$K_SUCCESS;
}
