#define	__MODULE__	"CFG_DIFF"
#define	__IDENT__	"X.01-01"
#define	__REV__		"1.1.0"
#define	__FAC__		"CFG"

/*
**++
**
**  FACILITY:	yaLIBCFG - configuration trees in the libconfig syntax
**
**  MODULE:	cfg_diff.c
**
**  ABSTRACT:	Comparison of two trees; the listing of the differences in the manner of the
**		OpenVMS DIFFERENCES utility; the fingerprint of a tree.
**
**  DESCRIPTION: The comparison is of the settings, not of the text: comments, layout, the order
**		of the settings in a group do not count.  The differences are found walking both
**		trees together:
**
**		  - in a group, settings are matched by name;
**		  - in a list, elements are matched by a KEY when there is one: the setting
**		    flagged CFG$M_KEY in the schema, when a schema is given; else ID (or NAME, or
**		    KEY) when every element of both lists is a group holding it as a scalar,
**		    unique in its list.  The path of such an element is "ports.[id=9]".  Otherwise - and
**		    always with CFG$M_POSITIONAL - by position, "list.[2]";
**		  - an array is a value: compared whole;
**		  - a node whose type differs (32 and 64-bit integers are one type) is a change of
**		    the whole node.
**
**		The FINGERPRINT of a tree is a UUID of version 5 (RFC 9562): SHA-1 over the
**		yaLIBCFG namespace and a canonical encoding of the settings - the settings of a
**		group in the order of their names, the elements of a list with a key in the
**		order of the keys - so the fingerprint, like the comparison (without
**		CFG$M_POSITIONAL), does not see comments, layout or the order of what the
**		comparison matches by name or key.  Two trees of the same settings have the same
**		fingerprint; a listing names the fingerprints of both trees, so a listing can be
**		told whose it is.
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

#include	<stdlib.h>
#include	<string.h>
#include	<strings.h>

#include	"cfg_int.h"

#define	$ISLIVE(n)	( !((n)->flags & CFG$M_DELETED) && ((n)->type != CFG$K_TYPE_INCLUDE) )
#define	$ISSCALAR(t)	( ((t) >= CFG$K_TYPE_INT) && ((t) <= CFG$K_TYPE_STRING) )
#define	$ISINTEGER(t)	( ((t) == CFG$K_TYPE_INT) || ((t) == CFG$K_TYPE_INT64) )

/*
**  The namespace of the fingerprints: 6b2b0e1e-3f6a-5c4e-9d1a-0c5f9a7e2b14.  Never to be
**  changed - every fingerprint ever made would change with it.
*/
static	const uint8_t	s_ns[16] = { 0x6B, 0x2B, 0x0E, 0x1E, 0x3F, 0x6A, 0x5C, 0x4E, 0x9D, 0x1A, 0x0C, 0x5F, 0x9A, 0x7E, 0x2B, 0x14 };

static	const char	*s_keys[] = { "id", "name", "key" };	/* list keys, in this order	*/

typedef	struct cfg_dent_t {
	uint32_t	op;				/* CFG$K_DIFF_*					*/
	CFG$NODE	*old,
			*new;
	char		*path;				/* zero-terminated				*/
} CFG$DENT;

struct cfg_diff_t {
	CFG$TREE	*old,
			*new;
	uint32_t	opts,
			count,
			max;
	int		schema;				/* a schema was given				*/
	CFG$DENT	*ent;
};

typedef	struct cfg_buf_t {
	char		*p;
	uint32_t	len,
			max;
	unsigned	sts;
} CFG$BUF;



/*
**  DESCRIPTION: Append octets to a growing buffer; a failure is remembered in STS.
*/
static void	s_put	(
		CFG$BUF		*a_b,
	const	char		*a_ptr,
		uint32_t	a_len
			)
{
char	*l_new;

	if ( !(1 & a_b->sts) )
		return;

	if ( a_b->len + a_len + 1 > a_b->max )
		{
		uint32_t	l_max = a_b->max ? a_b->max : 1024;

		while ( l_max < a_b->len + a_len + 1 )
			l_max *= 2;

		if ( !(l_new = realloc(a_b->p, l_max)) )
			{
			a_b->sts = CFG$_NOMEM;
			return;
			}

		a_b->p = l_new;
		a_b->max = l_max;
		}

	if ( a_len )
		memcpy(a_b->p + a_b->len, a_ptr, a_len);

	a_b->len += a_len;
	a_b->p[a_b->len] = '\0';
}

static void	s_puts	(
		CFG$BUF		*a_b,
	const	char		*a_str
			)
{
	s_put(a_b, a_str, (uint32_t) strlen(a_str));
}



