#define	__MODULE__	"CFG_PARSE"
#define	__IDENT__	"X.01-01"
#define	__REV__		"1.1.0"
#define	__FAC__		"CFG"

/*
**++
**
**  FACILITY:	yaLIBCFG - configuration trees in the libconfig syntax
**
**  MODULE:	cfg_parse.c
**
**  ABSTRACT:	The parse: source files into a tree, @include, the offsets every node needs
**		for the lossless write-back.
**
**  DESCRIPTION: A recursive descent over the tokens of CFG_LEX.C:
**
**		    settings	:= { setting | @include "file" }
**		    setting	:= name ( '=' | ':' ) value [ ';' | ',' ]
**		    value	:= scalar | '{' settings '}' | '[' [ scalar { ',' scalar } ] ']'
**				   | '(' [ value { ',' value } ] ')'
**
**		The grammar is LL(1) and tiny; a generated parser would bring nothing but a
**		second place where the offsets of the tokens have to be tracked.
**
**		Comments belong to the node just below them: the comment lines right above a
**		setting or an element (no blank line between) are its LEADING text, a comment on
**		the same line after its terminator is the end of its TAIL.  So a node removed by
**		the program takes its comments with it.
**
**		@include is taken where a setting may stand; the file it names must hold whole
**		settings, and a file may be included only once in a tree.  (libconfig splices an included file in as raw text, so a file holding
**		half a group works there; it does not here.)  The settings of an included file
**		become children of the group the directive stands in, with the number of their
**		own source, so that the writer puts them back into their own file.
**
**		The parse stops at the first error.
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
#include	<errno.h>
#include	<fcntl.h>
#include	<unistd.h>
#include	<limits.h>
#include	<sys/stat.h>

#include	"cfg_int.h"

typedef	struct cfg_parse_t {
	CFG$TREE	*tree;
	CFG$SRC		*src;
	CFG$LEX		lex;
	uint32_t	depth;				/* nesting of values				*/
	uint32_t	prevend;			/* end of the last token taken			*/
	struct cfg_parse_t *up;				/* the file that includes this one		*/
} CFG$PARSE;

static unsigned	s_settings	(CFG$PARSE *a_p, CFG$NODE *a_cont, uint32_t a_floor, uint32_t a_endtok);
static unsigned	s_value		(CFG$PARSE *a_p, CFG$NODE *a_node, uint32_t a_floor);



/*
**  DESCRIPTION: Take the next token, remembering where the current one ended.
*/
static unsigned	s_next	(
		CFG$PARSE	*a_p
			)
{
	a_p->prevend = a_p->lex.tend;

	return	cfg$$lex_next(&a_p->lex);
}



/*
**  DESCRIPTION: Record a syntax error at the current token: "expected X, found Y".
*/
static unsigned	s_expected	(
		CFG$PARSE	*a_p,
	const	char		*a_what
			)
{
char		l_msg[96];
fao_desc_t	l_dsc = $FAO_DESC(l_msg);
unsigned short	l_len = 0;

	$CFG_FAO("expected !AZ, found !AZ", &l_len, &l_dsc, a_what, cfg$$tokname(a_p->lex.tok));

	return	cfg$$error(a_p->tree, CFG$_SYNTAX, a_p->src->no, a_p->lex.tline, a_p->lex.tcol, l_msg, l_len);
}



/*
**  DESCRIPTION: Offset of the beginning of the line that holds an offset.
*/
static uint32_t	s_linestart	(
	const	char		*a_buf,
		uint32_t	a_pos
			)
{
	while ( a_pos && (a_buf[a_pos - 1] != '\n') )
		a_pos--;

	return	a_pos;
}



