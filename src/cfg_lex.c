#define	__MODULE__	"CFG_LEX"
#define	__IDENT__	"X.01-01"
#define	__REV__		"1.1.0"
#define	__FAC__		"CFG"

/*
**++
**
**  FACILITY:	yaLIBCFG - configuration trees in the libconfig syntax
**
**  MODULE:	cfg_lex.c
**
**  ABSTRACT:	The lexer: splits a source buffer into tokens with their offsets and
**		positions, and notes where the comments are.
**
**  DESCRIPTION: Written by hand, not by flex: the writer of the library needs the exact offsets
**		of every token and of every comment, and the parse needs to know nothing else.
**		Whitespace and comments are not tokens; the spans of the comments are kept in the
**		source (COMM) so that the parse can tell a comment line from a blank one when it
**		gathers the comment lines that belong to a setting.
**
**		Tokens follow libconfig 1.7:
**		    name	[A-Za-z*][-A-Za-z0-9_*]*
**		    integer	[-+]?[0-9]+ | 0x[0-9A-Fa-f]+ | 0b[01]+ | 0o[0-7]+, an L or LL suffix: 64 bits
**		    float	[-+]?([0-9]*.[0-9]*([eE][-+]?[0-9]+)? | [0-9]+[eE][-+]?[0-9]+)
**		    boolean	true / false, any case
**		    string	"..." with \\ \" \n \r \t \f \xHH
**		    comments	# ..., // ..., slash-star ... star-slash
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
#include	<locale.h>
#include	<pthread.h>
#include	<errno.h>
#include	<math.h>

#include	"cfg_int.h"

#define	CFG$K_MAXNUM		64		/* longest text of a number			*/

static	pthread_once_t	s_cloc_once = PTHREAD_ONCE_INIT;
static	locale_t	s_cloc;			/* "C" locale for strtod_l()			*/

static	void	s_cloc_init	(void)
{
	s_cloc = newlocale(LC_NUMERIC_MASK, "C", (locale_t) 0);
}

#define	$ISSPACE(c)	( ((c) == ' ') || ((c) == '\t') || ((c) == '\r') || ((c) == '\n') || ((c) == '\f') || ((c) == '\v') )
#define	$ISDIGIT(c)	( ((c) >= '0') && ((c) <= '9') )
#define	$ISALPHA(c)	( (((c) >= 'a') && ((c) <= 'z')) || (((c) >= 'A') && ((c) <= 'Z')) )
#define	$ISNAME0(c)	( $ISALPHA(c) || ((c) == '*') )
#define	$ISNAME(c)	( $ISNAME0(c) || $ISDIGIT(c) || ((c) == '-') || ((c) == '_') )



/*
**  DESCRIPTION: Name of a token for a diagnostic.
*/
const char *	cfg$$tokname	(
		uint32_t	a_tok
			)
{
static	const char	*s_names[] = { "end of file", "name", "integer", "integer", "float", "boolean", "string",
				"'='", "';'", "','", "'{'", "'}'", "'['", "']'", "'('", "')'", "@include" };

	return	(a_tok < $ARRSZ(s_names)) ? s_names[a_tok] : "?";
}



/*
**  DESCRIPTION: Note the span of a comment in the source.
**
**  RETURNS:
**	STS$K_SUCCESS, CFG$_NOMEM
*/
static unsigned	s_note_comment	(
		CFG$SRC		*a_src,
		uint32_t	a_beg,
		uint32_t	a_end
			)
{
uint32_t	*l_new;

	if ( (a_src->ncomm + 1) * 2 > a_src->commmax )
		{
		uint32_t	l_max = a_src->commmax ? a_src->commmax * 2 : 64;

		if ( !(l_new = realloc(a_src->comm, l_max * sizeof(uint32_t))) )
			return	CFG$_NOMEM;

		a_src->comm = l_new;
		a_src->commmax = l_max;
		}

	a_src->comm[a_src->ncomm * 2] = a_beg;
	a_src->comm[a_src->ncomm * 2 + 1] = a_end;
	a_src->ncomm++;

	return	STS$K_SUCCESS;
}