/*
**  DESCRIPTION: Append a piece made by FAO from a control string and its parameters, written
**		plain (see $FAO); a failure of FAO is remembered in STS of the buffer.
*/
#define	$LISTFAO(b, ctl, ...)	do {								\
	char		l_line[1024];								\
	fao_desc_t	l_dsc = $FAO_DESC(l_line);						\
	unsigned short	l_flen = 0;								\
												\
	if ( 1 & $FAO(ctl, &l_flen, &l_dsc, __VA_ARGS__) )					\
		s_put(b, l_line, l_flen);							\
	else	(b)->sts = CFG$_BADARG;								\
	} while ( 0 )



/*
**  DESCRIPTION: Are two scalars equal?  Integers of both sizes compare by value.
*/
static int	s_scalar_eq	(
		CFG$NODE	*a_a,
		CFG$NODE	*a_b
			)
{
	if ( $ISINTEGER(a_a->type) && $ISINTEGER(a_b->type) )
		return	a_a->v.i == a_b->v.i;

	if ( a_a->type != a_b->type )
		return	0;

	switch ( a_a->type )
		{
		case CFG$K_TYPE_FLOAT:	return	!memcmp(&a_a->v.f, &a_b->v.f, sizeof(double));
		case CFG$K_TYPE_BOOL:	return	!a_a->v.b == !a_b->v.b;
		case CFG$K_TYPE_STRING:	return	(a_a->v.s.len == a_b->v.s.len) && !memcmp(a_a->v.s.ptr, a_b->v.s.ptr, a_a->v.s.len);
		}

	return	0;
}



/*
**  DESCRIPTION: Are two values equal, all the way down?  (Order counts in lists and arrays.)
*/
static int	s_equal	(
		CFG$NODE	*a_a,
		CFG$NODE	*a_b
			)
{
CFG$NODE	*l_x, *l_y;

	if ( $ISSCALAR(a_a->type) || $ISSCALAR(a_b->type) )
		return	$ISSCALAR(a_a->type) && $ISSCALAR(a_b->type) && s_scalar_eq(a_a, a_b);

	if ( (a_a->type != a_b->type) || (a_a->count != a_b->count) )
		return	0;

	if ( a_a->type == CFG$K_TYPE_GROUP )
		{
		for ( l_x = a_a->child; l_x; l_x = l_x->next )
			if ( $ISLIVE(l_x) && (!(l_y = cfg$$find(a_b, l_x->name, l_x->namelen)) || !s_equal(l_x, l_y)) )
				return	0;

		return	1;
		}

	for ( l_x = a_a->child, l_y = a_b->child; ; l_x = l_x->next, l_y = l_y->next )
		{
		while ( l_x && !$ISLIVE(l_x) )
			l_x = l_x->next;

		while ( l_y && !$ISLIVE(l_y) )
			l_y = l_y->next;

		if ( !l_x || !l_y )
			return	!l_x && !l_y;

		if ( !s_equal(l_x, l_y) )
			return	0;
		}
}



/*
**  DESCRIPTION: Record one difference.
*/
static unsigned	s_record	(
		CFG$DIFF	*a_d,
		uint32_t	a_op,
		CFG$NODE	*a_old,
		CFG$NODE	*a_new,
	const	char		*a_path,
		uint32_t	a_plen
			)
{
CFG$DENT	*l_new;

	if ( a_d->count == a_d->max )
		{
		uint32_t	l_max = a_d->max ? a_d->max * 2 : 32;

		if ( !(l_new = realloc(a_d->ent, l_max * sizeof(CFG$DENT))) )
			return	CFG$_NOMEM;

		a_d->ent = l_new;
		a_d->max = l_max;
		}

	l_new = &a_d->ent[a_d->count];

	if ( !(l_new->path = malloc((size_t) a_plen + 1)) )
		return	CFG$_NOMEM;

	memcpy(l_new->path, a_path, a_plen);
	l_new->path[a_plen] = '\0';
	l_new->op = a_op;
	l_new->old = a_old;
	l_new->new = a_new;
	a_d->count++;

	return	STS$K_SUCCESS;
}



/*
**  DESCRIPTION: Does every live element of both lists hold the key (a scalar) the schema names?
**
**  RETURNS:
**	the key, NULL - match by position
*/
static const char *	s_haskey	(
		CFG$NODE	*a_old,
		CFG$NODE	*a_new,
	const	char		*a_key
			)
{
CFG$NODE	*l_lists[2] = { a_old, a_new }, *l_k, *l_j;
uint32_t	l_klen;

	if ( !a_key )
		return	NULL;

	l_klen = (uint32_t) strlen(a_key);

	/* Every element holds it as a scalar, and no two in a list hold the same - else by position */
	for ( int l = 0; l < 2; l++ )
		for ( CFG$NODE *l_e = l_lists[l]->child; l_e; l_e = l_e->next )
			{
			if ( !$ISLIVE(l_e) )
				continue;

			if ( (l_e->type != CFG$K_TYPE_GROUP) || !(l_k = cfg$$find(l_e, a_key, l_klen)) || !$ISSCALAR(l_k->type) )
				return	NULL;

			for ( CFG$NODE *l_f = l_e->next; l_f; l_f = l_f->next )
				if ( $ISLIVE(l_f) && (l_f->type == CFG$K_TYPE_GROUP) && (l_j = cfg$$find(l_f, a_key, l_klen)) && s_scalar_eq(l_k, l_j) )
					return	NULL;
			}

	return	a_key;
}



