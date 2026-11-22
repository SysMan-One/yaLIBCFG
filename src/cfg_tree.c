#define	__MODULE__	"CFG_TREE"
#define	__IDENT__	"X.01-01"
#define	__REV__		"1.1.0"
#define	__FAC__		"CFG"

/*
**++
**
**  FACILITY:	yaLIBCFG - configuration trees in the libconfig syntax
**
**  MODULE:	cfg_tree.c
**
**  ABSTRACT:	The tree: the arena, the nodes, the paths, reading and changing values,
**		adding and removing nodes.
**
**  DESCRIPTION: All the nodes and strings of a tree live in its arena and go away with the tree
**		(CFG$FREE).  A changed node is marked, and so are all the groups above it
**		(CFG$M_DIRTY) - the writer copies an unmarked node as its source text.  A removed
**		node that came from a file stays as a tombstone; one the program made is unlinked.
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

#define	_GNU_SOURCE
#include	<stdlib.h>
#include	<string.h>
#include	<strings.h>
#include	<math.h>

#include	"cfg_int.h"

#define	$ISLIVE(n)	( !((n)->flags & CFG$M_DELETED) && ((n)->type != CFG$K_TYPE_INCLUDE) )
#define	$ISSCALAR(t)	( ((t) >= CFG$K_TYPE_INT) && ((t) <= CFG$K_TYPE_STRING) )
#define	$ISINTEGER(t)	( ((t) == CFG$K_TYPE_INT) || ((t) == CFG$K_TYPE_INT64) )



/*
**  DESCRIPTION: Take memory from the arena of the tree, zeroed, aligned to 8.
**
**  RETURNS:
**	the address, NULL - no memory
*/
void *	cfg$$alloc	(
		CFG$TREE	*a_tree,
		size_t		a_size
			)
{
CFG$ARENA	*l_ar = a_tree->arena;
size_t		l_size = (a_size + 7) & ~(size_t) 7, l_chunk;
void		*l_ptr;

	if ( !l_ar || (l_ar->used + l_size > l_ar->size) )
		{
		l_chunk = (l_size > CFG$K_ARENA) ? l_size : CFG$K_ARENA;

		if ( !(l_ar = malloc(sizeof(CFG$ARENA) + l_chunk)) )
			return	NULL;

		l_ar->size = l_chunk;
		l_ar->used = 0;
		l_ar->next = a_tree->arena;
		a_tree->arena = l_ar;
		}

	l_ptr = l_ar->data + l_ar->used;
	l_ar->used += l_size;
	memset(l_ptr, 0, l_size);

	return	l_ptr;
}



/*
**  DESCRIPTION: Copy a string into the arena, zero-terminated.
*/
char *	cfg$$strdup	(
		CFG$TREE	*a_tree,
	const	char		*a_str,
		size_t		a_len
			)
{
char	*l_ptr;

	if ( !(l_ptr = cfg$$alloc(a_tree, a_len + 1)) )
		return	NULL;

	if ( a_len )
		memcpy(l_ptr, a_str, a_len);

	return	l_ptr;
}



/*
**  DESCRIPTION: Make a node and append it to the children of a parent.
**
**  INPUTS:
**	a_tree:		the tree
**	a_parent:	the parent, NULL for the root
**	a_type:		CFG$K_TYPE_*
**
**  RETURNS:
**	the node, NULL - no memory
*/
CFG$NODE *	cfg$$node	(
		CFG$TREE	*a_tree,
		CFG$NODE	*a_parent,
		uint32_t	a_type
			)
{
CFG$NODE	*l_node;

	if ( !(l_node = cfg$$alloc(a_tree, sizeof(CFG$NODE))) )
		return	NULL;

	l_node->tree = a_tree;
	l_node->type = (uint8_t) a_type;
	l_node->sep = CFG$K_NOSEP;

	if ( (l_node->parent = a_parent) )
		{
		if ( (l_node->prev = a_parent->last) )
			a_parent->last->next = l_node;
		else	a_parent->child = l_node;

		a_parent->last = l_node;

		if ( a_type != CFG$K_TYPE_INCLUDE )
			a_parent->count++;
		}

	return	l_node;
}



