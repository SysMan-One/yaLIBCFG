#ifndef	__YALIBCFGDEF$H__
#define	__YALIBCFGDEF$H__	1

#define	__YALIBCFGDEF_IDENT__	"X.01-03ECO03"

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
**	X.01-02		22-NOV-2026	RRL
**		CFG$M_SKIPERRS, CFG$K_TYPE_JUNK, CFG$_SKIPPED.
**
**	X.01-03		22-NOV-2026	RRL
**		Integers: octal with a leading 0, the radix of DCL; ! comments.
**
**	X.01-03ECO01	22-NOV-2026	RRL
**		CFGTOOL tells this IDENT in its banner.
**
**	X.01-03ECO02	22-NOV-2026	RRL
**		CFGTOOL help: the comments and the integers of a file.
**
**	X.01-03ECO03	22-NOV-2026	RRL
**		CFG$SET_VALUE (CFGTOOL SET): a plain number keeps the old form and width.
**
**--
*/

#include	"utility_routines.h"
#include	"starlet.h"

/*
**  $FAO of StarLet 1.8.0 formats into a buffer with the parameters written plain.  Built with an
**  older StarLet, the same macro over __UTIL$FAO stands in (without the parameter count).  Goes
**  when every machine has StarLet 1.8.
*/
#ifndef	$FAO
#define	$FAO(ctl, outlen, dsc, ...)	__util$fao(ctl, outlen, dsc FAO$_EACH(__VA_ARGS__))
#endif

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
	CFG$K_MSG_INCTWICE,			/* A file is included twice			*/
	CFG$K_MSG_BROKEN,			/* The tree comes from a failed parse		*/
	CFG$K_MSG_INVALID,			/* The tree does not match its schema		*/
	CFG$K_MSG_VTYPE,			/* Schema: a value of another type		*/
	CFG$K_MSG_RANGE,			/* Schema: a number out of its range		*/
	CFG$K_MSG_LENGTH,			/* Schema: a string of a wrong length		*/
	CFG$K_MSG_COUNT,			/* Schema: a wrong number of elements		*/
	CFG$K_MSG_BADKWD,			/* Schema: not one of the keywords		*/
	CFG$K_MSG_BADADDR,			/* Schema: not an IPv4, IPv6 or MAC address	*/
	CFG$K_MSG_REQUIRED,			/* Schema: a required setting is missing	*/
	CFG$K_MSG_UNKNOWN,			/* Schema: a setting the schema does not know	*/
	CFG$K_MSG_DUPKEY,			/* Schema: two elements of a list, one key	*/
	CFG$K_MSG_CHECK,			/* Schema: the check routine refused a value	*/
	CFG$K_MSG_DEPRECATED,			/* Schema: a setting no longer in use		*/
	CFG$K_MSG_MOREERRS,			/* More diagnostics than were kept		*/
	CFG$K_MSG_BADSCHEMA,			/* The schema itself is wrong			*/
	CFG$K_MSG_CORRECT,			/* CFGTOOL CHECK: the file is correct		*/
	CFG$K_MSG_IDENTICAL,			/* CFGTOOL DIFF: no differences			*/
	CFG$K_MSG_DIFFERENT,			/* CFGTOOL DIFF: there are differences		*/
	CFG$K_MSG_NOSETTING,			/* CFGTOOL: no setting on the path		*/
	CFG$K_MSG_MODIFIED,			/* CFGTOOL SET: a setting set			*/
	CFG$K_MSG_DELETED,			/* CFGTOOL DELETE: a setting deleted		*/
	CFG$K_MSG_CANTSET,			/* CFGTOOL SET, DELETE: refused			*/
	CFG$K_MSG_OPENOUT,			/* CFGTOOL: cannot create the output file	*/
	CFG$K_MSG_INCMODIFIED,			/* A change in an included file, WRITE_FILE	*/
	CFG$K_MSG_ARGLONG,			/* CFGTOOL: an argument over 255 octets		*/
	CFG$K_MSG_SKIPPED,			/* CFG$M_SKIPERRS: bad settings were skipped	*/
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
#define	CFG$_INCTWICE	$CFGSTS(CFG$K_MSG_INCTWICE,	STS$K_ERROR)
#define	CFG$_BROKEN	$CFGSTS(CFG$K_MSG_BROKEN,	STS$K_ERROR)
#define	CFG$_INVALID	$CFGSTS(CFG$K_MSG_INVALID,	STS$K_ERROR)
#define	CFG$_VTYPE	$CFGSTS(CFG$K_MSG_VTYPE,	STS$K_ERROR)
#define	CFG$_RANGE	$CFGSTS(CFG$K_MSG_RANGE,	STS$K_ERROR)
#define	CFG$_LENGTH	$CFGSTS(CFG$K_MSG_LENGTH,	STS$K_ERROR)
#define	CFG$_COUNT	$CFGSTS(CFG$K_MSG_COUNT,	STS$K_ERROR)
#define	CFG$_BADKWD	$CFGSTS(CFG$K_MSG_BADKWD,	STS$K_ERROR)
#define	CFG$_BADADDR	$CFGSTS(CFG$K_MSG_BADADDR,	STS$K_ERROR)
#define	CFG$_REQUIRED	$CFGSTS(CFG$K_MSG_REQUIRED,	STS$K_ERROR)
#define	CFG$_UNKNOWN	$CFGSTS(CFG$K_MSG_UNKNOWN,	STS$K_ERROR)
#define	CFG$_DUPKEY	$CFGSTS(CFG$K_MSG_DUPKEY,	STS$K_ERROR)
#define	CFG$_CHECK	$CFGSTS(CFG$K_MSG_CHECK,	STS$K_ERROR)
#define	CFG$_DEPRECATED	$CFGSTS(CFG$K_MSG_DEPRECATED,	STS$K_WARN)
#define	CFG$_MOREERRS	$CFGSTS(CFG$K_MSG_MOREERRS,	STS$K_INFO)
#define	CFG$_BADSCHEMA	$CFGSTS(CFG$K_MSG_BADSCHEMA,	STS$K_ERROR)
#define	CFG$_CORRECT	$CFGSTS(CFG$K_MSG_CORRECT,	STS$K_SUCCESS)
#define	CFG$_IDENTICAL	$CFGSTS(CFG$K_MSG_IDENTICAL,	STS$K_SUCCESS)
#define	CFG$_DIFFERENT	$CFGSTS(CFG$K_MSG_DIFFERENT,	STS$K_INFO)
#define	CFG$_NOSETTING	$CFGSTS(CFG$K_MSG_NOSETTING,	STS$K_ERROR)
#define	CFG$_MODIFIED	$CFGSTS(CFG$K_MSG_MODIFIED,	STS$K_SUCCESS)
#define	CFG$_DELETED	$CFGSTS(CFG$K_MSG_DELETED,	STS$K_SUCCESS)
#define	CFG$_CANTSET	$CFGSTS(CFG$K_MSG_CANTSET,	STS$K_ERROR)
#define	CFG$_OPENOUT	$CFGSTS(CFG$K_MSG_OPENOUT,	STS$K_ERROR)
#define	CFG$_INCMODIFIED $CFGSTS(CFG$K_MSG_INCMODIFIED,	STS$K_ERROR)
#define	CFG$_ARGLONG	$CFGSTS(CFG$K_MSG_ARGLONG,	STS$K_ERROR)
#define	CFG$_SKIPPED	$CFGSTS(CFG$K_MSG_SKIPPED,	STS$K_INFO)

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
	CFG$K_TYPE_INCLUDE,			/* @include "file" - kept for the write-back	*/
	CFG$K_TYPE_JUNK,			/* a bad setting skipped (CFG$M_SKIPERRS) - its	*/
						/* text kept for the write-back, nothing else	*/

	/* Types of a schema only: strings with a meaning, checked by CFG$VALIDATE */
	CFG$K_TYPE_KWD		= 32,		/* one of the keywords of KWD			*/
	CFG$K_TYPE_IPV4,			/* "192.168.1.1"				*/
	CFG$K_TYPE_IPV6,			/* "2001:db8::1"				*/
	CFG$K_TYPE_IPADDR,			/* either of them				*/
	CFG$K_TYPE_MAC				/* "00:1b:44:11:3a:b7" (or with '-')		*/
	};