/*
**  DESCRIPTION: Tell whether a stretch of the source holds nothing but whitespace and comments.
**
**  INPUTS:
**	a_src:		the source; its comment spans are complete for the stretch
**	a_beg,a_end:	the stretch
**
**  RETURNS:
**	1 - only whitespace and comments (and at least one comment), 0 - anything else
*/
int	cfg$$lex_comment	(
		CFG$SRC		*a_src,
		uint32_t	a_beg,
		uint32_t	a_end
			)
{
int	l_seen = 0;
uint32_t	l_lo, l_hi, l_mid;

	for ( uint32_t p = a_beg; p < a_end; )
		{
		if ( $ISSPACE(a_src->buf[p]) )
			{
			p++;
			continue;
			}

		/* Binary search of the comment span that begins at P */
		for ( l_lo = 0, l_hi = a_src->ncomm; l_lo < l_hi; )
			{
			l_mid = (l_lo + l_hi) / 2;

			if ( a_src->comm[l_mid * 2] < p )
				l_lo = l_mid + 1;
			else	l_hi = l_mid;
			}

		if ( (l_lo >= a_src->ncomm) || (a_src->comm[l_lo * 2] != p) )
			return	0;

		p = a_src->comm[l_lo * 2 + 1];
		l_seen = 1;
		}

	return	l_seen;
}



/*
**  DESCRIPTION: The "C" locale for the conversions of floating numbers (made on the first use).
*/
void *	cfg$$cloc	(void)
{
	pthread_once(&s_cloc_once, s_cloc_init);

	return	(void *) s_cloc;
}



/*
**  DESCRIPTION: Find the comment span that holds an offset.
**
**  INPUTS:
**	a_src:	the source
**	a_pos:	the offset
**
**  OUTPUTS:
**	a_beg,a_end:	the span, when found
**
**  RETURNS:
**	1 - A_POS is inside a comment, 0 - it is not
*/
int	cfg$$span_at	(
		CFG$SRC		*a_src,
		uint32_t	a_pos,
		uint32_t	*a_beg,
		uint32_t	*a_end
			)
{
uint32_t	l_lo = 0, l_hi = a_src->ncomm, l_mid;

	/* The last span that begins at or before A_POS */
	while ( l_lo < l_hi )
		{
		l_mid = (l_lo + l_hi) / 2;

		if ( a_src->comm[l_mid * 2] <= a_pos )
			l_lo = l_mid + 1;
		else	l_hi = l_mid;
		}

	if ( !l_lo || (a_pos >= a_src->comm[(l_lo - 1) * 2 + 1]) )
		return	0;

	*a_beg = a_src->comm[(l_lo - 1) * 2];
	*a_end = a_src->comm[(l_lo - 1) * 2 + 1];

	return	1;
}



/*
**  DESCRIPTION: Prepare a lexer for a source.
*/
unsigned	cfg$$lex_init	(
		CFG$LEX		*a_lex,
		CFG$TREE	*a_tree,
		CFG$SRC		*a_src
			)
{
	memset(a_lex, 0, sizeof(CFG$LEX));

	a_lex->tree = a_tree;
	a_lex->src = a_src;
	a_lex->line = 1;

	pthread_once(&s_cloc_once, s_cloc_init);

	return	STS$K_SUCCESS;
}



/*
**  DESCRIPTION: Record a diagnostic at a position of the source.
*/
static unsigned	s_lexerr	(
		CFG$LEX		*a_lex,
		unsigned	a_sts,
		uint32_t	a_line,
		uint32_t	a_col,
	const	char		*a_text,
		size_t		a_len
			)
{
	return	cfg$$error(a_lex->tree, a_sts, a_lex->src->no, a_line, a_col, a_text, a_len);
}