/*
**  DESCRIPTION: Mark a node and every node above it as changed below.
*/
void	cfg$$dirty	(
		CFG$NODE	*a_node
			)
{
	for ( ; a_node; a_node = a_node->parent )
		a_node->flags |= CFG$M_DIRTY;
}



/*
**  DESCRIPTION: Compare two names, regardless of case when the tree was parsed so.
**
**  RETURNS:
**	0 - equal
*/
int	cfg$$namecmp	(
		CFG$TREE	*a_tree,
	const	char		*a_a,
		uint32_t	a_alen,
	const	char		*a_b,
		uint32_t	a_blen
			)
{
	if ( a_alen != a_blen )
		return	1;

	return	(a_tree->opts & CFG$M_NOCASE) ? strncasecmp(a_a, a_b, a_alen) : memcmp(a_a, a_b, a_alen);
}



/*
**  DESCRIPTION: Find a live setting of a group by name.
**
**  RETURNS:
**	the node, NULL - none
*/
CFG$NODE *	cfg$$find	(
		CFG$NODE	*a_group,
	const	char		*a_name,
		uint32_t	a_len
			)
{
	for ( CFG$NODE *l_n = a_group->child; l_n; l_n = l_n->next )
		if ( $ISLIVE(l_n) && l_n->name && !cfg$$namecmp(a_group->tree, l_n->name, l_n->namelen, a_name, a_len) )
			return	l_n;

	return	NULL;
}



/*
**  DESCRIPTION: Tell whether a string is a valid setting name: [A-Za-z*][-A-Za-z0-9_*]*
**
**  RETURNS:
**	1 - valid, 0 - not
*/
int	cfg$$isname	(
	const	char		*a_name,
		uint32_t	a_len
			)
{
	if ( !a_len || (a_len > CFG$K_MAXNAME) )
		return	0;

	for ( uint32_t i = 0; i < a_len; i++ )
		{
		char	l_c = a_name[i];
		int	l_ok = ((l_c >= 'a') && (l_c <= 'z')) || ((l_c >= 'A') && (l_c <= 'Z')) || (l_c == '*');

		if ( i && !l_ok )
			l_ok = ((l_c >= '0') && (l_c <= '9')) || (l_c == '-') || (l_c == '_');

		if ( !l_ok )
			return	0;
		}

	return	1;
}



/*
**  DESCRIPTION: Free a tree: the arena, the sources.
**
**  INPUTS:
**	a_tree:	the tree; NULL - nothing to do
**
**  RETURNS:
**	STS$K_SUCCESS
*/
unsigned	cfg$free	(
		CFG$TREE	*a_tree
			)
{
CFG$ARENA	*l_ar, *l_next;

	if ( !a_tree )
		return	STS$K_SUCCESS;

	for ( l_ar = a_tree->arena; l_ar; l_ar = l_next )
		{
		l_next = l_ar->next;
		free(l_ar);
		}

	for ( uint32_t i = 0; i < a_tree->nsrc; i++ )
		{
		free(a_tree->src[i]->buf);
		free(a_tree->src[i]->path);
		free(a_tree->src[i]->real);
		free(a_tree->src[i]->comm);
		free(a_tree->src[i]);
		}

	free(a_tree);

	return	STS$K_SUCCESS;
}



/*
**  DESCRIPTION: The file name of a source of a tree: 0 - the main file, then the included ones in
**		the order they were met.
**
**  OUTPUTS:
**	a_path:	zero-terminated, owned by the tree
**
**  RETURNS:
**	STS$K_SUCCESS, CFG$_NOTFOUND - no such source, CFG$_BADARG
*/
unsigned	cfg$source	(
		CFG$TREE	*a_tree,
		uint32_t	a_srcno,
	const	char		**a_path
			)
{
	if ( !a_tree || !a_path )
		return	CFG$_BADARG;

	if ( a_srcno >= a_tree->nsrc )
		return	CFG$_NOTFOUND;

	*a_path = a_tree->src[a_srcno]->path;

	return	STS$K_SUCCESS;
}



/*
**  DESCRIPTION: The root group of a tree.
*/
unsigned	cfg$root	(
		CFG$TREE	*a_tree,
		CFG$NODE	**a_node
			)
{
	if ( !a_tree || !a_node )
		return	CFG$_BADARG;

	*a_node = a_tree->root;

	return	STS$K_SUCCESS;
}