/*
**  DESCRIPTION: The key of a list, when it has one: the first of ID, NAME, KEY that every live
**		element of both lists holds as a scalar, unique within its list.
**
**  RETURNS:
**	the key name, NULL - match by position
*/
static const char *	s_listkey	(
		CFG$NODE	*a_old,
		CFG$NODE	*a_new
			)
{
CFG$NODE	*l_lists[2] = { a_old, a_new }, *l_e, *l_f, *l_k, *l_j;

	for ( uint32_t k = 0; k < $ARRSZ(s_keys); k++ )
		{
		uint32_t	l_klen = (uint32_t) strlen(s_keys[k]);
		int		l_ok = 1;

		for ( int l = 0; l < 2 && l_ok; l++ )
			for ( l_e = l_lists[l]->child; l_e && l_ok; l_e = l_e->next )
				{
				if ( !$ISLIVE(l_e) )
					continue;

				if ( (l_e->type != CFG$K_TYPE_GROUP) || !(l_k = cfg$$find(l_e, s_keys[k], l_klen)) || !$ISSCALAR(l_k->type) )
					{
					l_ok = 0;
					break;
					}

				/* Unique in its list */
				for ( l_f = l_e->next; l_f; l_f = l_f->next )
					if ( $ISLIVE(l_f) && (l_f->type == CFG$K_TYPE_GROUP) && (l_j = cfg$$find(l_f, s_keys[k], l_klen)) && s_scalar_eq(l_k, l_j) )
						{
						l_ok = 0;
						break;
						}
				}

		if ( l_ok )
			return	s_keys[k];
		}

	return	NULL;
}



static unsigned	s_node	(CFG$DIFF *a_d, CFG$NODE *a_old, CFG$NODE *a_new, CFG$BUF *a_path, const CFG$SCHEMA *a_ent);



/*
**  DESCRIPTION: Add a step to the path, run a comparison under it, take the step back.
*/
static unsigned	s_under	(
		CFG$DIFF	*a_d,
		CFG$BUF		*a_path,
	const	char		*a_step,
		uint32_t	a_slen,
		uint32_t	a_op,
		CFG$NODE	*a_old,
		CFG$NODE	*a_new,
	const	CFG$SCHEMA	*a_ent
			)
{
uint32_t	l_save = a_path->len;
unsigned	l_sts;

	if ( a_path->len && (a_step[0] != '[') )
		s_puts(a_path, ".");
	else if ( a_path->len )
		s_puts(a_path, ".");

	s_put(a_path, a_step, a_slen);

	if ( !(1 & a_path->sts) )
		return	a_path->sts;

	l_sts = a_op ? s_record(a_d, a_op, a_old, a_new, a_path->p, a_path->len) : s_node(a_d, a_old, a_new, a_path, a_ent);

	a_path->len = l_save;
	a_path->p[l_save] = '\0';

	return	l_sts;
}



/*
**  DESCRIPTION: The path step of an element of a list: "[key=value]" or "[n]".
*/
static unsigned	s_step	(
		CFG$NODE	*a_elem,
	const	char		*a_key,
		uint32_t	a_idx,
		CFG$BUF		*a_out
			)
{
char		*l_txt, l_num[12];
uint32_t	l_len, l_n = 0;
CFG$NODE	*l_k;
unsigned	l_sts;

	a_out->len = 0;
	s_puts(a_out, "[");

	if ( a_key && (l_k = cfg$$find(a_elem, a_key, (uint32_t) strlen(a_key))) )
		{
		if ( !(1 & (l_sts = cfg$$text_flat(l_k, 0, &l_txt, &l_len))) )
			return	l_sts;

		s_puts(a_out, a_key);
		s_puts(a_out, "=");
		s_put(a_out, l_txt, l_len);
		free(l_txt);
		}
	else	{
		do	{
			l_num[sizeof(l_num) - 1 - l_n++] = (char) ('0' + a_idx % 10);
			a_idx /= 10;
			} while ( a_idx );

		s_put(a_out, l_num + sizeof(l_num) - l_n, l_n);
		}

	s_puts(a_out, "]");

	return	a_out->sts;
}



