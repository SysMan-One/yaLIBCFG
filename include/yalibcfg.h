#ifndef	__YALIBCFG$H__
#define	__YALIBCFG$H__	1

/*
**++
**
**  FACILITY:	yaLIBCFG - configuration trees in the libconfig syntax
**
**  MODULE:	yalibcfg.h
**
**  ABSTRACT:	The programming interface of the library.
**
**  DESCRIPTION: A file is parsed into a tree of nodes.  Every node remembers where in its source
**		file it came from, so that a tree written back without changes is the file byte for
**		byte, and a tree changed by the program is written back with only the changed
**		pieces replaced - comments and layout stay.
**
**		All the routines return a condition value (success - the low bit set); results go
**		out through output parameters.  Nothing is put out by the library itself:
**		positional diagnostics of a parse are kept in the tree and put out by CFG$PUTERRS
**		when the caller wishes.
**
**		A tree belongs to one thread at a time; different trees are independent.
**		CFG$INIMSG registers the message catalogue and must be called once, before the
**		first CFG$PUTERRS or $PUTMSG_FAO of a CFG$_ value; it is not thread-safe.
**
**		Paths:	"a.b.c"		- settings of nested groups ('/' is taken as '.' too)
**			"ports.[2]"	- element 2 (from 0) of a list or an array
**			"ports.[id=7]"	- the element of a list of groups whose setting ID is 7
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

#include	<stdint.h>
#include	"yalibcfgdef.h"

#ifdef	__cplusplus
extern "C" {
#endif

typedef	struct cfg_tree_t	CFG$TREE;		/* opaque					*/
typedef	struct cfg_node_t	CFG$NODE;		/* opaque					*/
typedef	struct cfg_diff_t	CFG$DIFF;		/* opaque					*/

/*
**  What CFG$NODE_INFO tells about a node
*/
typedef	struct cfg_info_t {
	const char	*name;				/* not zero-terminated; NULL for an element	*/
	uint32_t	namelen;
	uint32_t	type;				/* CFG$K_TYPE_*					*/
	uint32_t	count;				/* children of a group, list, array		*/
	const char	*file;				/* source file, zero-terminated			*/
	uint32_t	line,				/* position of the node in the file, from 1;	*/
			col;				/* 0 for a node made by the program		*/
} CFG$INFO;

unsigned	cfg$inimsg	(void);

unsigned	cfg$parse_file	(const char *a_path, uint32_t a_opts, CFG$TREE **a_tree);
unsigned	cfg$parse_buf	(const char *a_buf, uint32_t a_len, const char *a_name, uint32_t a_opts, CFG$TREE **a_tree);
unsigned	cfg$puterrs	(CFG$TREE *a_tree);
unsigned	cfg$free	(CFG$TREE *a_tree);
unsigned	cfg$source	(CFG$TREE *a_tree, uint32_t a_srcno, const char **a_path);

unsigned	cfg$root	(CFG$TREE *a_tree, CFG$NODE **a_node);
unsigned	cfg$lookup	(CFG$NODE *a_base, const char *a_path, CFG$NODE **a_node);
unsigned	cfg$node_info	(CFG$NODE *a_node, CFG$INFO *a_info);
unsigned	cfg$first	(CFG$NODE *a_parent, CFG$NODE **a_node);
unsigned	cfg$next	(CFG$NODE *a_node, CFG$NODE **a_next);

unsigned	cfg$get_int	(CFG$NODE *a_base, const char *a_path, int32_t *a_val);
unsigned	cfg$get_int64	(CFG$NODE *a_base, const char *a_path, int64_t *a_val);
unsigned	cfg$get_float	(CFG$NODE *a_base, const char *a_path, double *a_val);
unsigned	cfg$get_bool	(CFG$NODE *a_base, const char *a_path, int *a_val);
unsigned	cfg$get_string	(CFG$NODE *a_base, const char *a_path, const char **a_str, uint32_t *a_len);