/*
**  DESCRIPTION: Skip whitespace and comments, noting the comments.
**
**  RETURNS:
**	STS$K_SUCCESS, CFG$_UNTERM (a comment is not closed), CFG$_NOMEM
*/
static unsigned	s_skip	(
		CFG$LEX		*a_lex
			)
{
const	char	*l_buf = a_lex->src->buf;
uint32_t	l_len = a_lex->src->len, l_beg, l_line, l_col;
unsigned	l_sts;

	while ( a_lex->pos < l_len )
		{
		char	l_c = l_buf[a_lex->pos];

		if ( l_c == '\n' )
			{
			a_lex->pos++;
			a_lex->line++;
			a_lex->lpos = a_lex->pos;
			continue;
			}

		if ( $ISSPACE(l_c) )
			{
			a_lex->pos++;
			continue;
			}

		l_beg = a_lex->pos;

		/* # ... and // ... - up to the end of the line, the newline is not a part of it */
		if ( (l_c == '#') || ((l_c == '/') && (a_lex->pos + 1 < l_len) && (l_buf[a_lex->pos + 1] == '/')) )
			{
			while ( (a_lex->pos < l_len) && (l_buf[a_lex->pos] != '\n') )
				a_lex->pos++;

			if ( !(1 & (l_sts = s_note_comment(a_lex->src, l_beg, a_lex->pos))) )
				return	l_sts;

			continue;
			}

		/* slash-star ... star-slash */
		if ( (l_c == '/') && (a_lex->pos + 1 < l_len) && (l_buf[a_lex->pos + 1] == '*') )
			{
			l_line = a_lex->line;
			l_col = a_lex->pos - a_lex->lpos + 1;
			a_lex->pos += 2;

			for ( ;; )
				{
				if ( a_lex->pos + 1 >= l_len )
					return	s_lexerr(a_lex, CFG$_UNTERM, l_line, l_col, "comment", 7);

				if ( (l_buf[a_lex->pos] == '*') && (l_buf[a_lex->pos + 1] == '/') )
					{
					a_lex->pos += 2;
					break;
					}

				if ( l_buf[a_lex->pos] == '\n' )
					{
					a_lex->line++;
					a_lex->lpos = a_lex->pos + 1;
					}

				a_lex->pos++;
				}

			if ( !(1 & (l_sts = s_note_comment(a_lex->src, l_beg, a_lex->pos))) )
				return	l_sts;

			continue;
			}

		break;
		}

	return	STS$K_SUCCESS;
}



