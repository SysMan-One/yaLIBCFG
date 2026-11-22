#define	__MODULE__	"CFG_SCHEMA"
#define	__IDENT__	"X.01-01"
#define	__REV__		"1.1.0"
#define	__FAC__		"CFG"

/*
**++
**
**  FACILITY:	yaLIBCFG - configuration trees in the libconfig syntax
**
**  MODULE:	cfg_schema.c
**
**  ABSTRACT:	Validation of a tree by a schema: types, ranges, lengths, keywords,
**		addresses, required and unknown settings, keys of lists, the program's own
**		checks.
**
**  DESCRIPTION: A schema is a table of the program (CFG$SCHEMA, see YALIBCFG.H), in the spirit
**		of the verb tables of CLI_ROUTINES: one entry per setting of a group, the
**		entries of a group's settings in SUB, one entry for the elements of a list or an
**		array in SUB[0].
**
**		The validation does not stop at the first error: every diagnostic is recorded in
**		the tree with its position (up to CFG$K_MAXERRS, the rest counted), so that one
**		run of CFG$PUTERRS tells the user everything that is wrong with the file.  A
**		node of a wrong type is reported once and not looked into.
**
**		Diagnostics name the setting by its path - with the key of a list where the
**		schema has one: "ports.[id=9].mtu = 99999, allowed 64..9216".
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
#include	<arpa/inet.h>

#include	"cfg_int.h"

#define	$ISLIVE(n)	( !((n)->flags & CFG$M_DELETED) && ((n)->type != CFG$K_TYPE_INCLUDE) )
#define	$ISINTEGER(t)	( ((t) == CFG$K_TYPE_INT) || ((t) == CFG$K_TYPE_INT64) )
#define	$ISSTRTYPE(t)	( ((t) == CFG$K_TYPE_STRING) || (((t) >= CFG$K_TYPE_KWD) && ((t) <= CFG$K_TYPE_MAC)) )
#define	$ISSCALARTYPE(t) ( (((t) >= CFG$K_TYPE_INT) && ((t) <= CFG$K_TYPE_STRING)) || $ISSTRTYPE(t) )

#define	CFG$K_MAXPATH		1024		/* longest path kept for a diagnostic		*/
#define	CFG$K_MAXSDEPTH		64		/* deepest schema (an entry seen before ends a path) */

typedef	struct cfg_val_t {
	CFG$TREE	*tree;
	void		*arg;
	uint32_t	nerrors,			/* severity ERROR or worse			*/
			ndiag;				/* all, kept or not				*/
	uint32_t	base;				/* NERRORS before the key of an element was	*/
	int		hasbase;			/* checked (S_SEQ) - see S_NODE			*/
	uint32_t	plen;
	char		path[CFG$K_MAXPATH + 1];
} CFG$VAL;

typedef	struct cfg_text_t {
	char		buf[CFG$K_MAXPATH];		/* a step of a path, a text; the record of a	*/
	uint32_t	len;				/* diagnostic keeps the first 160 octets	*/
} CFG$TEXT;



/*
**  DESCRIPTION: Append to the text of a diagnostic; what does not fit is cut.
*/
static void	s_add	(
		CFG$TEXT	*a_t,
	const	char		*a_str,
		uint32_t	a_len
			)
{
	if ( a_len > sizeof(a_t->buf) - a_t->len )
		a_len = (uint32_t) (sizeof(a_t->buf) - a_t->len);

	memcpy(a_t->buf + a_t->len, a_str, a_len);
	a_t->len += a_len;
}

static void	s_adds	(
		CFG$TEXT	*a_t,
	const	char		*a_str
			)
{
	s_add(a_t, a_str, (uint32_t) strlen(a_str));
}

static void	s_addnum	(
		CFG$TEXT	*a_t,
		int64_t		a_val
			)
{
char		l_num[32];
fao_desc_t	l_dsc = $FAO_DESC(l_num);
unsigned short	l_len = 0;

	$CFG_FAO("!SQ", &l_len, &l_dsc, a_val);
	s_add(a_t, l_num, l_len);
}

static void	s_addfloat	(
		CFG$TEXT	*a_t,
		double		a_val
			)
{
char		l_num[64];
uint32_t	l_len = 0;

	if ( 1 & cfg$$text_double(a_val, l_num, sizeof(l_num), &l_len) )
		s_add(a_t, l_num, l_len);
}