unsigned	cfg$set_int	(CFG$NODE *a_base, const char *a_path, int32_t a_val);
unsigned	cfg$set_int64	(CFG$NODE *a_base, const char *a_path, int64_t a_val);
unsigned	cfg$set_float	(CFG$NODE *a_base, const char *a_path, double a_val);
unsigned	cfg$set_bool	(CFG$NODE *a_base, const char *a_path, int a_val);
unsigned	cfg$set_string	(CFG$NODE *a_base, const char *a_path, const char *a_str, uint32_t a_len);
unsigned	cfg$set_value	(CFG$NODE *a_base, const char *a_path, const char *a_text, uint32_t a_len);

unsigned	cfg$add		(CFG$NODE *a_parent, const char *a_name, uint32_t a_type, CFG$NODE **a_node);
unsigned	cfg$delete	(CFG$NODE *a_node);

unsigned	cfg$write_buf	(CFG$TREE *a_tree, uint32_t a_srcno, char **a_buf, uint32_t *a_len);
unsigned	cfg$write	(CFG$TREE *a_tree);
unsigned	cfg$write_file	(CFG$TREE *a_tree, const char *a_path);
unsigned	cfg$write_canonical(CFG$TREE *a_tree, char **a_buf, uint32_t *a_len);
unsigned	cfg$node_text	(CFG$NODE *a_node, uint32_t a_opts, char **a_buf, uint32_t *a_len);

/*
**  The schema: a table of entries, one per setting of a group, ended by an entry with a NULL
**  name.  The name "*" stands for any setting the table does not name.  A list or an array
**  has one entry for its elements in SUB (its name is not used).  See sections 3.7 and 4.6 of
**  the manual.
*/
typedef	struct cfg_keyword_t {
	const	char	*name;				/* NULL ends the table				*/
	int64_t		value;				/* for the program; not checked			*/
} CFG$KEYWORD;

typedef	unsigned (*cfg_check_t) (CFG$NODE *a_node, void *a_arg, char *a_msg, uint32_t a_msgsz, uint32_t *a_msglen);

typedef	struct cfg_schema_t {
	const	char	*name;				/* setting name; "*" - any other; NULL - end	*/
	uint32_t	type;				/* CFG$K_TYPE_*					*/
	uint32_t	flags;				/* CFG$M_REQUIRED, _KEY, _RANGE, ...		*/
	int64_t		min,				/* integer range, string length, count		*/
			max;
	double		fmin,				/* float range					*/
			fmax;
	const	CFG$KEYWORD *kwd;			/* CFG$K_TYPE_KWD: the allowed words		*/
	const	struct cfg_schema_t *sub;		/* a group: its settings; a list, an array:	*/
							/* one entry for the elements			*/
	cfg_check_t	check;				/* a routine of the program, or NULL		*/
	const	char	*help;				/* what the setting is for			*/
} CFG$SCHEMA;

unsigned	cfg$validate	(CFG$TREE *a_tree, const CFG$SCHEMA *a_schema, void *a_arg, uint32_t *a_nerr);

/*
**  Comparison of two trees, the fingerprint of a tree
*/
unsigned	cfg$fingerprint	(CFG$TREE *a_tree, uint8_t a_uuid[16]);
unsigned	cfg$diff	(CFG$TREE *a_old, CFG$TREE *a_new, const CFG$SCHEMA *a_schema, uint32_t a_opts, CFG$DIFF **a_diff);
unsigned	cfg$diff_count	(CFG$DIFF *a_diff, uint32_t *a_count);
unsigned	cfg$diff_entry	(CFG$DIFF *a_diff, uint32_t a_idx, uint32_t *a_op, const char **a_path, CFG$NODE **a_old, CFG$NODE **a_new);
unsigned	cfg$diff_listing(CFG$DIFF *a_diff, uint32_t a_opts, char **a_buf, uint32_t *a_len);
unsigned	cfg$diff_free	(CFG$DIFF *a_diff);

#ifdef	__cplusplus
}
#endif

#endif	/* __YALIBCFG$H__ */