/*
**  DESCRIPTION: Tell about a node.
**
**  OUTPUTS:
**	a_info:	name, type, count of live children, source file and position
*/
unsigned	cfg$node_info	(
		CFG$NODE	*a_node,
		CFG$INFO	*a_info
			)
{
	if ( !a_node || !a_info )
		return	CFG$_BADARG;

	a_info->name = a_node->name;
	a_info->namelen = a_node->namelen;
	a_info->type = a_node->type;
	a_info->count = a_node->count;
	a_info->file = (a_node->srcno < a_node->tree->nsrc) ? a_node->tree->src[a_node->srcno]->path : "";
	a_info->line = a_node->line;
	a_info->col = a_node->col;

	return	STS$K_SUCCESS;
}



/*
**  DESCRIPTION: The first live child of a node.
**
**  RETURNS:
**	STS$K_SUCCESS, CFG$_NOTFOUND - no children
*/
unsigned	cfg$first	(
		CFG$NODE	*a_parent,
		CFG$NODE	**a_node
			)
{
	if ( !a_parent || !a_node )
		return	CFG$_BADARG;

	for ( CFG$NODE *l_n = a_parent->child; l_n; l_n = l_n->next )
		if ( $ISLIVE(l_n) )
			{
			*a_node = l_n;
			return	STS$K_SUCCESS;
			}

	return	CFG$_NOTFOUND;
}



/*
**  DESCRIPTION: The next live sibling of a node.
**
**  RETURNS:
**	STS$K_SUCCESS, CFG$_NOTFOUND - it was the last
*/
unsigned	cfg$next	(
		CFG$NODE	*a_node,
		CFG$NODE	**a_next
			)
{
	if ( !a_node || !a_next )
		return	CFG$_BADARG;

	for ( CFG$NODE *l_n = a_node->next; l_n; l_n = l_n->next )
		if ( $ISLIVE(l_n) )
			{
			*a_next = l_n;
			return	STS$K_SUCCESS;
			}

	return	CFG$_NOTFOUND;
}



/*
**  DESCRIPTION: Does a scalar node hold the value written as text in a path ("[id=7]")?
**
**  RETURNS:
**	1 - it does, 0 - it does not
*/
static int	s_valeq	(
		CFG$NODE	*a_node,
	const	char		*a_val,
		uint32_t	a_len
			)
{
char	l_tmp[32], *l_endp;

	switch ( a_node->type )
		{
		case CFG$K_TYPE_STRING:
			if ( (a_len >= 2) && (a_val[0] == '"') && (a_val[a_len - 1] == '"') )
				a_val++, a_len -= 2;

			return	(a_node->v.s.len == a_len) && !memcmp(a_node->v.s.ptr, a_val, a_len);

		case CFG$K_TYPE_INT:
		case CFG$K_TYPE_INT64:
			if ( !a_len || (a_len >= sizeof(l_tmp)) )
				return	0;

			memcpy(l_tmp, a_val, a_len);
			l_tmp[a_len] = '\0';

			return	(strtoll(l_tmp, &l_endp, 0) == a_node->v.i) && (*l_endp == '\0');

		case CFG$K_TYPE_BOOL:
			if ( (a_len == 4) && !strncasecmp(a_val, "true", 4) )
				return	a_node->v.b;

			if ( (a_len == 5) && !strncasecmp(a_val, "false", 5) )
				return	!a_node->v.b;

			return	0;
		}

	return	0;
}