/*
**  The value of a node, as in a file
*/
static void	s_addvalue	(
		CFG$TEXT	*a_t,
		CFG$NODE	*a_n
			)
{
char		*l_txt;
uint32_t	l_len;

	if ( 1 & cfg$$text_flat(a_n, 0, &l_txt, &l_len) )
		{
		s_add(a_t, l_txt, l_len);
		free(l_txt);
		}
}



/*
**  DESCRIPTION: Record a diagnostic at a node: the path first, then the rest of the text.
**
**  INPUTS:
**	a_v:	the validation
**	a_sts:	condition value
**	a_n:	the node it is about (its position is taken)
**	a_t:	the text after the path, or NULL
*/
static void	s_diag	(
		CFG$VAL		*a_v,
		unsigned	a_sts,
		CFG$NODE	*a_n,
		CFG$TEXT	*a_t
			)
{
CFG$TEXT	l_all = { .len = 0 };
uint32_t	l_line = a_n->line, l_col = a_n->col;

	s_add(&l_all, a_v->plen ? a_v->path : "(the file)", a_v->plen ? a_v->plen : 10);

	if ( a_t )
		s_add(&l_all, a_t->buf, a_t->len);

	/* The root (or a node the program made) has no position: the beginning of its file */
	if ( !l_line )
		l_line = l_col = 1;

	cfg$$error(a_v->tree, a_sts, a_n->srcno, l_line, l_col, l_all.buf, l_all.len);

	a_v->ndiag++;

	if ( ($SEV(a_sts) == STS$K_ERROR) || ($SEV(a_sts) == STS$K_FATAL) )
		a_v->nerrors++;
}



/*
**  DESCRIPTION: Add a step to the path ("a", ".b", ".[2]", ".[id=9]"); give the old length back.
*/
static uint32_t	s_push	(
		CFG$VAL		*a_v,
	const	char		*a_step,
		uint32_t	a_len
			)
{
uint32_t	l_old = a_v->plen;

	if ( a_v->plen && (a_v->plen < CFG$K_MAXPATH) )
		a_v->path[a_v->plen++] = '.';

	if ( a_len > CFG$K_MAXPATH - a_v->plen )
		a_len = CFG$K_MAXPATH - a_v->plen;

	memcpy(a_v->path + a_v->plen, a_step, a_len);
	a_v->plen += a_len;
	a_v->path[a_v->plen] = '\0';

	return	l_old;
}

static void	s_pop	(
		CFG$VAL		*a_v,
		uint32_t	a_old
			)
{
	a_v->plen = a_old;
	a_v->path[a_old] = '\0';
}



/*
**  DESCRIPTION: The entry of a table for a setting name: by name (with regard to the case rule of
**		the tree), else the "*" entry, else none.
**
**  RETURNS:
**	the entry, NULL
*/
const CFG$SCHEMA *	cfg$$schema_entry	(
	const	CFG$SCHEMA	*a_table,
		CFG$TREE	*a_tree,
	const	char		*a_name,
		uint32_t	a_len
			)
{
const	CFG$SCHEMA	*l_any = NULL;

	for ( ; a_table && a_table->name; a_table++ )
		{
		if ( (a_table->name[0] == '*') && !a_table->name[1] )
			l_any = a_table;
		else if ( !cfg$$namecmp(a_tree, a_table->name, (uint32_t) strlen(a_table->name), a_name, a_len) )
			return	a_table;
		}

	return	l_any;
}



/*
**  DESCRIPTION: The key of the elements of a list by its element entry: the name of the setting
**		flagged CFG$M_KEY in a group element.
**
**  RETURNS:
**	the name, NULL - no key
*/
const char *	cfg$$schema_key	(
	const	CFG$SCHEMA	*a_elem
			)
{
	if ( !a_elem || (a_elem->type != CFG$K_TYPE_GROUP) )
		return	NULL;

	for ( const CFG$SCHEMA *l_e = a_elem->sub; l_e && l_e->name; l_e++ )
		if ( l_e->flags & CFG$M_KEY )
			return	l_e->name;

	return	NULL;
}



/*
**  DESCRIPTION: Is the schema itself sound?  Programmer's errors are found before any file is
**		blamed.
**
**  RETURNS:
**	STS$K_SUCCESS, CFG$_BADSCHEMA
*/
typedef	struct cfg_seen_t {
	const	CFG$SCHEMA	**ent;			/* entries checked already, with		*/
	uint8_t		*inlist;			/* whether as an element of a list		*/
	uint32_t	n,
			max;
	unsigned	sts;				/* CFG$_NOMEM once a growth failed		*/
} CFG$SEEN;