/*
**  DESCRIPTION: Where the leading text of a node begins: the comment lines right above it.
**
**  INPUTS:
**	a_src:		the source
**	a_floor:	nothing before it may be taken - the end of the previous sibling or the
**			inside of the opening bracket
**	a_beg:		the first token of the node
**
**  RETURNS:
**	the offset; A_BEG when something else stands before the node on its line
*/
static uint32_t	s_lead	(
		CFG$SRC		*a_src,
		uint32_t	a_floor,
		uint32_t	a_beg
			)
{
const	char	*l_buf = a_src->buf;
uint32_t	l_lead, l_pls, l_ple, l_q, l_cb, l_ce;

	l_lead = s_linestart(l_buf, a_beg);

	if ( l_lead < a_floor )
		return	a_beg;

	for ( l_q = l_lead; l_q < a_beg; l_q++ )
		if ( (l_buf[l_q] != ' ') && (l_buf[l_q] != '\t') )
			return	a_beg;

	/* Climb over the comment lines above */
	while ( l_lead && (l_lead > a_floor) )
		{
		l_ple = l_lead - 1;				/* the newline of the line above		*/
		l_pls = s_linestart(l_buf, l_ple);

		if ( l_pls < a_floor )
			break;

		/* The last octet of the line above that is not whitespace */
		for ( l_q = l_ple; (l_q > l_pls) && ((l_buf[l_q - 1] == ' ') || (l_buf[l_q - 1] == '\t') || (l_buf[l_q - 1] == '\r')); l_q-- )
			;

		if ( l_q == l_pls )
			break;					/* a blank line					*/

		/* The end of a block comment that began on an earlier line */
		if ( cfg$$span_at(a_src, l_q - 1, &l_cb, &l_ce) && (l_cb < l_pls) )
			{
			l_pls = s_linestart(l_buf, l_cb);

			if ( l_pls < a_floor )
				break;
			}

		if ( !cfg$$lex_comment(a_src, l_pls, l_ple) )
			break;

		l_lead = l_pls;
		}

	return	l_lead;
}



/*
**  DESCRIPTION: Where the tail of a node ends: past a comment on the same line and the newline.
**
**  INPUTS:
**	a_src:	the source
**	a_pos:	past the terminator (or the value, when there is none)
**
**  RETURNS:
**	the offset
*/
static uint32_t	s_tail	(
		CFG$SRC		*a_src,
		uint32_t	a_pos
			)
{
const	char	*l_buf = a_src->buf;
uint32_t	l_q = a_pos, l_cb, l_ce;

	while ( (l_q < a_src->len) && ((l_buf[l_q] == ' ') || (l_buf[l_q] == '\t')) )
		l_q++;

	if ( (l_q < a_src->len) && cfg$$span_at(a_src, l_q, &l_cb, &l_ce) && (l_cb == l_q)
		&& !memchr(l_buf + l_cb, '\n', l_ce - l_cb) )
		{
		l_q = l_ce;

		while ( (l_q < a_src->len) && ((l_buf[l_q] == ' ') || (l_buf[l_q] == '\t')) )
			l_q++;
		}

	if ( l_q >= a_src->len )
		return	l_q;

	if ( l_buf[l_q] == '\r' )
		l_q++;

	if ( (l_q < a_src->len) && (l_buf[l_q] == '\n') )
		return	l_q + 1;

	return	a_pos;
}