/*
**  DESCRIPTION: Take one step of a path from a node.
**
**  INPUTS:
**	a_node:		where the step starts
**	a_comp,a_len:	the component: a name, "[n]" or "[key=value]"
**
**  OUTPUTS:
**	a_next:		the node the step leads to
**
**  RETURNS:
**	STS$K_SUCCESS, CFG$_NOTFOUND, CFG$_BADPATH
*/
static unsigned	s_step	(
		CFG$NODE	*a_node,
	const	char		*a_comp,
		uint32_t	a_len,
		CFG$NODE	**a_next
			)
{
CFG$NODE	*l_n, *l_k;
const	char	*l_eq;
uint32_t	l_idx = 0;

	if ( a_comp[0] != '[' )
		{
		if ( a_node->type != CFG$K_TYPE_GROUP )
			return	CFG$_NOTFOUND;

		return	(*a_next = cfg$$find(a_node, a_comp, a_len)) ? STS$K_SUCCESS : CFG$_NOTFOUND;
		}

	if ( (a_len < 3) || (a_comp[a_len - 1] != ']') )
		return	CFG$_BADPATH;

	a_comp++;
	a_len -= 2;

	if ( (a_node->type != CFG$K_TYPE_LIST) && (a_node->type != CFG$K_TYPE_ARRAY) && (a_node->type != CFG$K_TYPE_GROUP) )
		return	CFG$_NOTFOUND;

	/* [key=value] - the element (a group) whose setting KEY holds VALUE */
	if ( (l_eq = memchr(a_comp, '=', a_len)) )
		{
		uint32_t	l_klen = (uint32_t) (l_eq - a_comp);

		if ( !cfg$$isname(a_comp, l_klen) )
			return	CFG$_BADPATH;

		for ( l_n = a_node->child; l_n; l_n = l_n->next )
			if ( $ISLIVE(l_n) && (l_n->type == CFG$K_TYPE_GROUP) && (l_k = cfg$$find(l_n, a_comp, l_klen))
				&& s_valeq(l_k, l_eq + 1, a_len - l_klen - 1) )
				{
				*a_next = l_n;
				return	STS$K_SUCCESS;
				}

		return	CFG$_NOTFOUND;
		}

	/* [n] - by position among the live children */
	for ( uint32_t i = 0; i < a_len; i++ )
		{
		if ( (a_comp[i] < '0') || (a_comp[i] > '9') || (l_idx > 100000000U) )
			return	CFG$_BADPATH;

		l_idx = l_idx * 10 + (uint32_t) (a_comp[i] - '0');
		}

	for ( l_n = a_node->child; l_n; l_n = l_n->next )
		if ( $ISLIVE(l_n) && !l_idx-- )
			{
			*a_next = l_n;
			return	STS$K_SUCCESS;
			}

	return	CFG$_NOTFOUND;
}



/*
**  DESCRIPTION: Length of the next component of a path ('.' and '/' separate; '[...]' is whole).
*/
static uint32_t	s_complen	(
	const	char		*a_path
			)
{
uint32_t	l_len = 0;

	if ( a_path[0] == '[' )
		{
		while ( a_path[l_len] && (a_path[l_len] != ']') )
			l_len++;

		return	a_path[l_len] ? l_len + 1 : l_len;
		}

	while ( a_path[l_len] && (a_path[l_len] != '.') && (a_path[l_len] != '/') && (a_path[l_len] != '[') )
		l_len++;

	return	l_len;
}



/*
**  DESCRIPTION: Find a node by a path from a base node.
**
**  INPUTS:
**	a_base:	where the path starts
**	a_path:	the path; NULL or "" - the base itself
**
**  OUTPUTS:
**	a_node:	the node
**
**  RETURNS:
**	STS$K_SUCCESS, CFG$_NOTFOUND, CFG$_BADPATH, CFG$_BADARG
*/
unsigned	cfg$lookup	(
		CFG$NODE	*a_base,
	const	char		*a_path,
		CFG$NODE	**a_node
			)
{
CFG$NODE	*l_n = a_base;
uint32_t	l_len;
unsigned	l_sts;

	if ( !a_base || !a_node )
		return	CFG$_BADARG;

	while ( a_path && *a_path )
		{
		if ( (*a_path == '.') || (*a_path == '/') )
			{
			a_path++;
			continue;
			}

		if ( !(l_len = s_complen(a_path)) )
			return	CFG$_BADPATH;

		if ( !(1 & (l_sts = s_step(l_n, a_path, l_len, &l_n))) )
			return	l_sts;

		a_path += l_len;
		}

	*a_node = l_n;

	return	STS$K_SUCCESS;
}



unsigned	cfg$get_int	(
		CFG$NODE	*a_base,
	const	char		*a_path,
		int32_t		*a_val
			)
{
CFG$NODE	*l_n;
unsigned	l_sts;

	if ( !a_val )
		return	CFG$_BADARG;

	if ( !(1 & (l_sts = cfg$lookup(a_base, a_path, &l_n))) )
		return	l_sts;

	if ( !$ISINTEGER(l_n->type) || (l_n->v.i < INT32_MIN) || (l_n->v.i > INT32_MAX) )
		return	CFG$_TYPE;

	*a_val = (int32_t) l_n->v.i;

	return	STS$K_SUCCESS;
}