static unsigned	s_sound	(
	const	CFG$SCHEMA	*a_e,
		int		a_inlist,
		int		a_depth,
		CFG$SEEN	*a_seen
			)
{
int	l_keys = 0;

	if ( a_depth >= CFG$K_MAXSDEPTH )
		return	CFG$_BADSCHEMA;

	/*
	** Every entry is checked once (for each of the two roles - in a list or not, a key is
	** valid only in the first): a schema that refers to itself, a menu of menus, takes as
	** long as its table is big, not as long as the number of its paths.
	*/
	for ( uint32_t i = 0; i < a_seen->n; i++ )
		if ( (a_seen->ent[i] == a_e) && (a_seen->inlist[i] == (uint8_t) !!a_inlist) )
			return	STS$K_SUCCESS;

	if ( a_seen->n == a_seen->max )
		{
		uint32_t	l_max = a_seen->max ? a_seen->max * 2 : 64;
		const	CFG$SCHEMA	**l_e = realloc(a_seen->ent, l_max * sizeof(*l_e));
		uint8_t		*l_l;

		if ( l_e )
			a_seen->ent = l_e;

		if ( !l_e || !(l_l = realloc(a_seen->inlist, l_max)) )
			return	a_seen->sts = CFG$_NOMEM;

		a_seen->inlist = l_l;
		a_seen->max = l_max;
		}

	a_seen->ent[a_seen->n] = a_e;
	a_seen->inlist[a_seen->n++] = (uint8_t) !!a_inlist;

	if ( !$ISSCALARTYPE(a_e->type) && (a_e->type != CFG$K_TYPE_GROUP) && (a_e->type != CFG$K_TYPE_LIST) && (a_e->type != CFG$K_TYPE_ARRAY) )
		return	CFG$_BADSCHEMA;

	if ( (a_e->type == CFG$K_TYPE_KWD) && !a_e->kwd )
		return	CFG$_BADSCHEMA;

	if ( ((a_e->flags & CFG$M_RANGE) || (a_e->flags & CFG$M_LENGTH) || (a_e->flags & CFG$M_COUNT)) && (a_e->min > a_e->max)
		&& !((a_e->type == CFG$K_TYPE_FLOAT) && (a_e->flags & CFG$M_RANGE)) )
		return	CFG$_BADSCHEMA;

	if ( (a_e->type == CFG$K_TYPE_FLOAT) && (a_e->flags & CFG$M_RANGE) && (a_e->fmin > a_e->fmax) )
		return	CFG$_BADSCHEMA;

	if ( (a_e->type == CFG$K_TYPE_LIST) || (a_e->type == CFG$K_TYPE_ARRAY) )
		{
		/* One entry for the elements; of an array - a scalar */
		if ( !a_e->sub || ((a_e->type == CFG$K_TYPE_ARRAY) && !$ISSCALARTYPE(a_e->sub->type)) )
			return	CFG$_BADSCHEMA;

		return	s_sound(a_e->sub, a_e->type == CFG$K_TYPE_LIST, a_depth + 1, a_seen);
		}

	if ( a_e->type != CFG$K_TYPE_GROUP )
		return	STS$K_SUCCESS;

	for ( const CFG$SCHEMA *l_s = a_e->sub; l_s && l_s->name; l_s++ )
		{
		/* A key: a scalar of a group that is an element of a list, only one */
		if ( l_s->flags & CFG$M_KEY )
			if ( !a_inlist || !$ISSCALARTYPE(l_s->type) || (l_keys++ > 0) || ((l_s->name[0] == '*') && !l_s->name[1]) )
				return	CFG$_BADSCHEMA;

		if ( !(1 & s_sound(l_s, 0, a_depth + 1, a_seen)) )
			return	(a_seen->sts == CFG$_NOMEM) ? CFG$_NOMEM : CFG$_BADSCHEMA;
		}

	return	STS$K_SUCCESS;
}



