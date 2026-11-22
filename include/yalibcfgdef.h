#ifndef	__YALIBCFGDEF$H__
#define	__YALIBCFGDEF$H__	1

#define	__YALIBCFGDEF_IDENT__	"X.01-01"

/*
**++
**
**  FACILITY:	yaLIBCFG - configuration trees in the libconfig syntax
**
**  MODULE:	yalibcfgdef.h
**
**  ABSTRACT:	Condition values of the CFG facility, limits, flags and node types.
**
**  AUTHOR:	StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
**
**  CREATION DATE:  22-NOV-2026
**
**  MODIFICATION HISTORY:
**
**	X.01-01		22-NOV-2026	RRL
**		First edition: stage 1 - parse, tree, paths, lossless write.
**
**--
*/

#include	"utility_routines.h"
#include	"starlet.h"

#ifdef	__cplusplus
extern "C" {
#endif

/*
**  Facility number - see utility_routines/doc/FACILITIES.txt
*/
#define	CFG$K_FACNO		0x0114
#define	$CFGSTS(msg, sev)	( $DEFFAC(CFG$K_FACNO) | $DEFMSG(msg) | $DEFSEV(sev) )

/*
**  Message numbers.  The text of every message lives in the catalogue of
**  CFG_MSG.C.  The messages that carry a position all end in the same way:
**  ", <file> line <n> column <m>".
*/
enum	{
	CFG$K_MSG_NORMAL	= 1,		/* Normal successful completion			*/
	CFG$K_MSG_SYNTAX,			/* Syntax error					*/
	CFG$K_MSG_UNTERM,			/* Unterminated string, comment, bracket	*/
	CFG$K_MSG_BADNUM,			/* Malformed or out-of-range number		*/
	CFG$K_MSG_BADESC,			/* Bad escape sequence in a string		*/
	CFG$K_MSG_DUPNAME,			/* Duplicate setting in a group			*/
	CFG$K_MSG_MIXARRAY,			/* Array elements of different types		*/
	CFG$K_MSG_INCLUDE,			/* Cannot open an include file			*/
	CFG$K_MSG_INCDEPTH,			/* Include files nested too deep		*/
	CFG$K_MSG_INCLOOP,			/* A file includes itself			*/
	CFG$K_MSG_OPENIN,			/* Cannot open the input file			*/
	CFG$K_MSG_READERR,			/* Error reading a file				*/
	CFG$K_MSG_WRITERR,			/* Error writing a file				*/
	CFG$K_MSG_NOTFOUND,			/* No setting on the path			*/
	CFG$K_MSG_BADPATH,			/* Malformed path				*/
	CFG$K_MSG_TYPE,				/* The setting is of another type		*/
	CFG$K_MSG_EXISTS,			/* A setting of that name exists already	*/
	CFG$K_MSG_BADNAME,			/* Not a valid setting name			*/
	CFG$K_MSG_BADARG,			/* Bad argument					*/
	CFG$K_MSG_NOMEM,			/* Memory allocation failure			*/
	CFG$K_MSG_TOOBIG,			/* A file or a value is too big			*/
	CFG$K_MSG_MAX				/* End-Of-List					*/
	};

#define	CFG$_NORMAL	$CFGSTS(CFG$K_MSG_NORMAL,	STS$K_SUCCESS)
#define	CFG$_SYNTAX	$CFGSTS(CFG$K_MSG_SYNTAX,	STS$K_ERROR)
#define	CFG$_UNTERM	$CFGSTS(CFG$K_MSG_UNTERM,	STS$K_ERROR)
#define	CFG$_BADNUM	$CFGSTS(CFG$K_MSG_BADNUM,	STS$K_ERROR)
#define	CFG$_BADESC	$CFGSTS(CFG$K_MSG_BADESC,	STS$K_ERROR)
#define	CFG$_DUPNAME	$CFGSTS(CFG$K_MSG_DUPNAME,	STS$K_ERROR)
#define	CFG$_MIXARRAY	$CFGSTS(CFG$K_MSG_MIXARRAY,	STS$K_ERROR)
#define	CFG$_INCLUDE	$CFGSTS(CFG$K_MSG_INCLUDE,	STS$K_ERROR)
#define	CFG$_INCDEPTH	$CFGSTS(CFG$K_MSG_INCDEPTH,	STS$K_ERROR)
#define	CFG$_INCLOOP	$CFGSTS(CFG$K_MSG_INCLOOP,	STS$K_ERROR)
#define	CFG$_OPENIN	$CFGSTS(CFG$K_MSG_OPENIN,	STS$K_ERROR)
#define	CFG$_READERR	$CFGSTS(CFG$K_MSG_READERR,	STS$K_ERROR)
#define	CFG$_WRITERR	$CFGSTS(CFG$K_MSG_WRITERR,	STS$K_ERROR)
#define	CFG$_NOTFOUND	$CFGSTS(CFG$K_MSG_NOTFOUND,	STS$K_WARN)
#define	CFG$_BADPATH	$CFGSTS(CFG$K_MSG_BADPATH,	STS$K_ERROR)
#define	CFG$_TYPE	$CFGSTS(CFG$K_MSG_TYPE,		STS$K_ERROR)
#define	CFG$_EXISTS	$CFGSTS(CFG$K_MSG_EXISTS,	STS$K_ERROR)
#define	CFG$_BADNAME	$CFGSTS(CFG$K_MSG_BADNAME,	STS$K_ERROR)
#define	CFG$_BADARG	$CFGSTS(CFG$K_MSG_BADARG,	STS$K_ERROR)
#define	CFG$_NOMEM	$CFGSTS(CFG$K_MSG_NOMEM,	STS$K_FATAL)
#define	CFG$_TOOBIG	$CFGSTS(CFG$K_MSG_TOOBIG,	STS$K_ERROR)

/*
**  Node types
*/
enum	{
	CFG$K_TYPE_NONE		= 0,
	CFG$K_TYPE_GROUP,			/* { name = value; ... }			*/
	CFG$K_TYPE_LIST,			/* ( value, ... ) - values of any type		*/
	CFG$K_TYPE_ARRAY,			/* [ scalar, ... ] - scalars of one type	*/
	CFG$K_TYPE_INT,				/* 32-bit integer				*/
	CFG$K_TYPE_INT64,			/* 64-bit integer				*/
	CFG$K_TYPE_FLOAT,			/* double					*/
	CFG$K_TYPE_BOOL,			/* true / false					*/
	CFG$K_TYPE_STRING,			/* "..."					*/
	CFG$K_TYPE_INCLUDE			/* @include "file" - kept for the write-back	*/
	};

/*
**  Options of the parse (CFG$PARSE_FILE, CFG$PARSE_BUF)
*/
#define	CFG$M_NOCASE		1		/* Names are looked up regardless of case	*/
#define	CFG$M_NOINCLUDE		2		/* @include is refused				*/

/*
**  Limits
*/
#define	CFG$K_MAXNAME		255		/* Longest setting name (ASC)			*/
#define	CFG$K_MAXINCDEPTH	10		/* Deepest nesting of @include			*/
#define	CFG$K_MAXSRC		64		/* Most source files in one tree		*/
#define	CFG$K_MAXFILE		(64U * 1024U * 1024U)	/* Biggest file that is read		*/
#define	CFG$K_MAXERRS		32		/* Most errors kept in a tree			*/
#define	CFG$K_MAXDEPTH		256		/* Deepest nesting of values			*/

#ifdef	__cplusplus
}
#endif

#endif	/* __YALIBCFGDEF$H__ */