unsigned	cfg$get_int64	(
		CFG$NODE	*a_base,
	const	char		*a_path,
		int64_t		*a_val
			)
{
CFG$NODE	*l_n;
unsigned	l_sts;

	if ( !a_val )
		return	CFG$_BADARG;

	if ( !(1 & (l_sts = cfg$lookup(a_base, a_path, &l_n))) )
		return	l_sts;

	if ( !$ISINTEGER(l_n->type) )
		return	CFG$_TYPE;

	*a_val = l_n->v.i;

	return	STS$K_SUCCESS;
}



unsigned	cfg$get_float	(
		CFG$NODE	*a_base,
	const	char		*a_path,
		double		*a_val
			)
{
CFG$NODE	*l_n;
unsigned	l_sts;

	if ( !a_val )
		return	CFG$_BADARG;

	if ( !(1 & (l_sts = cfg$lookup(a_base, a_path, &l_n))) )
		return	l_sts;

	if ( l_n->type == CFG$K_TYPE_FLOAT )
		*a_val = l_n->v.f;
	else if ( $ISINTEGER(l_n->type) )
		*a_val = (double) l_n->v.i;
	else	return	CFG$_TYPE;

	return	STS$K_SUCCESS;
}



unsigned	cfg$get_bool	(
		CFG$NODE	*a_base,
	const	char		*a_path,
		int		*a_val
			)
{
CFG$NODE	*l_n;
unsigned	l_sts;

	if ( !a_val )
		return	CFG$_BADARG;

	if ( !(1 & (l_sts = cfg$lookup(a_base, a_path, &l_n))) )
		return	l_sts;

	if ( l_n->type != CFG$K_TYPE_BOOL )
		return	CFG$_TYPE;

	*a_val = l_n->v.b;

	return	STS$K_SUCCESS;
}



/*
**  OUTPUTS:
**	a_str,a_len:	the decoded string, in the tree (zero-terminated, but may hold zeros)
*/
unsigned	cfg$get_string	(
		CFG$NODE	*a_base,
	const	char		*a_path,
	const	char		**a_str,
		uint32_t	*a_len
			)
{
CFG$NODE	*l_n;
unsigned	l_sts;

	if ( !a_str || !a_len )
		return	CFG$_BADARG;

	if ( !(1 & (l_sts = cfg$lookup(a_base, a_path, &l_n))) )
		return	l_sts;

	if ( l_n->type != CFG$K_TYPE_STRING )
		return	CFG$_TYPE;

	*a_str = l_n->v.s.ptr;
	*a_len = l_n->v.s.len;

	return	STS$K_SUCCESS;
}



/*
**  DESCRIPTION: Make a new node (no source text) at the end of a container; the caller has
**		checked the name and the type.
**
**  RETURNS:
**	STS$K_SUCCESS, CFG$_NOMEM
*/
static unsigned	s_newnode	(
		CFG$NODE	*a_parent,
	const	char		*a_name,
		uint32_t	a_nlen,
		uint32_t	a_type,
		CFG$NODE	**a_node
			)
{
CFG$NODE	*l_n;

	if ( !(l_n = cfg$$node(a_parent->tree, a_parent, a_type)) )
		return	CFG$_NOMEM;

	if ( a_name && !(l_n->name = cfg$$strdup(a_parent->tree, a_name, a_nlen)) )
		return	CFG$_NOMEM;

	l_n->namelen = (uint8_t) a_nlen;
	l_n->srcno = a_parent->srcno;
	l_n->flags |= CFG$M_NEW;

	if ( a_type == CFG$K_TYPE_STRING )
		l_n->v.s.ptr = "";

	cfg$$dirty(l_n);
	*a_node = l_n;

	return	STS$K_SUCCESS;
}