/*
**  DESCRIPTION: Read a whole file into a new source of the tree.
**
**  INPUTS:
**	a_tree:		the tree
**	a_path:		file name
**	a_depth:	@include depth
**
**  OUTPUTS:
**	a_src:		the source
**
**  RETURNS:
**	STS$K_SUCCESS, CFG$_OPENIN, CFG$_READERR, CFG$_TOOBIG, CFG$_NOMEM
*/
static unsigned	s_load	(
		CFG$TREE	*a_tree,
	const	char		*a_path,
		uint32_t	a_depth,
		CFG$SRC		**a_src
			)
{
CFG$SRC		*l_src;
struct stat	l_st;
int		l_fd, l_err;
ssize_t		l_rc;
uint32_t	l_got = 0;
char		l_real[PATH_MAX];

	if ( a_tree->nsrc >= CFG$K_MAXSRC )
		return	cfg$$syserr(a_tree, CFG$_OPENIN, a_path, EMFILE);

	if ( 0 > (l_fd = open(a_path, O_RDONLY | O_CLOEXEC)) )
		return	cfg$$syserr(a_tree, CFG$_OPENIN, a_path, errno);

	if ( fstat(l_fd, &l_st) )
		{
		l_err = errno;
		close(l_fd);
		return	cfg$$syserr(a_tree, CFG$_READERR, a_path, l_err);
		}

	if ( !S_ISREG(l_st.st_mode) || (l_st.st_size > (off_t) CFG$K_MAXFILE) )
		{
		close(l_fd);
		return	cfg$$syserr(a_tree, CFG$_READERR, a_path, S_ISREG(l_st.st_mode) ? EFBIG : EINVAL);
		}

	if ( !(l_src = calloc(1, sizeof(CFG$SRC))) || !(l_src->buf = malloc((size_t) l_st.st_size + 1)) || !(l_src->path = strdup(a_path)) )
		{
		close(l_fd);

		if ( l_src )
			{
			free(l_src->buf);
			free(l_src);
			}

		return	CFG$_NOMEM;
		}

	while ( l_got < (uint32_t) l_st.st_size )
		{
		if ( 0 > (l_rc = read(l_fd, l_src->buf + l_got, (size_t) l_st.st_size - l_got)) )
			{
			if ( errno == EINTR )
				continue;

			l_err = errno;
			close(l_fd);
			free(l_src->buf);
			free(l_src->path);
			free(l_src);
			return	cfg$$syserr(a_tree, CFG$_READERR, a_path, l_err);
			}

		if ( !l_rc )
			break;

		l_got += (uint32_t) l_rc;
		}

	close(l_fd);

	l_src->len = l_got;
	l_src->buf[l_got] = '\0';
	l_src->depth = (uint16_t) a_depth;
	l_src->mtime = (int64_t) l_st.st_mtime;
	l_src->real = realpath(a_path, l_real) ? strdup(l_real) : NULL;
	l_src->no = (uint16_t) a_tree->nsrc;
	a_tree->src[a_tree->nsrc++] = l_src;

	*a_src = l_src;

	return	STS$K_SUCCESS;
}



/*
**  DESCRIPTION: Make the tree and its root.
*/
static unsigned	s_newtree	(
		uint32_t	a_opts,
		CFG$TREE	**a_tree
			)
{
CFG$TREE	*l_tree;

	if ( !(l_tree = calloc(1, sizeof(CFG$TREE))) )
		return	CFG$_NOMEM;

	l_tree->opts = a_opts;

	if ( !(l_tree->root = cfg$$node(l_tree, NULL, CFG$K_TYPE_GROUP)) )
		{
		free(l_tree);
		return	CFG$_NOMEM;
		}

	*a_tree = l_tree;

	return	STS$K_SUCCESS;
}



