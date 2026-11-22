#define	__MODULE__	"CFG_WRITE"
#define	__IDENT__	"X.01-01"
#define	__REV__		"1.1.0"
#define	__FAC__		"CFG"

/*
**++
**
**  FACILITY:	yaLIBCFG - configuration trees in the libconfig syntax
**
**  MODULE:	cfg_write.c
**
**  ABSTRACT:	The lossless writer: a tree back into the text of its source files.
**
**  DESCRIPTION: The text of a source is rebuilt from the text of the file itself:
**
**		  - a node nothing has touched is copied as it stood, comments and all;
**		  - a changed scalar gets a new text of the value, its name, '=', terminator and
**		    comments stay;
**		  - a group with changes below is walked: the text between its children is
**		    copied, every child is written by these same rules;
**		  - a removed node leaves out its text with the comment lines above it;
**		  - a new node is made in the canonical form, with the indentation of its
**		    siblings, after the last of them;
**		  - in a list or an array the commas are fixed up - an element followed by a
**		    live one gets a ',', the last one loses the ',' it had for a removed one;
**		  - a list or an array on one line that lost or gained elements is rebuilt on
**		    one line (the elements nothing touched keep their text).
**
**		So a tree written without changes is its file byte for byte.
**
**		The settings an @include brought live in the group of the directive with the
**		number of their own source; writing a source takes only its own nodes.
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
#include	<math.h>
#include	<string.h>
#include	<errno.h>
#include	<fcntl.h>
#include	<unistd.h>
#include	<locale.h>
#include	<limits.h>
#include	<sys/stat.h>

#include	"cfg_int.h"

#define	CFG$K_MAXINDENT		256		/* longest indentation that is made		*/

typedef	struct cfg_out_t {
	char		*p;
	uint32_t	len,
			max;
	unsigned	sts;				/* CFG$_NOMEM once a growth failed		*/
	CFG$TREE	*tree;
	char		unit[16];			/* one step of indentation of the source	*/
} CFG$OUT;

#define	$ISLIVE(n)	( !((n)->flags & CFG$M_DELETED) )
#define	$ISSCALAR(t)	( ((t) >= CFG$K_TYPE_INT) && ((t) <= CFG$K_TYPE_STRING) )
#define	$ISSEQ(t)	( ((t) == CFG$K_TYPE_LIST) || ((t) == CFG$K_TYPE_ARRAY) )

static void	s_gen_value	(CFG$OUT *a_o, CFG$NODE *a_n, const char *a_indent);
static void	s_children	(CFG$OUT *a_o, CFG$NODE *a_cont, uint32_t a_srcno, uint32_t a_cur, uint32_t a_end);



/*
**  DESCRIPTION: Append octets to the output; a failure is remembered in STS.
*/
static void	s_put	(
		CFG$OUT		*a_o,
	const	char		*a_ptr,
		uint32_t	a_len
			)
{
char	*l_new;

	if ( !(1 & a_o->sts) || !a_len )
		return;

	if ( a_o->len + a_len + 1 > a_o->max )
		{
		uint32_t	l_max = a_o->max ? a_o->max : 4096;

		while ( l_max < a_o->len + a_len + 1 )
			l_max *= 2;

		if ( !(l_new = realloc(a_o->p, l_max)) )
			{
			a_o->sts = CFG$_NOMEM;
			return;
			}

		a_o->p = l_new;
		a_o->max = l_max;
		}

	memcpy(a_o->p + a_o->len, a_ptr, a_len);
	a_o->len += a_len;
	a_o->p[a_o->len] = '\0';
}

static void	s_puts	(
		CFG$OUT		*a_o,
	const	char		*a_str
			)
{
	s_put(a_o, a_str, (uint32_t) strlen(a_str));
}

static char	s_lastc	(
		CFG$OUT		*a_o
			)
{
	return	a_o->len ? a_o->p[a_o->len - 1] : '\n';
}