/*
**  DESCRIPTION: Name of a type for a diagnostic.
*/
static const char *	s_typename	(
		uint32_t	a_type
			)
{
	switch ( a_type )
		{
		case CFG$K_TYPE_GROUP:	return	"a group";
		case CFG$K_TYPE_LIST:	return	"a list";
		case CFG$K_TYPE_ARRAY:	return	"an array";
		case CFG$K_TYPE_INT:	return	"an integer";
		case CFG$K_TYPE_INT64:	return	"a 64-bit integer";
		case CFG$K_TYPE_FLOAT:	return	"a float";
		case CFG$K_TYPE_BOOL:	return	"a boolean";
		case CFG$K_TYPE_STRING:	return	"a string";
		case CFG$K_TYPE_KWD:	return	"a keyword";
		case CFG$K_TYPE_IPV4:	return	"an IPv4 address";
		case CFG$K_TYPE_IPV6:	return	"an IPv6 address";
		case CFG$K_TYPE_IPADDR:	return	"an IP address";
		case CFG$K_TYPE_MAC:	return	"a MAC address";
		}

	return	"?";
}



/*
**  DESCRIPTION: Does a node have the type an entry asks for?
*/
static int	s_typeok	(
		CFG$NODE		*a_n,
	const	CFG$SCHEMA		*a_e
			)
{
	switch ( a_e->type )
		{
		case CFG$K_TYPE_INT:	return	$ISINTEGER(a_n->type) && (a_n->v.i >= INT32_MIN) && (a_n->v.i <= INT32_MAX);
		case CFG$K_TYPE_INT64:	return	$ISINTEGER(a_n->type);
		case CFG$K_TYPE_FLOAT:	return	(a_n->type == CFG$K_TYPE_FLOAT) || $ISINTEGER(a_n->type);
		}

	if ( $ISSTRTYPE(a_e->type) )
		return	a_n->type == CFG$K_TYPE_STRING;

	return	a_n->type == a_e->type;
}



/*
**  DESCRIPTION: Is a string a MAC address: six pairs of hex digits, ':' or '-' between them?
*/
static int	s_ismac	(
	const	char		*a_s,
		uint32_t	a_len
			)
{
	if ( a_len != 17 )
		return	0;

	for ( uint32_t i = 0; i < 17; i++ )
		{
		char	l_c = a_s[i];

		if ( (i % 3) == 2 )
			{
			if ( (l_c != a_s[2]) || ((l_c != ':') && (l_c != '-')) )
				return	0;
			}
		else if ( !(((l_c >= '0') && (l_c <= '9')) || (((l_c | 0x20) >= 'a') && ((l_c | 0x20) <= 'f'))) )
			return	0;
		}

	return	1;
}



/*
**  DESCRIPTION: Is a string an address of the kind?
*/
static int	s_isaddr	(
		uint32_t	a_type,
	const	char		*a_s,
		uint32_t	a_len
			)
{
char	l_tmp[64];
uint8_t	l_bin[16];

	if ( a_type == CFG$K_TYPE_MAC )
		return	s_ismac(a_s, a_len);

	if ( !a_len || (a_len >= sizeof(l_tmp)) || memchr(a_s, '\0', a_len) )
		return	0;

	memcpy(l_tmp, a_s, a_len);
	l_tmp[a_len] = '\0';

	if ( (a_type != CFG$K_TYPE_IPV6) && (inet_pton(AF_INET, l_tmp, l_bin) == 1) )
		return	1;

	if ( (a_type != CFG$K_TYPE_IPV4) && (inet_pton(AF_INET6, l_tmp, l_bin) == 1) )
		return	1;

	return	0;
}



static void	s_group	(CFG$VAL *a_v, CFG$NODE *a_n, const CFG$SCHEMA *a_e);
static void	s_seq	(CFG$VAL *a_v, CFG$NODE *a_n, const CFG$SCHEMA *a_e);