/*
**  DESCRIPTION: Scan a number at the current position.
**
**  IMPLICIT OUTPUTS:
**	TOK, IVAL or FVAL, FMT, SUFFIXL of the lexer; POS past the number
**
**  RETURNS:
**	STS$K_SUCCESS, CFG$_BADNUM
*/
static unsigned	s_number	(
		CFG$LEX		*a_lex
			)
{
const	char	*l_buf = a_lex->src->buf;
uint32_t	l_len = a_lex->src->len, l_p = a_lex->pos, l_digits = 0, l_base = 10;
int		l_neg = 0, l_float = 0;
uint64_t	l_mag = 0, l_lim;
char		l_tmp[CFG$K_MAXNUM + 1];

	a_lex->fmt = CFG$K_FMT_DEC;
	a_lex->suffixl = 0;

	if ( (l_buf[l_p] == '-') || (l_buf[l_p] == '+') )
		l_neg = (l_buf[l_p++] == '-');

	/* 0x, 0b, 0o prefixes */
	if ( (l_p + 1 < l_len) && (l_buf[l_p] == '0') )
		{
		char	l_x = l_buf[l_p + 1] | 0x20;

		if ( l_x == 'x' )
			{
			l_base = 16;
			a_lex->fmt = CFG$K_FMT_HEX;
			}
		else if ( l_x == 'b' )
			{
			l_base = 2;
			a_lex->fmt = CFG$K_FMT_BIN;
			}
		else if ( l_x == 'o' )
			{
			l_base = 8;
			a_lex->fmt = CFG$K_FMT_OCT;
			}

		if ( l_base != 10 )
			l_p += 2;
		}

	if ( l_base == 10 )
		{
		/* Decimal: an integer unless a '.' or an exponent follows */
		uint32_t	l_q = l_p;

		while ( (l_q < l_len) && $ISDIGIT(l_buf[l_q]) )
			l_q++, l_digits++;

		if ( (l_q < l_len) && (l_buf[l_q] == '.') )
			{
			l_float = 1;
			l_q++;

			while ( (l_q < l_len) && $ISDIGIT(l_buf[l_q]) )
				l_q++, l_digits++;
			}

		if ( l_digits && (l_q < l_len) && ((l_buf[l_q] | 0x20) == 'e') )
			{
			uint32_t	l_e = l_q + 1, l_ed = 0;

			if ( (l_e < l_len) && ((l_buf[l_e] == '-') || (l_buf[l_e] == '+')) )
				l_e++;

			while ( (l_e < l_len) && $ISDIGIT(l_buf[l_e]) )
				l_e++, l_ed++;

			if ( l_ed )
				{
				l_float = 1;
				l_q = l_e;
				}
			}

		if ( !l_digits )
			goto	l_bad;

		if ( l_float )
			{
			char	*l_endp;

			if ( (l_q - a_lex->pos) > CFG$K_MAXNUM )
				goto	l_bad;

			memcpy(l_tmp, l_buf + a_lex->pos, l_q - a_lex->pos);
			l_tmp[l_q - a_lex->pos] = '\0';

			errno = 0;
			a_lex->fval = strtod_l(l_tmp, &l_endp, s_cloc);

			/* Too big - no text can stand for infinity; too small - a subnormal or zero, as read */
			if ( (*l_endp != '\0') || ((errno == ERANGE) && isinf(a_lex->fval)) )
				goto	l_bad;

			a_lex->tok = CFG$K_TOK_FLOAT;
			a_lex->pos = l_q;
			goto	l_check;
			}
		}

	/* An integer in the base */
	for ( l_digits = 0; l_p < l_len; l_p++, l_digits++ )
		{
		unsigned	l_d;
		char		l_c = l_buf[l_p];

		if ( $ISDIGIT(l_c) )
			l_d = (unsigned) (l_c - '0');
		else if ( (l_base == 16) && (((l_c | 0x20) >= 'a') && ((l_c | 0x20) <= 'f')) )
			l_d = (unsigned) ((l_c | 0x20) - 'a' + 10);
		else	break;

		if ( l_d >= l_base )
			goto	l_bad;

		if ( l_mag > (UINT64_MAX - l_d) / l_base )
			goto	l_bad;

		l_mag = l_mag * l_base + l_d;
		}

	if ( !l_digits )
		goto	l_bad;

	/* L or LL suffix */
	if ( (l_p < l_len) && ((l_buf[l_p] | 0x20) == 'l') )
		{
		a_lex->suffixl = 1;
		l_p++;

		if ( (l_p < l_len) && ((l_buf[l_p] | 0x20) == 'l') )
			l_p++;
		}

	/*
	** Decimal: the range of int64_t.  A hexadecimal, binary or octal number may fill all 64 bits,
	** as in libconfig: 0xFFFFFFFFFFFFFFFF is -1.
	*/
	l_lim = (l_base == 10) ? (l_neg ? (uint64_t) INT64_MAX + 1 : (uint64_t) INT64_MAX) : UINT64_MAX;

	if ( l_mag > l_lim )
		goto	l_bad;

	a_lex->ival = l_neg ? (int64_t) (0 - l_mag) : (int64_t) l_mag;

	if ( (l_base != 10) && !l_neg && (l_mag <= UINT32_MAX) && !a_lex->suffixl )
		a_lex->ival = (int64_t) (int32_t) (uint32_t) l_mag;	/* 0xFFFFFFFF - a 32-bit -1, as libconfig	*/

	a_lex->tok = (a_lex->suffixl || (a_lex->ival < INT32_MIN) || (a_lex->ival > INT32_MAX)) ? CFG$K_TOK_INT64 : CFG$K_TOK_INT;
	a_lex->pos = l_p;

l_check:
	/* Glued to a name character - not a number */
	if ( (a_lex->pos < l_len) && ($ISNAME(l_buf[a_lex->pos]) || (l_buf[a_lex->pos] == '.')) )
		goto	l_bad;

	return	STS$K_SUCCESS;

l_bad:
	/* The whole of the bad token, from its beginning, for the diagnostic */
	a_lex->pos = a_lex->tbeg;

	for ( l_p = a_lex->pos; (l_p < l_len) && ($ISNAME(l_buf[l_p]) || (l_buf[l_p] == '.') || (l_buf[l_p] == '+') || (l_buf[l_p] == '-')); l_p++ )
		;

	return	s_lexerr(a_lex, CFG$_BADNUM, a_lex->line, a_lex->pos - a_lex->lpos + 1, l_buf + a_lex->pos, (l_p > a_lex->pos) ? l_p - a_lex->pos : 1);
}