/*
**  DESCRIPTION: The text of a string value: quoted, escaped.
**
**  OUTPUTS:
**	a_out,a_outlen:	the text, malloc()-ed, zero-terminated - free() it
**
**  RETURNS:
**	STS$K_SUCCESS, CFG$_NOMEM
*/
unsigned	cfg$$format_string	(
	const	char		*a_str,
		uint32_t	a_len,
		char		**a_out,
		uint32_t	*a_outlen
			)
{
static	const char	s_hex[] = "0123456789ABCDEF";
char		*l_out;
uint32_t	l_o = 0;

	if ( !(l_out = malloc((size_t) a_len * 4 + 3)) )
		return	CFG$_NOMEM;

	l_out[l_o++] = '"';

	for ( uint32_t i = 0; i < a_len; i++ )
		{
		unsigned char	l_c = (unsigned char) a_str[i];

		switch ( l_c )
			{
			case '"':	l_out[l_o++] = '\\';	l_out[l_o++] = '"';	break;
			case '\\':	l_out[l_o++] = '\\';	l_out[l_o++] = '\\';	break;
			case '\n':	l_out[l_o++] = '\\';	l_out[l_o++] = 'n';	break;
			case '\r':	l_out[l_o++] = '\\';	l_out[l_o++] = 'r';	break;
			case '\t':	l_out[l_o++] = '\\';	l_out[l_o++] = 't';	break;
			case '\f':	l_out[l_o++] = '\\';	l_out[l_o++] = 'f';	break;

			default:
				if ( (l_c < ' ') || (l_c == 0x7F) )
					{
					l_out[l_o++] = '\\';
					l_out[l_o++] = 'x';
					l_out[l_o++] = s_hex[l_c >> 4];
					l_out[l_o++] = s_hex[l_c & 0xF];
					}
				else	l_out[l_o++] = (char) l_c;
			}
		}

	l_out[l_o++] = '"';
	l_out[l_o] = '\0';

	*a_out = l_out;
	*a_outlen = l_o;

	return	STS$K_SUCCESS;
}



/*
**  DESCRIPTION: The text of an integer in the form it was written in.
*/
static void	s_gen_int	(
		CFG$OUT		*a_o,
		CFG$NODE	*a_n
			)
{
char		l_tmp[80], *l_p = l_tmp + sizeof(l_tmp);
int		l_wide = (a_n->type == CFG$K_TYPE_INT64);
uint64_t	l_u = l_wide ? (uint64_t) a_n->v.i : (uint64_t) (uint32_t) (int32_t) a_n->v.i;
unsigned	l_base = (a_n->fmt == CFG$K_FMT_HEX) ? 16 : (a_n->fmt == CFG$K_FMT_BIN) ? 2 : 8;

	/* Built from the right: suffix, digits, prefix or sign */
	if ( l_wide )
		*--l_p = 'L';

	if ( a_n->fmt == CFG$K_FMT_DEC )
		{
		l_u = (a_n->v.i < 0) ? 0 - (uint64_t) a_n->v.i : (uint64_t) a_n->v.i;
		l_base = 10;
		}

	do	{
		*--l_p = "0123456789ABCDEF"[l_u % l_base];
		l_u /= l_base;
		} while ( l_u );

	if ( a_n->fmt != CFG$K_FMT_DEC )
		{
		*--l_p = (a_n->fmt == CFG$K_FMT_HEX) ? 'x' : (a_n->fmt == CFG$K_FMT_BIN) ? 'b' : 'o';
		*--l_p = '0';
		}
	else if ( a_n->v.i < 0 )
		*--l_p = '-';

	s_put(a_o, l_p, (uint32_t) (l_tmp + sizeof(l_tmp) - l_p));
}



/*
**  DESCRIPTION: The exact decimal digits of a positive finite double.
**
**		A double is M * 2^E with an integer M of 53 bits.  For E >= 0 the value is the
**		integer M * 2^E; for E < 0 it is M * 5^-E * 10^E - the integer M * 5^-E with the
**		decimal point moved.  The integer is built in limbs of base 10^9, so its decimal
**		digits fall out directly.  At most 1126 halvings: some 800 digits, 90 limbs.
**
**  OUTPUTS:
**	a_dig:		the digits, no leading zeros, not terminated; room for CFG$K_MAXDIG
**	a_ndig:		how many
**	a_dec:		position of the decimal point: value = 0.DIGITS * 10^A_DEC
*/
#define	CFG$K_LIMBS		100
#define	CFG$K_MAXDIG		(CFG$K_LIMBS * 9)
#define	CFG$K_BASE		1000000000U