/*
**  DESCRIPTION: @include "file" at the current token.
**
**  INPUTS:
**	a_p:		the parse of the including file
**	a_cont:		the group the directive stands in
**	a_floor:	end of the previous sibling
**
**  OUTPUTS:
**	a_end:		end of the directive's text (the floor for the next sibling)
**
**  RETURNS:
**	STS$K_SUCCESS or a condition value of the parse of the included file
*/
static unsigned	s_include_buf	(
		CFG$PARSE	*a_p,
		CFG$NODE	*a_cont,
		uint32_t	a_floor,
		uint32_t	*a_end,
		char		*l_name,
		char		*l_path
			)
{
CFG$NODE	*l_node;
CFG$PARSE	l_inc, *l_up;
CFG$SRC		*l_isrc;
uint32_t	l_len, l_line = a_p->lex.tline, l_col = a_p->lex.tcol, l_beg = a_p->lex.tbeg;
unsigned	l_sts;
const	char	*l_slash;

	if ( a_p->tree->opts & CFG$M_NOINCLUDE )
		return	cfg$$error(a_p->tree, CFG$_SYNTAX, a_p->src->no, l_line, l_col, "@include is not allowed", 23);

	if ( !(1 & (l_sts = s_next(a_p))) )
		return	l_sts;

	if ( a_p->lex.tok != CFG$K_TOK_STRING )
		return	s_expected(a_p, "a file name string");

	if ( (a_p->lex.tend - a_p->lex.tbeg) >= PATH_MAX )
		return	cfg$$error(a_p->tree, CFG$_TOOBIG, a_p->src->no, l_line, l_col, "file name", 9);

	if ( !(1 & (l_sts = cfg$$lex_string(&a_p->lex, a_p->lex.tbeg, a_p->lex.tend, l_name, &l_len))) )
		return	l_sts;

	l_name[l_len] = '\0';

	if ( !(l_node = cfg$$node(a_p->tree, a_cont, CFG$K_TYPE_INCLUDE)) )
		return	CFG$_NOMEM;

	l_node->srcno = a_p->src->no;
	l_node->line = l_line;
	l_node->col = l_col;
	l_node->beg = l_beg;
	l_node->vbeg = a_p->lex.tbeg;
	l_node->vend = a_p->lex.tend;
	l_node->lead = s_lead(a_p->src, a_floor, l_beg);

	if ( !(1 & (l_sts = s_next(a_p))) )
		return	l_sts;

	/* An optional ';' after the directive is the directive's */
	if ( a_p->lex.tok == CFG$K_TOK_SEMI )
		{
		l_node->vend = a_p->lex.tend;

		if ( !(1 & (l_sts = s_next(a_p))) )
			return	l_sts;
		}

	l_node->end = s_tail(a_p->src, l_node->vend);
	*a_end = l_node->end;

	/* A relative name is taken from the directory of the including file */
	if ( (l_name[0] != '/') && (l_slash = strrchr(a_p->src->path, '/')) )
		{
		if ( (size_t) (l_slash - a_p->src->path) + 1 + l_len + 1 > PATH_MAX )
			return	cfg$$error(a_p->tree, CFG$_TOOBIG, a_p->src->no, l_line, l_col, "file name", 9);

		memcpy(l_path, a_p->src->path, (size_t) (l_slash - a_p->src->path) + 1);
		memcpy(l_path + (l_slash - a_p->src->path) + 1, l_name, l_len + 1);
		}
	else	memcpy(l_path, l_name, l_len + 1);

	if ( a_p->src->depth + 1 > CFG$K_MAXINCDEPTH )
		return	cfg$$error(a_p->tree, CFG$_INCDEPTH, a_p->src->no, l_line, l_col, l_name, l_len);

	if ( !(1 & (l_sts = s_load(a_p->tree, l_path, a_p->src->depth + 1U, &l_isrc))) )
		{
		/* The OPENIN of the file is on record; say also where it was asked for */
		cfg$$error(a_p->tree, CFG$_INCLUDE, a_p->src->no, l_line, l_col, l_name, l_len);
		return	l_sts;
		}

	for ( l_up = a_p; l_up; l_up = l_up->up )
		if ( l_isrc->real && l_up->src->real && !strcmp(l_isrc->real, l_up->src->real) )
			return	cfg$$error(a_p->tree, CFG$_INCLOOP, a_p->src->no, l_line, l_col, l_name, l_len);

	/*
	** The same file twice: its settings would stand in two places, and a write-back of the
	** two could not both go into the one file - refused.
	*/
	for ( uint32_t i = 0; i + 1 < a_p->tree->nsrc; i++ )
		if ( l_isrc->real && a_p->tree->src[i]->real && !strcmp(l_isrc->real, a_p->tree->src[i]->real) )
			return	cfg$$error(a_p->tree, CFG$_INCTWICE, a_p->src->no, l_line, l_col, l_name, l_len);

	l_node->incsrc = l_isrc->no;

	memset(&l_inc, 0, sizeof(l_inc));
	l_inc.tree = a_p->tree;
	l_inc.src = l_isrc;
	l_inc.depth = a_p->depth;
	l_inc.up = a_p;

	cfg$$lex_init(&l_inc.lex, a_p->tree, l_isrc);

	if ( !(1 & (l_sts = cfg$$lex_next(&l_inc.lex))) )
		return	l_sts;

	return	s_settings(&l_inc, a_cont, 0, CFG$K_TOK_EOF);
}