/*
**  DESCRIPTION: Compare two lists.
*/
static unsigned	s_list	(
		CFG$DIFF	*a_d,
		CFG$NODE	*a_old,
		CFG$NODE	*a_new,
		CFG$BUF		*a_path,
	const	CFG$SCHEMA	*a_ent
			)
{
const	CFG$SCHEMA	*l_elem = (a_ent && (a_ent->type == CFG$K_TYPE_LIST) && a_ent->sub) ? a_ent->sub : NULL;
const	char	*l_skey = cfg$$schema_key(l_elem);
const	char	*l_key = (a_d->opts & CFG$M_POSITIONAL) ? NULL : l_skey ? s_haskey(a_old, a_new, l_skey) : s_listkey(a_old, a_new);
CFG$NODE	*l_x, *l_y, *l_k, *l_j;
CFG$BUF		l_step = { NULL, 0, 0, STS$K_SUCCESS };
unsigned	l_sts = STS$K_SUCCESS;
uint32_t	l_i = 0, l_klen = l_key ? (uint32_t) strlen(l_key) : 0;

	if ( l_key )
		{
		/* By key: the old elements in their order, then the new ones not matched */
		for ( l_x = a_old->child; l_x && (1 & l_sts); l_x = l_x->next )
			{
			if ( !$ISLIVE(l_x) )
				continue;

			l_k = cfg$$find(l_x, l_key, l_klen);

			for ( l_y = a_new->child; l_y; l_y = l_y->next )
				if ( $ISLIVE(l_y) && (l_j = cfg$$find(l_y, l_key, l_klen)) && s_scalar_eq(l_k, l_j) )
					break;

			if ( 1 & (l_sts = s_step(l_x, l_key, 0, &l_step)) )
				l_sts = s_under(a_d, a_path, l_step.p, l_step.len, l_y ? 0 : CFG$K_DIFF_DEL, l_x, l_y, l_elem);
			}

		for ( l_y = a_new->child; l_y && (1 & l_sts); l_y = l_y->next )
			{
			if ( !$ISLIVE(l_y) )
				continue;

			l_j = cfg$$find(l_y, l_key, l_klen);

			for ( l_x = a_old->child; l_x; l_x = l_x->next )
				if ( $ISLIVE(l_x) && (l_k = cfg$$find(l_x, l_key, l_klen)) && s_scalar_eq(l_k, l_j) )
					break;

			if ( !l_x && (1 & (l_sts = s_step(l_y, l_key, 0, &l_step))) )
				l_sts = s_under(a_d, a_path, l_step.p, l_step.len, CFG$K_DIFF_ADD, NULL, l_y, l_elem);
			}
		}
	else	{
		/* By position */
		for ( l_x = a_old->child, l_y = a_new->child; (1 & l_sts); l_i++ )
			{
			while ( l_x && !$ISLIVE(l_x) )
				l_x = l_x->next;

			while ( l_y && !$ISLIVE(l_y) )
				l_y = l_y->next;

			if ( !l_x && !l_y )
				break;

			if ( 1 & (l_sts = s_step(l_x ? l_x : l_y, NULL, l_i, &l_step)) )
				l_sts = s_under(a_d, a_path, l_step.p, l_step.len, !l_y ? CFG$K_DIFF_DEL : !l_x ? CFG$K_DIFF_ADD : 0, l_x, l_y, l_elem);

			l_x = l_x ? l_x->next : NULL;
			l_y = l_y ? l_y->next : NULL;
			}
		}

	free(l_step.p);

	return	l_sts;
}



/*
**  DESCRIPTION: Compare two nodes found at the same path.
*/
static unsigned	s_node	(
		CFG$DIFF	*a_d,
		CFG$NODE	*a_old,
		CFG$NODE	*a_new,
		CFG$BUF		*a_path,
	const	CFG$SCHEMA	*a_ent
			)
{
CFG$NODE	*l_x, *l_y;
unsigned	l_sts = STS$K_SUCCESS;
int		l_same = (a_old->type == a_new->type) || ($ISINTEGER(a_old->type) && $ISINTEGER(a_new->type));

	/* Scalars, arrays, a change of type: the node as a whole */
	if ( !l_same || $ISSCALAR(a_old->type) || (a_old->type == CFG$K_TYPE_ARRAY) )
		return	s_equal(a_old, a_new) ? STS$K_SUCCESS : s_record(a_d, CFG$K_DIFF_MOD, a_old, a_new, a_path->p, a_path->len);

	if ( a_old->type == CFG$K_TYPE_LIST )
		return	s_list(a_d, a_old, a_new, a_path, a_ent);

	/* A group: by name - the old settings in their order, then the new ones */
	for ( l_x = a_old->child; l_x && (1 & l_sts); l_x = l_x->next )
		{
		if ( !$ISLIVE(l_x) )
			continue;

		l_y = cfg$$find(a_new, l_x->name, l_x->namelen);
		l_sts = s_under(a_d, a_path, l_x->name, l_x->namelen, l_y ? 0 : CFG$K_DIFF_DEL, l_x, l_y,
			(a_ent && (a_ent->type == CFG$K_TYPE_GROUP)) ? cfg$$schema_entry(a_ent->sub, a_old->tree, l_x->name, l_x->namelen) : NULL);
		}

	for ( l_y = a_new->child; l_y && (1 & l_sts); l_y = l_y->next )
		if ( $ISLIVE(l_y) && !cfg$$find(a_old, l_y->name, l_y->namelen) )
			l_sts = s_under(a_d, a_path, l_y->name, l_y->namelen, CFG$K_DIFF_ADD, NULL, l_y, NULL);

	return	l_sts;
}