static void	s_exact	(
		double		a_val,
		char		*a_dig,
		int		*a_ndig,
		int		*a_dec
			)
{
uint32_t	l_limb[CFG$K_LIMBS];			/* little-endian, base 10^9			*/
int		l_n = 0, l_ex, l_e, l_o = 0;
uint64_t	l_m, l_carry;
double		l_f = frexp(a_val, &l_ex);

	l_m = (uint64_t) ldexp(l_f, 53);
	l_e = l_ex - 53;

	while ( l_m && !(l_m & 1) )			/* fewer factors to multiply by			*/
		{
		l_m >>= 1;
		l_e++;
		}

	l_limb[l_n++] = (uint32_t) (l_m % CFG$K_BASE);

	if ( (l_m /= CFG$K_BASE) )
		l_limb[l_n++] = (uint32_t) (l_m % CFG$K_BASE);

	if ( (l_m /= CFG$K_BASE) )
		l_limb[l_n++] = (uint32_t) l_m;

	/* Multiply by 2^E or 5^-E, in steps that keep a product of a limb within 64 bits */
	for ( int l_left = (l_e >= 0) ? l_e : -l_e; l_left > 0; )
		{
		int		l_step = (l_e >= 0) ? ((l_left > 29) ? 29 : l_left) : ((l_left > 13) ? 13 : l_left);
		uint64_t	l_mul = 1;

		for ( int i = 0; i < l_step; i++ )
			l_mul *= (l_e >= 0) ? 2 : 5;

		l_carry = 0;

		for ( int i = 0; i < l_n; i++ )
			{
			uint64_t	l_p = (uint64_t) l_limb[i] * l_mul + l_carry;

			l_limb[i] = (uint32_t) (l_p % CFG$K_BASE);
			l_carry = l_p / CFG$K_BASE;
			}

		while ( l_carry && (l_n < CFG$K_LIMBS) )
			{
			l_limb[l_n++] = (uint32_t) (l_carry % CFG$K_BASE);
			l_carry /= CFG$K_BASE;
			}

		l_left -= l_step;
		}

	/* The digits, the most significant limb without its leading zeros */
	for ( int i = l_n - 1; i >= 0; i-- )
		{
		char	l_nine[9];
		int	l_k = 9;

		for ( uint32_t l_v = l_limb[i]; l_k; l_v /= 10 )
			l_nine[--l_k] = (char) ('0' + l_v % 10);

		for ( l_k = 0; (i == l_n - 1) && (l_k < 8) && (l_nine[l_k] == '0'); l_k++ )
			;

		memcpy(a_dig + l_o, l_nine + l_k, (size_t) (9 - l_k));
		l_o += 9 - l_k;
		}

	*a_ndig = l_o;
	*a_dec = l_o + ((l_e < 0) ? l_e : 0);
}



/*
**  DESCRIPTION: Round exact digits to A_PREC significant ones, half to even.
**
**  INPUTS:
**	a_dig,a_ndig,a_dec:	the exact digits (S_EXACT)
**	a_prec:			significant digits wanted
**
**  OUTPUTS:
**	a_out:			A_PREC digits (or fewer when the exact ones are fewer)
**	a_nout,a_odec:		their count and the position of the decimal point
*/
static void	s_round	(
	const	char		*a_dig,
		int		a_ndig,
		int		a_dec,
		int		a_prec,
		char		*a_out,
		int		*a_nout,
		int		*a_odec
			)
{
int	l_up = 0, i;

	*a_odec = a_dec;

	if ( a_ndig <= a_prec )
		{
		memcpy(a_out, a_dig, (size_t) a_ndig);
		*a_nout = a_ndig;
		return;
		}

	memcpy(a_out, a_dig, (size_t) a_prec);
	*a_nout = a_prec;

	if ( a_dig[a_prec] > '5' )
		l_up = 1;
	else if ( a_dig[a_prec] == '5' )
		{
		for ( i = a_prec + 1; (i < a_ndig) && (a_dig[i] == '0'); i++ )
			;

		l_up = (i < a_ndig) || ((a_dig[a_prec - 1] - '0') & 1);
		}

	if ( !l_up )
		return;

	for ( i = a_prec - 1; i >= 0; i-- )
		{
		if ( a_out[i] != '9' )
			{
			a_out[i]++;
			return;
			}

		a_out[i] = '0';
		}

	/* 999 -> 1000: one digit more in front */
	a_out[0] = '1';
	(*a_odec)++;
}



