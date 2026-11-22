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

unsigned	cfg$add		(CFG$NODE *a_parent, const char *a_name, uint32_t a_type, CFG$NODE **a_node);
unsigned	cfg$delete	(CFG$NODE *a_node);

unsigned	cfg$write_buf	(CFG$TREE *a_tree, uint32_t a_srcno, char **a_buf, uint32_t *a_len);
unsigned	cfg$write	(CFG$TREE *a_tree);
unsigned	cfg$write_file	(CFG$TREE *a_tree, const char *a_path);

#ifdef	__cplusplus
}
#endif

#endif	/* __YALIBCFG$H__ */