/*
**  DESCRIPTION: Find or make the scalar node a CFG$SET_* writes to.  Missing settings named on
**		the path are made as groups, the last one as a scalar of the type; an element
**		([n], [k=v]) is never made.
**
**  INPUTS:
**	a_base:	where the path starts
**	a_path:	the path
**	a_type:	scalar type to write
**
**  OUTPUTS:
**	a_node:	the node
**
**  RETURNS:
**	STS$K_SUCCESS, CFG$_NOTFOUND, CFG$_BADPATH, CFG$_TYPE, CFG$_NOMEM
*/
static unsigned	s_target	(
		CFG$NODE	*a_base,
	const	char		*a_path,
		uint32_t	a_type,
		CFG$NODE	**a_node
			)
{
CFG$NODE	*l_n = a_base, *l_next;
uint32_t	l_len;
unsigned	l_sts;

	if ( !a_base || !a_path )
		return	CFG$_BADARG;

	while ( *a_path )
		{
		if ( (*a_path == '.') || (*a_path == '/') )
			{
			a_path++;
			continue;
			}

		if ( !(l_len = s_complen(a_path)) )
			return	CFG$_BADPATH;

		l_sts = s_step(l_n, a_path, l_len, &l_next);

		if ( (l_sts == CFG$_NOTFOUND) && (a_path[0] != '[') && (l_n->type == CFG$K_TYPE_GROUP) )
			{
			int	l_last = !a_path[l_len];

			if ( !cfg$$isname(a_path, l_len) )
				return	CFG$_BADNAME;

			if ( cfg$$find(l_n, a_path, l_len) )
				return	CFG$_EXISTS;

			l_sts = s_newnode(l_n, a_path, l_len, l_last ? a_type : CFG$K_TYPE_GROUP, &l_next);
			}

		if ( !(1 & l_sts) )
			return	l_sts;

		l_n = l_next;
		a_path += l_len;
		}

	if ( (l_n->type != a_type) && !($ISINTEGER(l_n->type) && $ISINTEGER(a_type)) )
		return	CFG$_TYPE;

	/* An element of an array keeps the type of the array */
	if ( (l_n->parent && (l_n->parent->type == CFG$K_TYPE_ARRAY)) && (l_n->type != a_type) && !$ISINTEGER(a_type) )
		return	CFG$_TYPE;

	*a_node = l_n;

	return	STS$K_SUCCESS;
}



/*
**  DESCRIPTION: Mark a node whose value has been set.
*/
static void	s_touch	(
		CFG$NODE	*a_node
			)
{
	if ( !(a_node->flags & CFG$M_NEW) )
		a_node->flags |= CFG$M_MODIFIED;

	cfg$$dirty(a_node);
}



unsigned	cfg$set_int	(
		CFG$NODE	*a_base,
	const	char		*a_path,
		int32_t		a_val
			)
{
	return	cfg$set_int64(a_base, a_path, (int64_t) a_val);
}



/*
**  DESCRIPTION: Set an integer.  The node keeps INT while the value fits 32 bits and it was not
**		written with the L suffix, else becomes INT64.
*/
unsigned	cfg$set_int64	(
		CFG$NODE	*a_base,
	const	char		*a_path,
		int64_t		a_val
			)
{
CFG$NODE	*l_n;
unsigned	l_sts;
int		l_wide = (a_val < INT32_MIN) || (a_val > INT32_MAX);

	if ( !(1 & (l_sts = s_target(a_base, a_path, l_wide ? CFG$K_TYPE_INT64 : CFG$K_TYPE_INT, &l_n))) )
		return	l_sts;

	l_n->v.i = a_val;

	if ( l_wide )
		l_n->type = CFG$K_TYPE_INT64;

	s_touch(l_n);

	return	STS$K_SUCCESS;
}



unsigned	cfg$set_float	(
		CFG$NODE	*a_base,
	const	char		*a_path,
		double		a_val
			)
{
CFG$NODE	*l_n;
unsigned	l_sts;

	if ( !isfinite(a_val) )
		return	CFG$_BADARG;			/* libconfig has no text for inf and nan	*/

	if ( !(1 & (l_sts = s_target(a_base, a_path, CFG$K_TYPE_FLOAT, &l_n))) )
		return	l_sts;

	l_n->v.f = a_val;
	s_touch(l_n);

	return	STS$K_SUCCESS;
}



unsigned	cfg$set_bool	(
		CFG$NODE	*a_base,
	const	char		*a_path,
		int		a_val
			)
{
CFG$NODE	*l_n;
unsigned	l_sts;

	if ( !(1 & (l_sts = s_target(a_base, a_path, CFG$K_TYPE_BOOL, &l_n))) )
		return	l_sts;

	l_n->v.b = !!a_val;
	s_touch(l_n);

	return	STS$K_SUCCESS;
}