/*
**  DESCRIPTION: The text of a floating number: the fewest significant digits that read back as
**		the same double, always with a '.' or an exponent.  No format strings, no locale:
**		the digits are those of the exact value (S_EXACT) rounded (S_ROUND); positional
**		notation for exponents -5..15, else d.ddde+XX - the look of %g that libconfig
**		writes.
*/
static void	s_gen_float	(
		CFG$OUT		*a_o,
		double		a_val
			)
{
char		l_exact[CFG$K_MAXDIG], l_dig[24], l_tmp[64], l_exp[8];
int		l_nex = 0, l_dex = 0, l_dec = 0, l_nd = 0, l_x, l_o = 0, l_e = 0, l_neg = signbit(a_val) ? 1 : 0;
locale_t	l_cloc = (locale_t) cfg$$cloc();
double		l_abs = fabs(a_val);

	if ( a_val == 0.0 )
		{
		s_puts(a_o, l_neg ? "-0.0" : "0.0");
		return;
		}

	s_exact(l_abs, l_exact, &l_nex, &l_dex);

	for ( int l_prec = 1; l_prec <= 17; l_prec++ )
		{
		s_round(l_exact, l_nex, l_dex, l_prec, l_dig, &l_nd, &l_dec);

		/* 0.DIGITSe<dec> - what STRTOD_L() makes of it */
		l_o = 0;
		l_tmp[l_o++] = '0';
		l_tmp[l_o++] = '.';
		memcpy(l_tmp + l_o, l_dig, (size_t) l_nd);
		l_o += l_nd;
		l_tmp[l_o++] = 'e';

		if ( (l_x = l_dec) < 0 )
			{
			l_tmp[l_o++] = '-';
			l_x = -l_x;
			}

		for ( l_e = 0; l_x || !l_e; l_x /= 10 )
			l_exp[l_e++] = (char) ('0' + l_x % 10);

		while ( l_e )
			l_tmp[l_o++] = l_exp[--l_e];

		l_tmp[l_o] = '\0';

		if ( strtod_l(l_tmp, NULL, l_cloc) == l_abs )
			break;
		}

	while ( (l_nd > 1) && (l_dig[l_nd - 1] == '0') )
		l_nd--;

	l_o = 0;

	if ( l_neg )
		l_tmp[l_o++] = '-';

	l_x = l_dec - 1;				/* the exponent of d.ddd			*/

	if ( (l_x >= -5) && (l_x < 16) )
		{
		if ( l_dec <= 0 )
			{
			l_tmp[l_o++] = '0';
			l_tmp[l_o++] = '.';

			for ( int i = 0; i < -l_dec; i++ )
				l_tmp[l_o++] = '0';

			memcpy(l_tmp + l_o, l_dig, (size_t) l_nd);
			l_o += l_nd;
			}
		else if ( l_dec >= l_nd )
			{
			memcpy(l_tmp + l_o, l_dig, (size_t) l_nd);
			l_o += l_nd;

			for ( int i = l_nd; i < l_dec; i++ )
				l_tmp[l_o++] = '0';

			l_tmp[l_o++] = '.';
			l_tmp[l_o++] = '0';
			}
		else	{
			memcpy(l_tmp + l_o, l_dig, (size_t) l_dec);
			l_o += l_dec;
			l_tmp[l_o++] = '.';
			memcpy(l_tmp + l_o, l_dig + l_dec, (size_t) (l_nd - l_dec));
			l_o += l_nd - l_dec;
			}
		}
	else	{
		l_tmp[l_o++] = l_dig[0];

		if ( l_nd > 1 )
			{
			l_tmp[l_o++] = '.';
			memcpy(l_tmp + l_o, l_dig + 1, (size_t) (l_nd - 1));
			l_o += l_nd - 1;
			}

		l_tmp[l_o++] = 'e';
		l_tmp[l_o++] = (l_x < 0) ? '-' : '+';
		l_x = (l_x < 0) ? -l_x : l_x;

		for ( l_e = 0; l_x || (l_e < 2); l_x /= 10 )
			l_exp[l_e++] = (char) ('0' + l_x % 10);

		while ( l_e )
			l_tmp[l_o++] = l_exp[--l_e];
		}

	s_put(a_o, l_tmp, (uint32_t) l_o);
}



/*
**  DESCRIPTION: The text of a scalar value.
*/
static void	s_gen_scalar	(
		CFG$OUT		*a_o,
		CFG$NODE	*a_n
			)
{
char		*l_str;
uint32_t	l_len;

	switch ( a_n->type )
		{
		case CFG$K_TYPE_INT:
		case CFG$K_TYPE_INT64:
			s_gen_int(a_o, a_n);
			break;

		case CFG$K_TYPE_FLOAT:
			s_gen_float(a_o, a_n->v.f);
			break;

		case CFG$K_TYPE_BOOL:
			s_puts(a_o, a_n->v.b ? "true" : "false");
			break;

		case CFG$K_TYPE_STRING:
			if ( !(1 & cfg$$format_string(a_n->v.s.ptr, a_n->v.s.len, &l_str, &l_len)) )
				{
				a_o->sts = CFG$_NOMEM;
				break;
				}

			s_put(a_o, l_str, l_len);
			free(l_str);
			break;
		}
}



/*
**  DESCRIPTION: Indentation one step deeper than A_INDENT, into A_OUT.
*/
static void	s_deeper	(
		CFG$OUT		*a_o,
	const	char		*a_indent,
		char		*a_out
			)
{
size_t	l_len = strlen(a_indent), l_ulen = strlen(a_o->unit);

	if ( l_len + l_ulen >= CFG$K_MAXINDENT )
		l_ulen = 0;

	memcpy(a_out, a_indent, l_len);
	memcpy(a_out + l_len, a_o->unit, l_ulen);
	a_out[l_len + l_ulen] = '\0';
}