/*
**  Flags of an entry of a schema (CFG$SCHEMA.FLAGS)
*/
#define	CFG$M_REQUIRED		0x0001		/* must be there				*/
#define	CFG$M_KEY		0x0002		/* the key of the elements of a list: required,	*/
						/* unique in the list; CFG$DIFF matches by it	*/
#define	CFG$M_RANGE		0x0004		/* MIN..MAX (FMIN..FMAX for a float) apply	*/
#define	CFG$M_LENGTH		0x0008		/* a string: its length within MIN..MAX		*/
#define	CFG$M_COUNT		0x0010		/* a list, an array: elements within MIN..MAX	*/
#define	CFG$M_DEPRECATED	0x0020		/* still taken, with a warning			*/
#define	CFG$M_OPEN		0x0040		/* a group: settings the schema does not name	*/
						/* are taken without a word			*/
#define	CFG$M_CASEKWD		0x0080		/* keywords compared with regard to case	*/

/*
**  Options of the parse (CFG$PARSE_FILE, CFG$PARSE_BUF)
*/
#define	CFG$M_NOCASE		1		/* Names are looked up regardless of case	*/
#define	CFG$M_NOINCLUDE		2		/* @include is refused				*/
#define	CFG$M_SKIPERRS		4		/* a bad setting is skipped, the parse goes on	*/

/*
**  Comparison (CFG$DIFF): the kinds of a difference, the options
*/
enum	{
	CFG$K_DIFF_MOD		= 1,		/* the value differs				*/
	CFG$K_DIFF_ADD,				/* in the new tree only				*/
	CFG$K_DIFF_DEL				/* in the old tree only				*/
	};

#define	CFG$M_POSITIONAL	1		/* lists matched by position, never by key	*/
#define	CFG$M_NOTIME		2		/* listing without times - the same text every run */

/*
**  Options of CFG$NODE_TEXT
*/
#define	CFG$M_TEXT_NAME		1		/* "name = value;" for a setting		*/
#define	CFG$M_TEXT_FLAT		2		/* on one line					*/

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