/*
**  DESCRIPTION: Check one node against its entry.
*/
static void	s_node	(
		CFG$VAL		*a_v,
		CFG$NODE	*a_n,
	const	CFG$SCHEMA	*a_e
			)
{
CFG$TEXT	l_t = { .len = 0 };
uint32_t	l_mlen = 0, l_errs = a_v->hasbase ? a_v->base : a_v->nerrors;
unsigned	l_sts;

	a_v->hasbase = 0;			/* for this node only				*/

	if ( a_e->flags & CFG$M_DEPRECATED )
		s_diag(a_v, CFG$_DEPRECATED, a_n, NULL);

	if ( !s_typeok(a_n, a_e) )
		{
		s_adds(&l_t, ": ");
		s_adds(&l_t, s_typename(a_e->type));
		s_adds(&l_t, " expected, found ");
		s_adds(&l_t, (($ISINTEGER(a_n->type)) && (a_e->type == CFG$K_TYPE_INT)) ? "a 64-bit integer" : s_typename(a_n->type));
		s_diag(a_v, CFG$_VTYPE, a_n, &l_t);
		return;					/* not looked into				*/
		}

	/* Integers and floats: the range */
	if ( (a_e->flags & CFG$M_RANGE) && $ISINTEGER(a_e->type) && ((a_n->v.i < a_e->min) || (a_n->v.i > a_e->max)) )
		{
		s_adds(&l_t, " = ");
		s_addvalue(&l_t, a_n);
		s_adds(&l_t, ", allowed ");
		s_addnum(&l_t, a_e->min);
		s_adds(&l_t, "..");
		s_addnum(&l_t, a_e->max);
		s_diag(a_v, CFG$_RANGE, a_n, &l_t);
		}

	if ( (a_e->flags & CFG$M_RANGE) && (a_e->type == CFG$K_TYPE_FLOAT) )
		{
		double	l_f = (a_n->type == CFG$K_TYPE_FLOAT) ? a_n->v.f : (double) a_n->v.i;

		if ( (l_f < a_e->fmin) || (l_f > a_e->fmax) )
			{
			s_adds(&l_t, " = ");
			s_addvalue(&l_t, a_n);
			s_adds(&l_t, ", allowed ");
			s_addfloat(&l_t, a_e->fmin);
			s_adds(&l_t, "..");
			s_addfloat(&l_t, a_e->fmax);
			s_diag(a_v, CFG$_RANGE, a_n, &l_t);
			}
		}

	/* Strings: the length, the keywords, the addresses */
	if ( $ISSTRTYPE(a_e->type) )
		{
		if ( (a_e->flags & CFG$M_LENGTH) && (((int64_t) a_n->v.s.len < a_e->min) || ((int64_t) a_n->v.s.len > a_e->max)) )
			{
			s_adds(&l_t, " = ");
			s_addvalue(&l_t, a_n);
			s_adds(&l_t, " is ");
			s_addnum(&l_t, a_n->v.s.len);
			s_adds(&l_t, " long, allowed ");
			s_addnum(&l_t, a_e->min);
			s_adds(&l_t, "..");
			s_addnum(&l_t, a_e->max);
			s_diag(a_v, CFG$_LENGTH, a_n, &l_t);
			l_t.len = 0;
			}

		if ( a_e->type == CFG$K_TYPE_KWD )
			{
			const	CFG$KEYWORD	*l_k;

			for ( l_k = a_e->kwd; l_k->name; l_k++ )
				if ( (strlen(l_k->name) == a_n->v.s.len) && ((a_e->flags & CFG$M_CASEKWD)
					? !memcmp(l_k->name, a_n->v.s.ptr, a_n->v.s.len) : !strncasecmp(l_k->name, a_n->v.s.ptr, a_n->v.s.len)) )
					break;

			if ( !l_k->name )
				{
				s_adds(&l_t, " = ");
				s_addvalue(&l_t, a_n);
				s_adds(&l_t, ", allowed:");

				for ( l_k = a_e->kwd; l_k->name; l_k++ )
					{
					s_adds(&l_t, (l_k == a_e->kwd) ? " " : ", ");
					s_adds(&l_t, l_k->name);
					}

				s_diag(a_v, CFG$_BADKWD, a_n, &l_t);
				}
			}

		if ( (a_e->type >= CFG$K_TYPE_IPV4) && (a_e->type <= CFG$K_TYPE_MAC) && !s_isaddr(a_e->type, a_n->v.s.ptr, a_n->v.s.len) )
			{
			s_adds(&l_t, " = ");
			s_addvalue(&l_t, a_n);
			s_adds(&l_t, " is not ");
			s_adds(&l_t, s_typename(a_e->type));
			s_diag(a_v, CFG$_BADADDR, a_n, &l_t);
			}
		}

	/* Containers: what is inside */
	if ( a_e->type == CFG$K_TYPE_GROUP )
		s_group(a_v, a_n, a_e);
	else if ( (a_e->type == CFG$K_TYPE_LIST) || (a_e->type == CFG$K_TYPE_ARRAY) )
		s_seq(a_v, a_n, a_e);

	/* The program's own check, last: it sees a node that passed the rest, its inside too */
	if ( a_e->check && (a_v->nerrors == l_errs) )
		{
		CFG$TEXT	l_m = { .len = 0 };

		l_sts = a_e->check(a_n, a_v->arg, l_m.buf + 2, sizeof(l_m.buf) - 2, &l_mlen);

		if ( !(1 & l_sts) )
			{
			l_m.buf[0] = ':';
			l_m.buf[1] = ' ';
			l_m.len = 2 + ((l_mlen < sizeof(l_m.buf) - 2) ? l_mlen : (uint32_t) sizeof(l_m.buf) - 2);
			s_diag(a_v, CFG$_CHECK, a_n, &l_m);
			}
		}
}