/*
**  DESCRIPTION: @include "file" - the names in buffers from the heap, so that the frames of the
**		recursion of the parse stay small whatever the compiler inlines.
*/
static unsigned	s_include	(
		CFG$PARSE	*a_p,
		CFG$NODE	*a_cont,
		uint32_t	a_floor,
		uint32_t	*a_end
			)
{
char		*l_buf;
unsigned	l_sts;

	if ( !(l_buf = malloc(2 * PATH_MAX)) )
		return	CFG$_NOMEM;

	l_sts = s_include_buf(a_p, a_cont, a_floor, a_end, l_buf, l_buf + PATH_MAX);
	free(l_buf);

	return	l_sts;
}



/*
**  DESCRIPTION: One setting at the current token (a name).
**
**  INPUTS:
**	a_p:		the parse
**	a_cont:		the group
**	a_floor:	end of the previous sibling
**
**  OUTPUTS:
**	a_end:		end of the setting's text
**
**  RETURNS:
**	STS$K_SUCCESS or a condition value of the parse
*/
static unsigned	s_setting	(
		CFG$PARSE	*a_p,
		CFG$NODE	*a_cont,
		uint32_t	a_floor,
		uint32_t	*a_end
			)
{
CFG$NODE	*l_node;
unsigned	l_sts;
uint32_t	l_nlen = a_p->lex.tend - a_p->lex.tbeg, l_after;
const	char	*l_name = a_p->src->buf + a_p->lex.tbeg;

	if ( l_nlen > CFG$K_MAXNAME )
		return	cfg$$error(a_p->tree, CFG$_TOOBIG, a_p->src->no, a_p->lex.tline, a_p->lex.tcol, "setting name", 12);

	if ( cfg$$find(a_cont, l_name, l_nlen) )
		return	cfg$$error(a_p->tree, CFG$_DUPNAME, a_p->src->no, a_p->lex.tline, a_p->lex.tcol, l_name, l_nlen);

	if ( !(l_node = cfg$$node(a_p->tree, a_cont, CFG$K_TYPE_NONE)) )
		return	CFG$_NOMEM;

	l_node->srcno = a_p->src->no;
	l_node->name = l_name;
	l_node->namelen = (uint8_t) l_nlen;
	l_node->line = a_p->lex.tline;
	l_node->col = a_p->lex.tcol;
	l_node->beg = a_p->lex.tbeg;
	l_node->lead = s_lead(a_p->src, a_floor, l_node->beg);

	if ( !(1 & (l_sts = s_next(a_p))) )
		return	l_sts;

	if ( a_p->lex.tok != CFG$K_TOK_EQ )
		return	s_expected(a_p, "'=' or ':'");

	if ( !(1 & (l_sts = s_next(a_p))) )
		return	l_sts;

	l_node->vbeg = a_p->lex.tbeg;

	if ( !(1 & (l_sts = s_value(a_p, l_node, l_node->vbeg))) )
		return	l_sts;

	l_node->vend = a_p->prevend;
	l_after = l_node->vend;

	if ( (a_p->lex.tok == CFG$K_TOK_SEMI) || (a_p->lex.tok == CFG$K_TOK_COMMA) )
		{
		l_after = a_p->lex.tend;

		if ( !(1 & (l_sts = s_next(a_p))) )
			return	l_sts;
		}

	l_node->end = s_tail(a_p->src, l_after);
	*a_end = l_node->end;

	return	STS$K_SUCCESS;
}