/*
**  DESCRIPTION: Take the next token.
**
**  IMPLICIT OUTPUTS:
**	TOK, TBEG, TEND, TLINE, TCOL and the value fields of the lexer
**
**  RETURNS:
**	STS$K_SUCCESS, CFG$_SYNTAX, CFG$_UNTERM, CFG$_BADNUM, CFG$_NOMEM
*/
unsigned	cfg$$lex_next	(
		CFG$LEX		*a_lex
			)
{
const	char	*l_buf = a_lex->src->buf;
uint32_t	l_len = a_lex->src->len;
unsigned	l_sts;
char		l_c;

	if ( !(1 & (l_sts = s_skip(a_lex))) )
		return	l_sts;

	a_lex->tbeg = a_lex->pos;
	a_lex->tline = a_lex->line;
	a_lex->tcol = a_lex->pos - a_lex->lpos + 1;

	if ( a_lex->pos >= l_len )
		{
		a_lex->tok = CFG$K_TOK_EOF;
		a_lex->tend = a_lex->pos;
		return	STS$K_SUCCESS;
		}

	l_c = l_buf[a_lex->pos];

	switch ( l_c )
		{
		case '=':
		case ':':	a_lex->tok = CFG$K_TOK_EQ;	a_lex->pos++;	break;
		case ';':	a_lex->tok = CFG$K_TOK_SEMI;	a_lex->pos++;	break;
		case ',':	a_lex->tok = CFG$K_TOK_COMMA;	a_lex->pos++;	break;
		case '{':	a_lex->tok = CFG$K_TOK_LBRACE;	a_lex->pos++;	break;
		case '}':	a_lex->tok = CFG$K_TOK_RBRACE;	a_lex->pos++;	break;
		case '[':	a_lex->tok = CFG$K_TOK_LBRACK;	a_lex->pos++;	break;
		case ']':	a_lex->tok = CFG$K_TOK_RBRACK;	a_lex->pos++;	break;
		case '(':	a_lex->tok = CFG$K_TOK_LPAREN;	a_lex->pos++;	break;
		case ')':	a_lex->tok = CFG$K_TOK_RPAREN;	a_lex->pos++;	break;

		case '"':
			/* A string - the escapes are checked when it is decoded */
			for ( a_lex->pos++; ; a_lex->pos++ )
				{
				if ( (a_lex->pos >= l_len) || (l_buf[a_lex->pos] == '\n') )
					return	s_lexerr(a_lex, CFG$_UNTERM, a_lex->tline, a_lex->tcol, "string", 6);

				if ( l_buf[a_lex->pos] == '\\' )
					{
					a_lex->pos++;
					if ( (a_lex->pos < l_len) && (l_buf[a_lex->pos] == '\n') )
						return	s_lexerr(a_lex, CFG$_UNTERM, a_lex->tline, a_lex->tcol, "string", 6);
					continue;
					}

				if ( l_buf[a_lex->pos] == '"' )
					{
					a_lex->pos++;
					break;
					}
				}

			a_lex->tok = CFG$K_TOK_STRING;
			break;

		case '@':
			if ( (l_len - a_lex->pos >= 8) && !memcmp(l_buf + a_lex->pos, "@include", 8)
				&& ((a_lex->pos + 8 >= l_len) || !$ISNAME(l_buf[a_lex->pos + 8])) )
				{
				a_lex->tok = CFG$K_TOK_INCLUDE;
				a_lex->pos += 8;
				break;
				}

			return	s_lexerr(a_lex, CFG$_SYNTAX, a_lex->tline, a_lex->tcol, "unexpected '@'", 14);

		default:
			if ( $ISDIGIT(l_c) || (((l_c == '-') || (l_c == '+') || (l_c == '.')) && (a_lex->pos + 1 < l_len)
				&& ($ISDIGIT(l_buf[a_lex->pos + 1]) || (l_buf[a_lex->pos + 1] == '.'))) )
				{
				if ( !(1 & (l_sts = s_number(a_lex))) )
					return	l_sts;
				break;
				}

			if ( $ISNAME0(l_c) )
				{
				while ( (a_lex->pos < l_len) && $ISNAME(l_buf[a_lex->pos]) )
					a_lex->pos++;

				a_lex->tok = CFG$K_TOK_NAME;

				if ( ((a_lex->pos - a_lex->tbeg) == 4) && !strncasecmp(l_buf + a_lex->tbeg, "true", 4) )
					{
					a_lex->tok = CFG$K_TOK_BOOL;
					a_lex->ival = 1;
					}
				else if ( ((a_lex->pos - a_lex->tbeg) == 5) && !strncasecmp(l_buf + a_lex->tbeg, "false", 5) )
					{
					a_lex->tok = CFG$K_TOK_BOOL;
					a_lex->ival = 0;
					}

				break;
				}

			{
			char	l_msg[32] = "unexpected character 'x'";

			l_msg[22] = ((unsigned char) l_c >= ' ') ? l_c : '?';
			return	s_lexerr(a_lex, CFG$_SYNTAX, a_lex->tline, a_lex->tcol, l_msg, 24);
			}
		}

	a_lex->tend = a_lex->pos;

	return	STS$K_SUCCESS;
}