/*
**  DESCRIPTION: Compare two trees.
**
**  INPUTS:
**	a_old,a_new:	the trees
**	a_schema:	NULL, or the schema of the trees: a list whose elements have a setting
**			flagged CFG$M_KEY is matched by that key (else by ID, NAME, KEY as
**			without a schema)
**	a_opts:		0 or CFG$M_POSITIONAL
**
**  OUTPUTS:
**	a_diff:		the differences - free with CFG$DIFF_FREE; the trees must live as long
**
**  RETURNS:
**	STS$K_SUCCESS (whether there are differences or not - see CFG$DIFF_COUNT), CFG$_NOMEM,
**	CFG$_BADARG
*/
unsigned	cfg$diff	(
		CFG$TREE	*a_old,
		CFG$TREE	*a_new,
	const	CFG$SCHEMA	*a_schema,
		uint32_t	a_opts,
		CFG$DIFF	**a_diff
			)
{
CFG$DIFF	*l_d;
CFG$BUF		l_path = { NULL, 0, 0, STS$K_SUCCESS };
CFG$SCHEMA	l_root;
unsigned	l_sts;

	memset(&l_root, 0, sizeof(l_root));

	if ( !a_old || !a_new || !a_diff )
		return	CFG$_BADARG;

	if ( a_old->broken || a_new->broken )
		return	CFG$_BROKEN;

	if ( !(l_d = calloc(1, sizeof(CFG$DIFF))) )
		return	CFG$_NOMEM;

	l_d->old = a_old;
	l_d->new = a_new;
	l_d->opts = a_opts;
	l_d->schema = (a_schema != NULL);

	s_puts(&l_path, "");

	l_root.type = CFG$K_TYPE_GROUP;
	l_root.sub = a_schema;

	if ( !(1 & (l_sts = l_path.sts)) || !(1 & (l_sts = s_node(l_d, a_old->root, a_new->root, &l_path, a_schema ? &l_root : NULL))) )
		{
		free(l_path.p);
		cfg$diff_free(l_d);
		return	l_sts;
		}

	free(l_path.p);
	*a_diff = l_d;

	return	STS$K_SUCCESS;
}



unsigned	cfg$diff_count	(
		CFG$DIFF	*a_diff,
		uint32_t	*a_count
			)
{
	if ( !a_diff || !a_count )
		return	CFG$_BADARG;

	*a_count = a_diff->count;

	return	STS$K_SUCCESS;
}



/*
**  DESCRIPTION: One difference.
**
**  OUTPUTS:
**	a_op:		CFG$K_DIFF_MOD, _ADD, _DEL
**	a_path:		its path, zero-terminated, owned by the comparison
**	a_old,a_new:	the nodes; NULL on the side it is missing from
**
**  RETURNS:
**	STS$K_SUCCESS, CFG$_NOTFOUND - no such entry
*/
unsigned	cfg$diff_entry	(
		CFG$DIFF	*a_diff,
		uint32_t	a_idx,
		uint32_t	*a_op,
	const	char		**a_path,
		CFG$NODE	**a_old,
		CFG$NODE	**a_new
			)
{
	if ( !a_diff )
		return	CFG$_BADARG;

	if ( a_idx >= a_diff->count )
		return	CFG$_NOTFOUND;

	if ( a_op )
		*a_op = a_diff->ent[a_idx].op;

	if ( a_path )
		*a_path = a_diff->ent[a_idx].path;

	if ( a_old )
		*a_old = a_diff->ent[a_idx].old;

	if ( a_new )
		*a_new = a_diff->ent[a_idx].new;

	return	STS$K_SUCCESS;
}



unsigned	cfg$diff_free	(
		CFG$DIFF	*a_diff
			)
{
	if ( !a_diff )
		return	STS$K_SUCCESS;

	for ( uint32_t i = 0; i < a_diff->count; i++ )
		free(a_diff->ent[i].path);

	free(a_diff->ent);
	free(a_diff);

	return	STS$K_SUCCESS;
}



/*
**  DESCRIPTION: The key of one list by itself - S_LISTKEY of the list with itself.
*/
static const char *	s_selfkey	(
		CFG$NODE	*a_list
			)
{
	return	s_listkey(a_list, a_list);
}