/*
**  DESCRIPTION: The settings of a group against the table of its entry.
*/
static void	s_group	(
		CFG$VAL		*a_v,
		CFG$NODE	*a_n,
	const	CFG$SCHEMA	*a_e
			)
{
const	CFG$SCHEMA	*l_s;
CFG$NODE		*l_c;
uint32_t		l_old;

	for ( l_c = a_n->child; l_c; l_c = l_c->next )
		{
		if ( !$ISLIVE(l_c) )
			continue;

		l_old = s_push(a_v, l_c->name, l_c->namelen);

		if ( (l_s = cfg$$schema_entry(a_e->sub, a_v->tree, l_c->name, l_c->namelen)) )
			s_node(a_v, l_c, l_s);
		else if ( !(a_e->flags & CFG$M_OPEN) )
			s_diag(a_v, CFG$_UNKNOWN, l_c, NULL);

		s_pop(a_v, l_old);
		}

	/* What must be there */
	for ( l_s = a_e->sub; l_s && l_s->name; l_s++ )
		if ( (l_s->flags & (CFG$M_REQUIRED | CFG$M_KEY)) && !((l_s->name[0] == '*') && !l_s->name[1])
			&& !cfg$$find(a_n, l_s->name, (uint32_t) strlen(l_s->name)) )
			{
			l_old = s_push(a_v, l_s->name, (uint32_t) strlen(l_s->name));
			s_diag(a_v, CFG$_REQUIRED, a_n, NULL);
			s_pop(a_v, l_old);
			}
}