unsigned	cfg$set_string	(
		CFG$NODE	*a_base,
	const	char		*a_path,
	const	char		*a_str,
		uint32_t	a_len
			)
{
CFG$NODE	*l_n;
unsigned	l_sts;
char		*l_copy;

	if ( !a_str && a_len )
		return	CFG$_BADARG;

	if ( !(1 & (l_sts = s_target(a_base, a_path, CFG$K_TYPE_STRING, &l_n))) )
		return	l_sts;

	if ( !(l_copy = cfg$$strdup(l_n->tree, a_str, a_len)) )
		return	CFG$_NOMEM;

	l_n->v.s.ptr = l_copy;
	l_n->v.s.len = a_len;
	s_touch(l_n);

	return	STS$K_SUCCESS;
}



/*
**  DESCRIPTION: Add a node at the end of a group, a list or an array.
**
**  INPUTS:
**	a_parent:	the container
**	a_name:		zero-terminated name - required for a group, NULL for a list or an array
**	a_type:		CFG$K_TYPE_*; an array takes scalars of the type of its elements
**
**  OUTPUTS:
**	a_node:		the new node (an integer is 0, a string empty, a boolean false); may be NULL
**
**  RETURNS:
**	STS$K_SUCCESS, CFG$_EXISTS, CFG$_BADNAME, CFG$_TYPE, CFG$_BADARG, CFG$_NOMEM
*/
unsigned	cfg$add	(
		CFG$NODE	*a_parent,
	const	char		*a_name,
		uint32_t	a_type,
		CFG$NODE	**a_node
			)
{
CFG$NODE	*l_n, *l_first;
uint32_t	l_nlen = 0;
unsigned	l_sts;

	if ( !a_parent || (a_type < CFG$K_TYPE_GROUP) || (a_type > CFG$K_TYPE_STRING) )
		return	CFG$_BADARG;

	switch ( a_parent->type )
		{
		case CFG$K_TYPE_GROUP:
			if ( !a_name )
				return	CFG$_BADARG;

			l_nlen = (uint32_t) strnlen(a_name, CFG$K_MAXNAME + 1);

			if ( !cfg$$isname(a_name, l_nlen) )
				return	CFG$_BADNAME;

			if ( cfg$$find(a_parent, a_name, l_nlen) )
				return	CFG$_EXISTS;

			break;

		case CFG$K_TYPE_LIST:
			if ( a_name )
				return	CFG$_BADARG;

			break;

		case CFG$K_TYPE_ARRAY:
			if ( a_name || !$ISSCALAR(a_type) )
				return	CFG$_TYPE;

			if ( (1 & cfg$first(a_parent, &l_first)) && (l_first->type != a_type)
				&& !($ISINTEGER(l_first->type) && $ISINTEGER(a_type)) )
				return	CFG$_TYPE;

			break;

		default:
			return	CFG$_TYPE;
		}

	if ( !(1 & (l_sts = s_newnode(a_parent, a_name, l_nlen, a_type, &l_n))) )
		return	l_sts;

	if ( a_node )
		*a_node = l_n;

	return	STS$K_SUCCESS;
}



/*
**  DESCRIPTION: Remove a node with all below it.
**
**  RETURNS:
**	STS$K_SUCCESS, CFG$_BADARG - the root, an @include, a node removed already
*/
unsigned	cfg$delete	(
		CFG$NODE	*a_node
			)
{
CFG$NODE	*l_p;

	if ( !a_node || !(l_p = a_node->parent) || (a_node->type == CFG$K_TYPE_INCLUDE) || (a_node->flags & CFG$M_DELETED) )
		return	CFG$_BADARG;

	l_p->count--;
	cfg$$dirty(l_p);

	if ( a_node->flags & CFG$M_NEW )
		{
		/* No text behind it - unlink */
		if ( a_node->prev )
			a_node->prev->next = a_node->next;
		else	l_p->child = a_node->next;

		if ( a_node->next )
			a_node->next->prev = a_node->prev;
		else	l_p->last = a_node->prev;

		a_node->flags |= CFG$M_DELETED;
		return	STS$K_SUCCESS;
		}

	a_node->flags |= CFG$M_DELETED;

	return	STS$K_SUCCESS;
}