/*
**  DESCRIPTION: The settings of a group (or of a file) up to the closing token.
**
**  INPUTS:
**	a_p:		the parse
**	a_cont:		the group
**	a_floor:	where the inside of the group begins
**	a_endtok:	'}' or end of file
**
**  RETURNS:
**	STS$K_SUCCESS or a condition value of the parse; the closing token is current
*/
static unsigned	s_settings	(
		CFG$PARSE	*a_p,
		CFG$NODE	*a_cont,
		uint32_t	a_floor,
		uint32_t	a_endtok
			)
{
unsigned	l_sts;
uint32_t	l_floor = a_floor;

	for ( ;; )
		{
		if ( a_p->lex.tok == a_endtok )
			return	STS$K_SUCCESS;

		if ( a_p->lex.tok == CFG$K_TOK_INCLUDE )
			l_sts = s_include(a_p, a_cont, l_floor, &l_floor);
		else if ( a_p->lex.tok == CFG$K_TOK_NAME )
			l_sts = s_setting(a_p, a_cont, l_floor, &l_floor);
		else	return	s_expected(a_p, (a_endtok == CFG$K_TOK_EOF) ? "a setting name" : "a setting name or '}'");

		if ( !(1 & l_sts) )
			return	l_sts;
		}
}



/*
**  DESCRIPTION: The elements of a list or an array up to the closing bracket.
**
**  INPUTS:
**	a_p:		the parse
**	a_cont:		the list or array
**	a_floor:	where its inside begins
**	a_endtok:	')' or ']'
**
**  RETURNS:
**	STS$K_SUCCESS or a condition value of the parse; the closing token is current
*/
static unsigned	s_elements	(
		CFG$PARSE	*a_p,
		CFG$NODE	*a_cont,
		uint32_t	a_floor,
		uint32_t	a_endtok
			)
{
CFG$NODE	*l_node, *l_first = NULL;
unsigned	l_sts;
uint32_t	l_floor = a_floor, l_after, l_tline, l_tcol;

	while ( a_p->lex.tok != a_endtok )
		{
		l_tline = a_p->lex.tline;
		l_tcol = a_p->lex.tcol;

		if ( (a_endtok == CFG$K_TOK_RBRACK) && ((a_p->lex.tok < CFG$K_TOK_INT) || (a_p->lex.tok > CFG$K_TOK_STRING)) )
			return	s_expected(a_p, "a scalar value or ']'");

		if ( !(l_node = cfg$$node(a_p->tree, a_cont, CFG$K_TYPE_NONE)) )
			return	CFG$_NOMEM;

		l_node->srcno = a_p->src->no;
		l_node->line = l_tline;
		l_node->col = l_tcol;
		l_node->beg = l_node->vbeg = a_p->lex.tbeg;
		l_node->lead = s_lead(a_p->src, l_floor, l_node->beg);

		if ( !(1 & (l_sts = s_value(a_p, l_node, l_node->vbeg))) )
			return	l_sts;

		l_node->vend = a_p->prevend;
		l_after = l_node->vend;

		/* An array: all of one type, integers of both sizes count as one */
		if ( a_endtok == CFG$K_TOK_RBRACK )
			{
			if ( !l_first )
				l_first = l_node;
			else if ( (l_first->type != l_node->type) && !(((l_first->type == CFG$K_TYPE_INT) || (l_first->type == CFG$K_TYPE_INT64))
					&& ((l_node->type == CFG$K_TYPE_INT) || (l_node->type == CFG$K_TYPE_INT64))) )
				return	cfg$$error(a_p->tree, CFG$_MIXARRAY, a_p->src->no, l_tline, l_tcol,
					a_p->src->buf + l_node->vbeg, l_node->vend - l_node->vbeg);
			}

		if ( a_p->lex.tok == CFG$K_TOK_COMMA )
			{
			l_node->sep = a_p->lex.tbeg;
			l_after = a_p->lex.tend;

			if ( !(1 & (l_sts = s_next(a_p))) )
				return	l_sts;

			/* libconfig takes no ',' before the closing bracket - nor do we: what we write it must read */
			if ( a_p->lex.tok == a_endtok )
				return	s_expected(a_p, "a value after ','");
			}
		else if ( a_p->lex.tok != a_endtok )
			return	s_expected(a_p, (a_endtok == CFG$K_TOK_RBRACK) ? "',' or ']'" : "',' or ')'");

		l_node->end = s_tail(a_p->src, l_after);
		l_floor = l_node->end;
		}

	return	STS$K_SUCCESS;
}