/*
**  DESCRIPTION: The elements of a list or an array against the entry for them; the count; the
**		uniqueness of a key.
*/
static void	s_seq	(
		CFG$VAL		*a_v,
		CFG$NODE	*a_n,
	const	CFG$SCHEMA	*a_e
			)
{
const	char	*l_key = (a_e->type == CFG$K_TYPE_LIST) ? cfg$$schema_key(a_e->sub) : NULL;
uint32_t	l_klen = l_key ? (uint32_t) strlen(l_key) : 0, l_i = 0, l_old;
CFG$NODE	*l_c, *l_k, *l_d, *l_j;
CFG$TEXT	l_t = { .len = 0 };

	if ( (a_e->flags & CFG$M_COUNT) && (((int64_t) a_n->count < a_e->min) || ((int64_t) a_n->count > a_e->max)) )
		{
		s_adds(&l_t, " has ");
		s_addnum(&l_t, a_n->count);
		s_adds(&l_t, ", allowed ");
		s_addnum(&l_t, a_e->min);
		s_adds(&l_t, "..");
		s_addnum(&l_t, a_e->max);
		s_diag(a_v, CFG$_COUNT, a_n, &l_t);
		}

	for ( l_c = a_n->child; l_c; l_c = l_c->next )
		{
		CFG$TEXT	l_step = { .len = 0 };

		if ( !$ISLIVE(l_c) )
			continue;

		/* The step: [key=value] when the element has its key, else [n] */
		s_adds(&l_step, "[");

		if ( l_key && (l_c->type == CFG$K_TYPE_GROUP) && (l_k = cfg$$find(l_c, l_key, l_klen))
			&& (l_k->type >= CFG$K_TYPE_INT) && (l_k->type <= CFG$K_TYPE_STRING) )
			{
			s_add(&l_step, l_key, l_klen);
			s_adds(&l_step, "=");
			s_addvalue(&l_step, l_k);
			}
		else	s_addnum(&l_step, l_i);

		s_adds(&l_step, "]");

		l_old = s_push(a_v, l_step.buf, l_step.len);

		/* The key first: an element with a key seen before is not offered to the check routine */
		a_v->base = a_v->nerrors;

		/* A key seen before in this list */
		if ( l_key && (l_c->type == CFG$K_TYPE_GROUP) && (l_k = cfg$$find(l_c, l_key, l_klen)) && $ISSCALARTYPE(l_k->type) )
			for ( l_d = a_n->child; l_d && (l_d != l_c); l_d = l_d->next )
				if ( $ISLIVE(l_d) && (l_d->type == CFG$K_TYPE_GROUP) && (l_j = cfg$$find(l_d, l_key, l_klen)) && $ISSCALARTYPE(l_j->type)
					&& (l_j->type == l_k->type || ($ISINTEGER(l_j->type) && $ISINTEGER(l_k->type)))
					&& ($ISINTEGER(l_k->type) ? (l_j->v.i == l_k->v.i)
					: (l_k->type == CFG$K_TYPE_STRING) ? ((l_j->v.s.len == l_k->v.s.len) && !memcmp(l_j->v.s.ptr, l_k->v.s.ptr, l_k->v.s.len))
					: (l_k->type == CFG$K_TYPE_BOOL) ? (!l_j->v.b == !l_k->v.b)
					: !memcmp(&l_j->v.f, &l_k->v.f, sizeof(double))) )
					{
					CFG$TEXT	l_m = { .len = 0 };

					if ( !l_d->line )
						s_adds(&l_m, ", the same as an element added by the program");
					else	{
						s_adds(&l_m, ", the same as line ");
						s_addnum(&l_m, l_d->line);

						/* The other one came from another file */
						if ( l_d->srcno != l_c->srcno )
							{
							s_adds(&l_m, " of ");
							s_adds(&l_m, a_v->tree->src[l_d->srcno]->path);
							}
						}
					s_diag(a_v, CFG$_DUPKEY, l_c, &l_m);
					break;
					}

		a_v->hasbase = 1;
		s_node(a_v, l_c, a_e->sub);
		s_pop(a_v, l_old);
		l_i++;
		}
}



/*
**  DESCRIPTION: Validate a tree by a schema.
**
**  INPUTS:
**	a_tree:		the tree; the record of its diagnostics is started afresh
**	a_schema:	the table of the settings of the root group, ended by a NULL name
**	a_arg:		passed to the check routines of the schema
**
**  OUTPUTS:
**	a_nerr:		number of diagnostics (errors and warnings); may be NULL
**
**  RETURNS:
**	STS$K_SUCCESS	- the tree matches (there may be DEPRECATED warnings);
**	CFG$_INVALID	- it does not: the diagnostics are in the tree, for CFG$PUTERRS;
**	CFG$_BADSCHEMA	- the schema itself is wrong (a programmer's error);
**	CFG$_BROKEN, CFG$_BADARG
*/
unsigned	cfg$validate	(
		CFG$TREE	*a_tree,
	const	CFG$SCHEMA	*a_schema,
		void		*a_arg,
		uint32_t	*a_nerr
			)
{
CFG$VAL		*l_v;
CFG$SCHEMA	l_root;
CFG$SEEN	l_seen = { NULL, NULL, 0, 0, STS$K_SUCCESS };
unsigned	l_sts;

	if ( !a_tree || !a_schema )
		return	CFG$_BADARG;

	if ( a_tree->broken )
		return	CFG$_BROKEN;

	/* The root is a group whose table is the schema */
	memset(&l_root, 0, sizeof(l_root));
	l_root.name = "";
	l_root.type = CFG$K_TYPE_GROUP;
	l_root.sub = a_schema;

	l_sts = s_sound(&l_root, 0, 0, &l_seen);
	free(l_seen.ent);
	free(l_seen.inlist);

	if ( !(1 & l_sts) )
		return	l_sts;

	if ( !(l_v = calloc(1, sizeof(CFG$VAL))) )
		return	CFG$_NOMEM;

	l_v->tree = a_tree;
	l_v->arg = a_arg;

	a_tree->nerr = 0;
	a_tree->ndropped = 0;

	s_group(l_v, a_tree->root, &l_root);

	if ( a_nerr )
		*a_nerr = l_v->ndiag;

	l_sts = l_v->nerrors ? CFG$_INVALID : STS$K_SUCCESS;
	free(l_v);

	return	l_sts;
}