/*
**  DESCRIPTION: The canonical text of a node made by the program: "name = value;" for a setting,
**		the value for an element.
*/
static void	s_gen_node	(
		CFG$OUT		*a_o,
		CFG$NODE	*a_n,
	const	char		*a_indent
			)
{
	if ( a_n->name )
		{
		s_put(a_o, a_n->name, a_n->namelen);
		s_puts(a_o, " = ");
		}

	s_gen_value(a_o, a_n, a_indent);

	if ( a_n->name )
		s_puts(a_o, ";");
}



/*
**  DESCRIPTION: The canonical text of a value made by the program.
*/
static void	s_gen_value	(
		CFG$OUT		*a_o,
		CFG$NODE	*a_n,
	const	char		*a_indent
			)
{
char		l_in[CFG$K_MAXINDENT];
CFG$NODE	*l_c;
int		l_flat = 1, l_first = 1;

	if ( $ISSCALAR(a_n->type) )
		{
		s_gen_scalar(a_o, a_n);
		return;
		}

	s_deeper(a_o, a_indent, l_in);

	if ( a_n->type == CFG$K_TYPE_GROUP )
		{
		if ( !a_n->count )
			{
			s_puts(a_o, "{ }");
			return;
			}

		s_puts(a_o, "{\n");

		for ( l_c = a_n->child; l_c; l_c = l_c->next )
			if ( $ISLIVE(l_c) )
				{
				s_puts(a_o, l_in);
				s_gen_node(a_o, l_c, l_in);
				s_puts(a_o, "\n");
				}

		s_puts(a_o, a_indent);
		s_puts(a_o, "}");
		return;
		}

	/* A list or an array: on one line when it holds only scalars */
	for ( l_c = a_n->child; l_c; l_c = l_c->next )
		if ( $ISLIVE(l_c) && !$ISSCALAR(l_c->type) )
			l_flat = 0;

	s_puts(a_o, (a_n->type == CFG$K_TYPE_ARRAY) ? "[" : "(");

	for ( l_c = a_n->child; l_c; l_c = l_c->next )
		{
		if ( !$ISLIVE(l_c) )
			continue;

		if ( l_flat )
			s_puts(a_o, l_first ? " " : ", ");
		else	{
			s_puts(a_o, l_first ? "\n" : ",\n");
			s_puts(a_o, l_in);
			}

		s_gen_value(a_o, l_c, l_in);
		l_first = 0;
		}

	if ( !l_flat )
		{
		s_puts(a_o, "\n");
		s_puts(a_o, a_indent);
		}
	else	s_puts(a_o, " ");

	s_puts(a_o, (a_n->type == CFG$K_TYPE_ARRAY) ? "]" : ")");
}



/*
**  DESCRIPTION: The whitespace at the beginning of the line that holds an offset, into A_OUT.
*/
static void	s_indent_of	(
		CFG$SRC		*a_src,
		uint32_t	a_pos,
		char		*a_out
			)
{
uint32_t	l_ls = a_pos, l_n = 0;

	while ( l_ls && (a_src->buf[l_ls - 1] != '\n') )
		l_ls--;

	while ( (l_ls + l_n < a_src->len) && (l_n < CFG$K_MAXINDENT - 1) && ((a_src->buf[l_ls + l_n] == ' ') || (a_src->buf[l_ls + l_n] == '\t')) )
		l_n++;

	memcpy(a_out, a_src->buf + l_ls, l_n);
	a_out[l_n] = '\0';
}



/*
**  DESCRIPTION: Is there a live element after this one?
*/
static int	s_livenext	(
		CFG$NODE	*a_n
			)
{
	for ( CFG$NODE *l_n = a_n->next; l_n; l_n = l_n->next )
		if ( $ISLIVE(l_n) )
			return	1;

	return	0;
}



/*
**  DESCRIPTION: Has a list or an array lost or gained an element?
*/
static int	s_reshaped	(
		CFG$NODE	*a_n
			)
{
	for ( CFG$NODE *l_c = a_n->child; l_c; l_c = l_c->next )
		if ( l_c->flags & (CFG$M_DELETED | CFG$M_NEW) )
			return	1;

	return	0;
}



static void	s_node	(CFG$OUT *a_o, CFG$NODE *a_n);