/*
**  DESCRIPTION: A value at the current token, into a node.
**
**  INPUTS:
**	a_p:		the parse
**	a_node:		the node (VBEG set)
**	a_floor:	unused for scalars; the opening bracket for containers
**
**  IMPLICIT OUTPUTS:
**	TYPE, V, FMT, FLAGS of the node; the token after the value is current, PREVEND is
**	past the value
**
**  RETURNS:
**	STS$K_SUCCESS or a condition value of the parse
*/
static unsigned	s_value	(
		CFG$PARSE	*a_p,
		CFG$NODE	*a_node,
		uint32_t	a_floor
			)
{
unsigned	l_sts;
uint32_t	l_beg, l_endtok;
char		*l_str;

	switch ( a_p->lex.tok )
		{
		case CFG$K_TOK_INT:
		case CFG$K_TOK_INT64:
			a_node->type = (a_p->lex.tok == CFG$K_TOK_INT) ? CFG$K_TYPE_INT : CFG$K_TYPE_INT64;
			a_node->v.i = a_p->lex.ival;
			a_node->fmt = a_p->lex.fmt;

			if ( a_p->lex.suffixl )
				a_node->flags |= CFG$M_SUFFIXL;

			return	s_next(a_p);

		case CFG$K_TOK_FLOAT:
			a_node->type = CFG$K_TYPE_FLOAT;
			a_node->v.f = a_p->lex.fval;
			return	s_next(a_p);

		case CFG$K_TOK_BOOL:
			a_node->type = CFG$K_TYPE_BOOL;
			a_node->v.b = (int) a_p->lex.ival;
			return	s_next(a_p);

		case CFG$K_TOK_STRING:
			/* "ab" "cd" - the pieces are joined */
			l_beg = a_p->lex.tbeg;

			while ( a_p->lex.tok == CFG$K_TOK_STRING )
				if ( !(1 & (l_sts = s_next(a_p))) )
					return	l_sts;

			if ( !(l_str = cfg$$alloc(a_p->tree, a_p->prevend - l_beg + 1)) )
				return	CFG$_NOMEM;

			if ( !(1 & (l_sts = cfg$$lex_string(&a_p->lex, l_beg, a_p->prevend, l_str, &a_node->v.s.len))) )
				return	l_sts;

			a_node->type = CFG$K_TYPE_STRING;
			a_node->v.s.ptr = l_str;
			return	STS$K_SUCCESS;

		case CFG$K_TOK_LBRACE:
		case CFG$K_TOK_LBRACK:
		case CFG$K_TOK_LPAREN:
			if ( ++a_p->depth > CFG$K_MAXDEPTH )
				return	cfg$$error(a_p->tree, CFG$_TOOBIG, a_p->src->no, a_p->lex.tline, a_p->lex.tcol, "nesting", 7);

			l_endtok = a_p->lex.tok + 1;	/* the closing token follows the opening one	*/
			a_node->type = (a_p->lex.tok == CFG$K_TOK_LBRACE) ? CFG$K_TYPE_GROUP :
				(a_p->lex.tok == CFG$K_TOK_LBRACK) ? CFG$K_TYPE_ARRAY : CFG$K_TYPE_LIST;
			l_beg = a_p->lex.tend;

			if ( !(1 & (l_sts = s_next(a_p))) )
				return	l_sts;

			l_sts = (a_node->type == CFG$K_TYPE_GROUP) ? s_settings(a_p, a_node, l_beg, l_endtok)
				: s_elements(a_p, a_node, l_beg, l_endtok);

			if ( !(1 & l_sts) )
				{
				/* Out of tokens inside the brackets - say which bracket is not closed */
				if ( (a_p->lex.tok == CFG$K_TOK_EOF) && (a_p->tree->nerr == 1) )
					{
					a_p->tree->nerr = 0;
					return	cfg$$error(a_p->tree, CFG$_UNTERM, a_p->src->no, a_node->line ? a_node->line : 1, a_node->col,
						cfg$$tokname(l_endtok - 1), strlen(cfg$$tokname(l_endtok - 1)));
					}

				return	l_sts;
				}

			a_p->depth--;
			return	s_next(a_p);

		default:
			return	s_expected(a_p, "a value");
		}

	(void) a_floor;
}