/*
**  DESCRIPTION: Order two scalars for the fingerprint: by type, then by value (strings bytewise,
**		shorter first; numbers by their bits as unsigned).
**
**  RETURNS:
**	<0, 0, >0
*/
static int	s_keycmp	(
		CFG$NODE	*a_a,
		CFG$NODE	*a_b
			)
{
int		l_ta = $ISINTEGER(a_a->type) ? CFG$K_TYPE_INT : a_a->type, l_tb = $ISINTEGER(a_b->type) ? CFG$K_TYPE_INT : a_b->type, l_c;
uint64_t	l_ua = 0, l_ub = 0;
uint32_t	l_min;

	if ( l_ta != l_tb )
		return	l_ta - l_tb;

	if ( l_ta == CFG$K_TYPE_STRING )
		{
		l_min = (a_a->v.s.len < a_b->v.s.len) ? a_a->v.s.len : a_b->v.s.len;

		if ( (l_c = memcmp(a_a->v.s.ptr, a_b->v.s.ptr, l_min)) )
			return	l_c;

		return	(a_a->v.s.len > a_b->v.s.len) - (a_a->v.s.len < a_b->v.s.len);
		}

	if ( l_ta == CFG$K_TYPE_FLOAT )
		{
		memcpy(&l_ua, &a_a->v.f, 8);
		memcpy(&l_ub, &a_b->v.f, 8);
		}
	else if ( l_ta == CFG$K_TYPE_BOOL )
		{
		l_ua = !!a_a->v.b;
		l_ub = !!a_b->v.b;
		}
	else	{
		l_ua = (uint64_t) a_a->v.i;
		l_ub = (uint64_t) a_b->v.i;
		}

	return	(l_ua > l_ub) - (l_ua < l_ub);
}



/*
**  DESCRIPTION: The canonical encoding of a node into the hash of the fingerprint.
*/
static unsigned	s_fp	(
		CFG$SHA1	*a_ctx,
		CFG$NODE	*a_n
			)
{
uint8_t		l_hdr[9];
CFG$NODE	*l_c, **l_v = NULL;
const	char	*l_key;
uint32_t	l_cnt = 0;
unsigned	l_sts = STS$K_SUCCESS;
uint64_t	l_u = 0;

	switch ( a_n->type )
		{
		case CFG$K_TYPE_INT:
		case CFG$K_TYPE_INT64:	l_hdr[0] = 'I'; l_u = (uint64_t) a_n->v.i;	break;
		case CFG$K_TYPE_FLOAT:	l_hdr[0] = 'F'; memcpy(&l_u, &a_n->v.f, 8);	break;
		case CFG$K_TYPE_BOOL:	l_hdr[0] = 'B'; l_u = !!a_n->v.b;		break;
		case CFG$K_TYPE_STRING:	l_hdr[0] = 'S'; l_u = a_n->v.s.len;		break;
		case CFG$K_TYPE_GROUP:	l_hdr[0] = 'G'; l_u = a_n->count;		break;
		case CFG$K_TYPE_LIST:	l_hdr[0] = 'L'; l_u = a_n->count;		break;
		case CFG$K_TYPE_ARRAY:	l_hdr[0] = 'A'; l_u = a_n->count;		break;
		default:		return	STS$K_SUCCESS;
		}

	for ( int i = 0; i < 8; i++ )
		l_hdr[1 + i] = (uint8_t) (l_u >> (56 - i * 8));

	cfg$$sha1_update(a_ctx, l_hdr, sizeof(l_hdr));

	if ( a_n->type == CFG$K_TYPE_STRING )
		cfg$$sha1_update(a_ctx, a_n->v.s.ptr, a_n->v.s.len);

	if ( $ISSCALAR(a_n->type) || !a_n->count )
		return	STS$K_SUCCESS;

	/* The children; those of a group in the order of their names */
	if ( !(l_v = malloc(a_n->count * sizeof(CFG$NODE *))) )
		return	CFG$_NOMEM;

	for ( l_c = a_n->child; l_c; l_c = l_c->next )
		if ( $ISLIVE(l_c) && (l_cnt < a_n->count) )
			l_v[l_cnt++] = l_c;

	/*
	** A list with a key (as CFG$DIFF matches it) in the order of the keys: the comparison
	** does not see the order of such a list, nor must the fingerprint
	*/
	if ( (a_n->type == CFG$K_TYPE_LIST) && (l_key = s_selfkey(a_n)) )
		for ( uint32_t i = 1; i < l_cnt; i++ )
			{
			CFG$NODE	*l_t = l_v[i], *l_tk = cfg$$find(l_t, l_key, (uint32_t) strlen(l_key));
			uint32_t	j = i;

			for ( ; j && (s_keycmp(cfg$$find(l_v[j - 1], l_key, (uint32_t) strlen(l_key)), l_tk) > 0); j-- )
				l_v[j] = l_v[j - 1];

			l_v[j] = l_t;
			}

	if ( a_n->type == CFG$K_TYPE_GROUP )
		for ( uint32_t i = 1; i < l_cnt; i++ )
			{
			CFG$NODE	*l_t = l_v[i];
			uint32_t	j = i;
			int		l_nocase = (a_n->tree->opts & CFG$M_NOCASE) != 0;

			for ( ; j; j-- )
				{
				CFG$NODE	*l_p = l_v[j - 1];
				uint32_t	l_min = (l_p->namelen < l_t->namelen) ? l_p->namelen : l_t->namelen;
				int		l_cmp = l_nocase ? strncasecmp(l_p->name, l_t->name, l_min) : memcmp(l_p->name, l_t->name, l_min);

				if ( (l_cmp < 0) || (!l_cmp && (l_p->namelen <= l_t->namelen)) )
					break;

				l_v[j] = l_p;
				}

			l_v[j] = l_t;
			}

	for ( uint32_t i = 0; (i < l_cnt) && (1 & l_sts); i++ )
		{
		if ( a_n->type == CFG$K_TYPE_GROUP )
			{
			uint8_t	l_nlen = l_v[i]->namelen;
			char	l_name[CFG$K_MAXNAME];

			/* A tree that ignores the case of names hashes them in lower case */
			for ( uint32_t k = 0; k < l_nlen; k++ )
				l_name[k] = (a_n->tree->opts & CFG$M_NOCASE) && (l_v[i]->name[k] >= 'A') && (l_v[i]->name[k] <= 'Z')
					? (char) (l_v[i]->name[k] | 0x20) : l_v[i]->name[k];

			cfg$$sha1_update(a_ctx, &l_nlen, 1);
			cfg$$sha1_update(a_ctx, l_name, l_nlen);
			}

		l_sts = s_fp(a_ctx, l_v[i]);
		}

	free(l_v);

	return	l_sts;
}