/*
**  DESCRIPTION: The value of a node that came from a file.
*/
static void	s_value	(
		CFG$OUT		*a_o,
		CFG$NODE	*a_n
			)
{
CFG$SRC		*l_src = a_o->tree->src[a_n->srcno];
const	char	*l_buf = l_src->buf;
char		l_in[CFG$K_MAXINDENT];
int		l_first = 1, l_pad;

	if ( a_n->flags & CFG$M_MODIFIED )
		{
		s_gen_scalar(a_o, a_n);
		return;
		}

	if ( !(a_n->flags & CFG$M_DIRTY) || $ISSCALAR(a_n->type) || (a_n->type == CFG$K_TYPE_INCLUDE) )
		{
		s_put(a_o, l_buf + a_n->vbeg, a_n->vend - a_n->vbeg);
		return;
		}

	/* A list or an array on one line that lost or gained elements: rebuilt on one line */
	if ( $ISSEQ(a_n->type) && !memchr(l_buf + a_n->vbeg, '\n', a_n->vend - a_n->vbeg) && s_reshaped(a_n) )
		{
		l_pad = (a_n->vend - a_n->vbeg > 2) && (l_buf[a_n->vbeg + 1] == ' ');
		s_indent_of(l_src, a_n->vbeg, l_in);

		s_put(a_o, l_buf + a_n->vbeg, 1);

		for ( CFG$NODE *l_c = a_n->child; l_c; l_c = l_c->next )
			{
			if ( !$ISLIVE(l_c) )
				continue;

			s_puts(a_o, l_first ? (l_pad ? " " : "") : ", ");
			l_first = 0;

			if ( l_c->flags & CFG$M_NEW )
				s_gen_value(a_o, l_c, l_in);
			else	s_value(a_o, l_c);
			}

		if ( l_pad && !l_first )
			s_puts(a_o, " ");

		s_put(a_o, l_buf + a_n->vend - 1, 1);
		return;
		}

	/* A container with changes below: its brackets, the inside walked */
	s_put(a_o, l_buf + a_n->vbeg, 1);
	s_children(a_o, a_n, a_n->srcno, a_n->vbeg + 1, a_n->vend - 1);
	s_put(a_o, l_buf + a_n->vend - 1, 1);
}



/*
**  DESCRIPTION: A node that came from a file: leading comments and head as they stood, the value,
**		the tail with the ',' of an element fixed up.
*/
static void	s_node	(
		CFG$OUT		*a_o,
		CFG$NODE	*a_n
			)
{
const	char	*l_buf = a_o->tree->src[a_n->srcno]->buf;
int		l_need;

	s_put(a_o, l_buf + a_n->lead, a_n->vbeg - a_n->lead);
	s_value(a_o, a_n);

	if ( !a_n->parent || !$ISSEQ(a_n->parent->type) )
		{
		s_put(a_o, l_buf + a_n->vend, a_n->end - a_n->vend);
		return;
		}

	/* An element: a ',' when a live one follows, none after the last */
	l_need = s_livenext(a_n);

	if ( (a_n->sep != CFG$K_NOSEP) && !l_need )
		{
		s_put(a_o, l_buf + a_n->vend, a_n->sep - a_n->vend);
		s_put(a_o, l_buf + a_n->sep + 1, a_n->end - a_n->sep - 1);
		}
	else if ( (a_n->sep == CFG$K_NOSEP) && l_need )
		{
		s_puts(a_o, ",");
		s_put(a_o, l_buf + a_n->vend, a_n->end - a_n->vend);
		}
	else	s_put(a_o, l_buf + a_n->vend, a_n->end - a_n->vend);
}



/*
**  DESCRIPTION: A node made by the program, among nodes from a file.
**
**  INPUTS:
**	a_o:		the output
**	a_n:		the node
**	a_ml:		the container spreads over lines - the node gets a line of its own
**	a_indent:	indentation for it
*/
static void	s_newnode	(
		CFG$OUT		*a_o,
		CFG$NODE	*a_n,
		int		a_ml,
	const	char		*a_indent
			)
{
int	l_nl = 0;
char	l_c = s_lastc(a_o);

	if ( a_ml )
		{
		if ( l_c != '\n' )
			{
			s_puts(a_o, "\n");
			l_nl = 1;
			}

		s_puts(a_o, a_indent);
		}
	else if ( (l_c != ' ') && (l_c != '\t') && (l_c != '{') && (l_c != '(') && (l_c != '[') )
		s_puts(a_o, " ");

	s_gen_node(a_o, a_n, a_indent);

	if ( a_n->parent && $ISSEQ(a_n->parent->type) && s_livenext(a_n) )
		s_puts(a_o, ",");

	if ( a_ml && !l_nl )
		s_puts(a_o, "\n");
}