/*
**  DESCRIPTION: Parse the main source of a tree.
*/
static unsigned	s_parse_main	(
		CFG$TREE	*a_tree
			)
{
CFG$PARSE	l_p;
unsigned	l_sts;

	memset(&l_p, 0, sizeof(l_p));
	l_p.tree = a_tree;
	l_p.src = a_tree->src[0];

	a_tree->root->srcno = 0;
	a_tree->root->end = a_tree->root->vend = l_p.src->len;

	cfg$$lex_init(&l_p.lex, a_tree, l_p.src);

	if ( !(1 & (l_sts = cfg$$lex_next(&l_p.lex))) )
		return	l_sts;

	return	s_settings(&l_p, a_tree->root, 0, CFG$K_TOK_EOF);
}



/*
**  DESCRIPTION: Parse a file into a new tree.
**
**  INPUTS:
**	a_path:	file name
**	a_opts:	CFG$M_NOCASE, CFG$M_NOINCLUDE
**
**  OUTPUTS:
**	a_tree:	the tree - also when the parse failed (it holds the diagnostics for
**		CFG$PUTERRS); NULL only when there is no memory.  Free it with CFG$FREE.
**
**  RETURNS:
**	CFG$_NORMAL or the condition value of the first error
*/
unsigned	cfg$parse_file	(
	const	char		*a_path,
		uint32_t	a_opts,
		CFG$TREE	**a_tree
			)
{
CFG$SRC		*l_src;
unsigned	l_sts;

	*a_tree = NULL;

	if ( !a_path )
		return	CFG$_BADARG;

	if ( !(1 & (l_sts = s_newtree(a_opts, a_tree))) )
		return	l_sts;

	if ( !(1 & (l_sts = s_load(*a_tree, a_path, 0, &l_src))) )
		{
		(*a_tree)->broken = 1;
		return	l_sts;
		}

	if ( !(1 & (l_sts = s_parse_main(*a_tree))) )
		{
		(*a_tree)->broken = 1;
		return	l_sts;
		}

	return	CFG$_NORMAL;
}



/*
**  DESCRIPTION: Parse a buffer into a new tree.
**
**  INPUTS:
**	a_buf,a_len:	the text; copied
**	a_name:		name for the diagnostics and the base of relative @include names;
**			may be NULL ("<buffer>")
**	a_opts:		CFG$M_NOCASE, CFG$M_NOINCLUDE
**
**  OUTPUTS:
**	a_tree:		as for CFG$PARSE_FILE
**
**  RETURNS:
**	CFG$_NORMAL or the condition value of the first error
*/
unsigned	cfg$parse_buf	(
	const	char		*a_buf,
		uint32_t	a_len,
	const	char		*a_name,
		uint32_t	a_opts,
		CFG$TREE	**a_tree
			)
{
CFG$SRC		*l_src;
unsigned	l_sts;

	*a_tree = NULL;

	if ( !a_buf && a_len )
		return	CFG$_BADARG;

	if ( a_len > CFG$K_MAXFILE )
		return	CFG$_TOOBIG;

	if ( !(1 & (l_sts = s_newtree(a_opts, a_tree))) )
		return	l_sts;

	if ( !(l_src = calloc(1, sizeof(CFG$SRC))) || !(l_src->buf = malloc((size_t) a_len + 1))
		|| !(l_src->path = strdup(a_name ? a_name : "<buffer>")) )
		{
		if ( l_src )
			{
			free(l_src->buf);
			free(l_src);
			}

		return	CFG$_NOMEM;
		}

	if ( a_len )
		memcpy(l_src->buf, a_buf, a_len);

	l_src->buf[a_len] = '\0';
	l_src->len = a_len;
	(*a_tree)->src[(*a_tree)->nsrc++] = l_src;

	if ( !(1 & (l_sts = s_parse_main(*a_tree))) )
		{
		(*a_tree)->broken = 1;
		return	l_sts;
		}

	return	CFG$_NORMAL;
}