/*
**  DESCRIPTION: Column of an offset, from 1: octets since the last newline before it.
*/
static uint32_t	s_col	(
	const	char		*a_buf,
		uint32_t	a_pos
			)
{
uint32_t	l_p = a_pos;

	while ( l_p && (a_buf[l_p - 1] != '\n') )
		l_p--;

	return	a_pos - l_p + 1;
}



/*
**  DESCRIPTION: Decode a string value: one or more quoted pieces with only whitespace and
**		comments between them, escapes resolved, the pieces joined.
**
**  INPUTS:
**	a_lex:		the lexer (for the source and the diagnostics)
**	a_beg,a_end:	the text from the first quote to past the last one
**
**  OUTPUTS:
**	a_out:		at least A_END - A_BEG octets
**	a_outlen:	the length of the result
**
**  RETURNS:
**	STS$K_SUCCESS, CFG$_BADESC
*/
unsigned	cfg$$lex_string	(
		CFG$LEX		*a_lex,
		uint32_t	a_beg,
		uint32_t	a_end,
		char		*a_out,
		uint32_t	*a_outlen
			)
{
const	char	*l_buf = a_lex->src->buf;
uint32_t	l_o = 0, l_line = a_lex->tline;

	for ( uint32_t p = a_beg; p < a_end; )
		{
		if ( l_buf[p] != '"' )
			{
			/* Between the pieces: skip whitespace and comments */
			if ( l_buf[p] == '\n' )
				l_line++;

			if ( (l_buf[p] == '/') && (p + 1 < a_end) && (l_buf[p + 1] == '*') )
				{
				for ( p += 2; (p + 1 < a_end) && !((l_buf[p] == '*') && (l_buf[p + 1] == '/')); p++ )
					if ( l_buf[p] == '\n' )
						l_line++;
				p += 2;
				continue;
				}

			if ( (l_buf[p] == '#') || ((l_buf[p] == '/') && (p + 1 < a_end) && (l_buf[p + 1] == '/')) )
				{
				while ( (p < a_end) && (l_buf[p] != '\n') )
					p++;
				continue;
				}

			p++;
			continue;
			}

		/* A quoted piece */
		for ( p++; (p < a_end) && (l_buf[p] != '"'); p++ )
			{
			if ( l_buf[p] != '\\' )
				{
				a_out[l_o++] = l_buf[p];
				continue;
				}

			p++;

			switch ( l_buf[p] )
				{
				case '\\':	a_out[l_o++] = '\\';	break;
				case '"':	a_out[l_o++] = '"';	break;
				case 'n':	a_out[l_o++] = '\n';	break;
				case 'r':	a_out[l_o++] = '\r';	break;
				case 't':	a_out[l_o++] = '\t';	break;
				case 'f':	a_out[l_o++] = '\f';	break;

				case 'x':
					{
					unsigned	l_v = 0;

					for ( int i = 1; i <= 2; i++ )
						{
						char	l_h = (p + i < a_end) ? l_buf[p + i] : 0;

						if ( $ISDIGIT(l_h) )
							l_v = l_v * 16 + (unsigned) (l_h - '0');
						else if ( ((l_h | 0x20) >= 'a') && ((l_h | 0x20) <= 'f') )
							l_v = l_v * 16 + (unsigned) ((l_h | 0x20) - 'a' + 10);
						else	return	s_lexerr(a_lex, CFG$_BADESC, l_line, s_col(l_buf, p - 1), l_buf + p - 1, 2 + (uint32_t) (i - 1));
						}

					a_out[l_o++] = (char) l_v;
					p += 2;
					break;
					}

				default:
					return	s_lexerr(a_lex, CFG$_BADESC, l_line, s_col(l_buf, p - 1), l_buf + p - 1, 2);
				}
			}

		p++;					/* the closing quote				*/
		}

	*a_outlen = l_o;

	return	STS$K_SUCCESS;
}