/*
**  DESCRIPTION: The fingerprint of a tree: a UUID of version 5 over its settings.
**
**  OUTPUTS:
**	a_uuid:	16 octets, in the order of RFC 9562 (put it out with the FAO directive !%U)
**
**  RETURNS:
**	STS$K_SUCCESS, CFG$_NOMEM, CFG$_BADARG
*/
unsigned	cfg$fingerprint	(
		CFG$TREE	*a_tree,
		uint8_t		a_uuid[16]
			)
{
CFG$SHA1	l_ctx;
uint8_t		l_md[20];
unsigned	l_sts;

	if ( !a_tree || !a_uuid )
		return	CFG$_BADARG;

	if ( a_tree->broken )
		return	CFG$_BROKEN;

	cfg$$sha1_init(&l_ctx);
	cfg$$sha1_update(&l_ctx, s_ns, sizeof(s_ns));

	if ( !(1 & (l_sts = s_fp(&l_ctx, a_tree->root))) )
		return	l_sts;

	cfg$$sha1_final(&l_ctx, l_md);

	memcpy(a_uuid, l_md, 16);
	a_uuid[6] = (uint8_t) ((a_uuid[6] & 0x0F) | 0x50);	/* version 5				*/
	a_uuid[8] = (uint8_t) ((a_uuid[8] & 0x3F) | 0x80);	/* variant of RFC 9562			*/

	return	STS$K_SUCCESS;
}



/*
**  DESCRIPTION: The header of a tree in a listing: the file, its time, its fingerprint.
*/
static void	s_head	(
		CFG$BUF		*a_b,
	const	char		*a_label,
		CFG$TREE	*a_tree,
		uint32_t	a_opts
			)
{
uint8_t		l_uuid[16];

	/* A file name may be as long as PATH_MAX: put as it is, not through FAO */
	s_puts(a_b, a_label);
	s_puts(a_b, " ");
	s_puts(a_b, a_tree->src[0]->path);
	s_puts(a_b, "\n");

	if ( !(1 & cfg$fingerprint(a_tree, l_uuid)) )
		memset(l_uuid, 0, sizeof(l_uuid));

	if ( a_tree->src[0]->mtime && !(a_opts & CFG$M_NOTIME) )
		$LISTFAO(a_b, "     !%D  UUID !%U!/", a_tree->src[0]->mtime, l_uuid);
	else	$LISTFAO(a_b, "     UUID !%U!/", l_uuid);
}