/*
**  DESCRIPTION: The children of a container that belong to a source, with the text between them.
**
**  INPUTS:
**	a_o:		the output
**	a_cont:		the container
**	a_srcno:	the source being written
**	a_cur,a_end:	the inside of the container in that source (the whole file for a root)
*/
static void	s_children	(
		CFG$OUT		*a_o,
		CFG$NODE	*a_cont,
		uint32_t	a_srcno,
		uint32_t	a_cur,
		uint32_t	a_end
			)
{
CFG$SRC		*l_src = a_o->tree->src[a_srcno];
const	char	*l_buf = l_src->buf;
char		l_indent[CFG$K_MAXINDENT], l_base[CFG$K_MAXINDENT];
int		l_ml, l_have = 0;

	/* Spread over lines: a file, or brackets with a newline between them */
	l_ml = !a_cont->parent || (a_cont->srcno != a_srcno) || (a_end > a_cur && memchr(l_buf + a_cur, '\n', a_end - a_cur));

	for ( CFG$NODE *l_c = a_cont->child; l_c; l_c = l_c->next )
		{
		if ( l_c->srcno != a_srcno )
			continue;

		if ( l_c->flags & CFG$M_NEW )
			{
			if ( !l_have )
				{
				/* No sibling from the file to copy the indentation from */
				if ( a_cont->parent && (a_cont->srcno == a_srcno) )
					{
					s_indent_of(l_src, a_cont->name ? a_cont->beg : a_cont->vbeg, l_base);
					s_deeper(a_o, l_base, l_indent);
					}
				else	l_indent[0] = '\0';

				l_have = 1;
				}

			s_newnode(a_o, l_c, l_ml, l_indent);
			continue;
			}

		s_put(a_o, l_buf + a_cur, l_c->lead - a_cur);
		a_cur = l_c->end;

		if ( l_c->flags & CFG$M_DELETED )
			{
			/* Out of a line it shared: the blanks after it go too */
			if ( (a_cur > 0) && (l_buf[a_cur - 1] != '\n') )
				while ( (a_cur < a_end) && ((l_buf[a_cur] == ' ') || (l_buf[a_cur] == '\t')) )
					a_cur++;

			continue;
			}

		s_node(a_o, l_c);

		s_indent_of(l_src, l_c->beg, l_indent);
		l_have = 1;
		}

	s_put(a_o, l_buf + a_cur, a_end - a_cur);
}



/*
**  DESCRIPTION: One step of indentation as the source uses it: a tab, or the spaces of the
**		first indented line.
*/
static void	s_unit	(
		CFG$OUT		*a_o,
		CFG$SRC		*a_src
			)
{
const	char	*l_buf = a_src->buf;

	strcpy(a_o->unit, "\t");

	for ( uint32_t l_p = 0; l_p < a_src->len; l_p++ )
		{
		if ( (l_p && (l_buf[l_p - 1] != '\n')) || ((l_buf[l_p] != ' ') && (l_buf[l_p] != '\t')) )
			continue;

		if ( l_buf[l_p] == '\t' )
			return;

		{
		uint32_t	l_n = 0;

		while ( (l_p + l_n < a_src->len) && (l_buf[l_p + l_n] == ' ') && (l_n < 8) )
			l_n++;

		if ( (l_p + l_n < a_src->len) && (l_buf[l_p + l_n] != '\n') && (l_buf[l_p + l_n] != '\r') )
			{
			memset(a_o->unit, ' ', l_n);
			a_o->unit[l_n] = '\0';
			return;
			}
		}
		}
}



/*
**  DESCRIPTION: Find the @include node that brought a source.
*/
static CFG$NODE *	s_find_include	(
		CFG$NODE	*a_n,
		uint32_t	a_srcno
			)
{
CFG$NODE	*l_r;

	for ( CFG$NODE *l_c = a_n->child; l_c; l_c = l_c->next )
		{
		if ( (l_c->type == CFG$K_TYPE_INCLUDE) && (l_c->incsrc == a_srcno) )
			return	l_c;

		if ( (l_c->type == CFG$K_TYPE_GROUP) && (l_r = s_find_include(l_c, a_srcno)) )
			return	l_r;
		}

	return	NULL;
}



/*
**  DESCRIPTION: The text of one source of a tree.
**
**  INPUTS:
**	a_tree:		the tree
**	a_srcno:	the source: 0 - the main file, then the included ones in the order met
**
**  OUTPUTS:
**	a_buf,a_len:	the text, malloc()-ed, zero-terminated - free() it
**
**  RETURNS:
**	STS$K_SUCCESS, CFG$_BADARG, CFG$_NOMEM
*/
unsigned	cfg$write_buf	(
		CFG$TREE	*a_tree,
		uint32_t	a_srcno,
		char		**a_buf,
		uint32_t	*a_len
			)
{
CFG$OUT		l_o;
CFG$NODE	*l_cont = NULL, *l_inc;

	if ( !a_tree || !a_buf || !a_len || (a_srcno >= a_tree->nsrc) )
		return	CFG$_BADARG;

	memset(&l_o, 0, sizeof(l_o));
	l_o.sts = STS$K_SUCCESS;
	l_o.tree = a_tree;

	s_unit(&l_o, a_tree->src[a_srcno]);

	if ( !a_srcno )
		l_cont = a_tree->root;
	else if ( (l_inc = s_find_include(a_tree->root, a_srcno)) )
		l_cont = l_inc->parent;

	if ( !l_cont )
		return	CFG$_BADARG;

	s_children(&l_o, l_cont, a_srcno, 0, a_tree->src[a_srcno]->len);

	if ( !(1 & l_o.sts) )
		{
		free(l_o.p);
		return	l_o.sts;
		}

	/* An empty source - an empty, but valid, string */
	if ( !l_o.p && !(l_o.p = calloc(1, 1)) )
		return	CFG$_NOMEM;

	*a_buf = l_o.p;
	*a_len = l_o.len;

	return	STS$K_SUCCESS;
}