/*
**  DESCRIPTION: One side of a difference section: the line in its file and the canonical text.
*/
static void	s_side	(
		CFG$BUF		*a_b,
		CFG$TREE	*a_tree,
	const	char		*a_path,
		CFG$NODE	*a_n
			)
{
char		*l_txt;
uint32_t	l_len;
unsigned	l_sts;
const	char	*l_file = a_tree->src[0]->path;

	if ( a_n && (a_n->srcno < a_tree->nsrc) )
		l_file = a_tree->src[a_n->srcno]->path;

	s_puts(a_b, "File ");
	s_puts(a_b, l_file);
	s_puts(a_b, "\n");

	if ( !a_n )
		{
		s_puts(a_b, "        (none)\n");
		return;
		}

	if ( a_n->line )
		$LISTFAO(a_b, "!6UL   ", a_n->line);
	else	s_puts(a_b, "         ");

	s_puts(a_b, a_path);
	s_puts(a_b, " = ");

	if ( !(1 & (l_sts = cfg$$text_flat(a_n, 0, &l_txt, &l_len))) )
		{
		a_b->sts = l_sts;
		return;
		}

	s_put(a_b, l_txt, l_len);
	free(l_txt);
	s_puts(a_b, ";\n");
}



/*
**  DESCRIPTION: The listing of a comparison, in the manner of the OpenVMS DIFFERENCES utility:
**
**		    CONFIGURATION DIFFERENCES                           22-NOV-2026 14:40:01.00
**
**		    Old: router.cfg
**		         22-NOV-2026 14:30:12.00  UUID 0e1d...-....
**		    New: router-new.cfg
**		         22-NOV-2026 14:39:55.00  UUID 7a44...-....
**
**		    ************
**		    File router.cfg
**		        15   ports.[id=9].mtu = 9216;
**		    ******
**		    File router-new.cfg
**		        15   ports.[id=9].mtu = 1500;
**		    ************
**
**		    Number of difference sections found: 1
**		       changed: 1, added: 0, removed: 0
**
**		    CONFIGURATION DIFFERENCES /KEYS=(ID,NAME,KEY)-
**		        router.cfg-
**		        router-new.cfg
**
**		The times are those of the last change of the files, UTC.
**
**  INPUTS:
**	a_diff:		the comparison
**	a_opts:		0 or CFG$M_NOTIME - no times in the listing: the same comparison gives
**			the same text on every run (for tests, for listings kept under git)
**
**  OUTPUTS:
**	a_buf,a_len:	the text, malloc()-ed, zero-terminated - free() it
**
**  RETURNS:
**	STS$K_SUCCESS, CFG$_NOMEM, CFG$_BADARG
*/
unsigned	cfg$diff_listing	(
		CFG$DIFF	*a_diff,
		uint32_t	a_opts,
		char		**a_buf,
		uint32_t	*a_len
			)
{
CFG$BUF		l_b = { NULL, 0, 0, STS$K_SUCCESS };
uint32_t	l_n[4] = { 0, 0, 0, 0 };

	if ( !a_diff || !a_buf || !a_len )
		return	CFG$_BADARG;

	if ( a_opts & CFG$M_NOTIME )
		s_puts(&l_b, "CONFIGURATION DIFFERENCES\n\n");
	else	$LISTFAO(&l_b, "CONFIGURATION DIFFERENCES!56<!>!%D!/!/", 0);	/* 0 - now		*/

	s_head(&l_b, "Old:", a_diff->old, a_opts);
	s_head(&l_b, "New:", a_diff->new, a_opts);
	s_puts(&l_b, "\n");

	for ( uint32_t i = 0; i < a_diff->count; i++ )
		{
		CFG$DENT	*l_e = &a_diff->ent[i];

		l_n[l_e->op]++;

		s_puts(&l_b, "************\n");
		s_side(&l_b, a_diff->old, l_e->path, l_e->old);
		s_puts(&l_b, "******\n");
		s_side(&l_b, a_diff->new, l_e->path, l_e->new);
		}

	if ( a_diff->count )
		s_puts(&l_b, "************\n");

	$LISTFAO(&l_b, "!/Number of difference sections found: !UL!/", a_diff->count);
	$LISTFAO(&l_b, "   changed: !UL, added: !UL, removed: !UL!/!/", l_n[CFG$K_DIFF_MOD], l_n[CFG$K_DIFF_ADD], l_n[CFG$K_DIFF_DEL]);

	s_puts(&l_b, (a_diff->opts & CFG$M_POSITIONAL) ? "CONFIGURATION DIFFERENCES /POSITIONAL-\n"
		: a_diff->schema ? "CONFIGURATION DIFFERENCES /SCHEMA-\n" : "CONFIGURATION DIFFERENCES /KEYS=(ID,NAME,KEY)-\n");

	s_puts(&l_b, "    ");
	s_puts(&l_b, a_diff->old->src[0]->path);
	s_puts(&l_b, "-\n    ");
	s_puts(&l_b, a_diff->new->src[0]->path);
	s_puts(&l_b, "\n");

	if ( !(1 & l_b.sts) )
		{
		free(l_b.p);
		return	l_b.sts;
		}

	*a_buf = l_b.p;
	*a_len = l_b.len;

	return	STS$K_SUCCESS;
}