/*
**  DESCRIPTION: Write a text into a file in its place: a temporary file in the same directory,
**		synced, then renamed over the old one; the mode of the old file is kept.
**
**  RETURNS:
**	STS$K_SUCCESS, CFG$_WRITERR (recorded in the tree), CFG$_NOMEM
*/
static unsigned	s_save	(
		CFG$TREE	*a_tree,
	const	char		*a_path,
	const	char		*a_buf,
		uint32_t	a_len
			)
{
char		l_tmp[PATH_MAX];
int		l_fd, l_err;
ssize_t		l_rc;
uint32_t	l_done = 0;
struct stat	l_st;

static	const char	s_sfx[] = ".yalibcfg-XXXXXX";
size_t			l_plen = strlen(a_path);

	if ( l_plen + sizeof(s_sfx) > sizeof(l_tmp) )
		return	cfg$$syserr(a_tree, CFG$_WRITERR, a_path, ENAMETOOLONG);

	memcpy(l_tmp, a_path, l_plen);
	memcpy(l_tmp + l_plen, s_sfx, sizeof(s_sfx));

	if ( 0 > (l_fd = mkostemp(l_tmp, O_CLOEXEC)) )
		return	cfg$$syserr(a_tree, CFG$_WRITERR, a_path, errno);

	if ( !stat(a_path, &l_st) )
		fchmod(l_fd, l_st.st_mode & 07777);
	else	fchmod(l_fd, 0644);

	while ( l_done < a_len )
		{
		if ( 0 > (l_rc = write(l_fd, a_buf + l_done, a_len - l_done)) )
			{
			if ( errno == EINTR )
				continue;

			goto	l_fail;
			}

		l_done += (uint32_t) l_rc;
		}

	if ( fsync(l_fd) )
		goto	l_fail;

	if ( close(l_fd) )
		{
		l_fd = -1;
		goto	l_fail;
		}

	l_fd = -1;

	if ( rename(l_tmp, a_path) )
		goto	l_fail;

	return	STS$K_SUCCESS;

l_fail:
	l_err = errno;

	if ( l_fd >= 0 )
		close(l_fd);

	unlink(l_tmp);

	return	cfg$$syserr(a_tree, CFG$_WRITERR, a_path, l_err);
}



/*
**  DESCRIPTION: Write back every source of the tree whose text has changed, each into its own
**		file.
**
**  RETURNS:
**	STS$K_SUCCESS, CFG$_WRITERR, CFG$_NOMEM, CFG$_BADARG
*/
unsigned	cfg$write	(
		CFG$TREE	*a_tree
			)
{
char		*l_buf;
uint32_t	l_len;
unsigned	l_sts;

	if ( !a_tree )
		return	CFG$_BADARG;

	for ( uint32_t i = 0; i < a_tree->nsrc; i++ )
		{
		if ( !(1 & (l_sts = cfg$write_buf(a_tree, i, &l_buf, &l_len))) )
			return	l_sts;

		if ( (l_len != a_tree->src[i]->len) || memcmp(l_buf, a_tree->src[i]->buf, l_len) )
			l_sts = s_save(a_tree, a_tree->src[i]->path, l_buf, l_len);

		free(l_buf);

		if ( !(1 & l_sts) )
			return	l_sts;
		}

	return	STS$K_SUCCESS;
}



/*
**  DESCRIPTION: Write the main source of the tree into another file; @include directives stay
**		as they are.
**
**  RETURNS:
**	STS$K_SUCCESS, CFG$_WRITERR, CFG$_NOMEM, CFG$_BADARG
*/
unsigned	cfg$write_file	(
		CFG$TREE	*a_tree,
	const	char		*a_path
			)
{
char		*l_buf;
uint32_t	l_len;
unsigned	l_sts;

	if ( !a_tree || !a_path )
		return	CFG$_BADARG;

	if ( !(1 & (l_sts = cfg$write_buf(a_tree, 0, &l_buf, &l_len))) )
		return	l_sts;

	l_sts = s_save(a_tree, a_path, l_buf, l_len);
	free(l_buf);

	return	l_sts;
}
