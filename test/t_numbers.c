/*
**  yaLIBCFG test: the forms of integers - octal with a leading 0 as libconfig reads it, the radix
**  of DCL (%X %D %B %O), the form kept when a value is set; '!' comments.
*/
#include "t_util.h"
#include "../src/cfg_int.h"

/* One setting "a = <txt>;": the status, the type, the value */
static void	num (const char *txt, unsigned wsts, uint32_t wtype, int64_t wval, int line)
{
	char		buf[128];
	CFG$TREE	*t;
	CFG$NODE	*r, *n;
	unsigned	sts;

	snprintf(buf, sizeof(buf), "a = %s;\n", txt);
	sts = cfg$parse_buf(buf, (uint32_t) strlen(buf), "<n>", 0, &t);
	g_total++;

	if ( sts != wsts )
		{
		g_fail++;
		fprintf(stderr, "FAIL line %d: %s: sts %08X want %08X\n", line, txt, sts, wsts);
		}
	else if ( 1 & sts )
		{
		cfg$root(t, &r);
		cfg$lookup(r, "a", &n);
		g_total++;

		if ( (n->type != wtype) || ((wtype != CFG$K_TYPE_FLOAT) && (n->v.i != wval)) || ((wtype == CFG$K_TYPE_FLOAT) && (n->v.f != (double) wval / 10)) )
			{
			g_fail++;
			fprintf(stderr, "FAIL line %d: %s: type %u value %lld\n", line, txt, n->type, (long long) n->v.i);
			}
		}

	cfg$free(t);
}

/* "a = <txt>;" set to a value: the text written */
static void	set (const char *txt, int wide, int64_t val, const char *want, int line)
{
	char		buf[128], *out;
	uint32_t	len;
	CFG$TREE	*t;
	CFG$NODE	*r;

	snprintf(buf, sizeof(buf), "a = %s;\n", txt);
	cfg$parse_buf(buf, (uint32_t) strlen(buf), "<s>", 0, &t);
	cfg$root(t, &r);

	if ( wide )
		cfg$set_int64(r, "a", val);
	else	cfg$set_int(r, "a", (int32_t) val);

	cfg$write_buf(t, 0, &out, &len);
	snprintf(buf, sizeof(buf), "a = %s;\n", want);
	g_total++;

	if ( strcmp(out, buf) )
		{
		g_fail++;
		fprintf(stderr, "FAIL line %d: %s set to %lld: got [%s] want [%s]\n", line, txt, (long long) val, out, buf);
		}

	free(out);
	cfg$free(t);

	/* And it reads back as the value set */
	cfg$parse_buf(buf, (uint32_t) strlen(buf), "<s>", 0, &t);
	{
		int64_t	v = 0;

		cfg$root(t, &r);
		g_total++;

		if ( !(1 & cfg$get_int64(r, "a", &v)) || (v != val) )
			{
			g_fail++;
			fprintf(stderr, "FAIL line %d: %s read back as %lld, want %lld\n", line, buf, (long long) v, (long long) val);
			}
	}
	cfg$free(t);
}

#define	NUM(t, s, ty, v)	num(t, s, ty, v, __LINE__)
#define	SET(t, w, v, r)		set(t, w, v, r, __LINE__)
#define	I32			CFG$K_TYPE_INT
#define	I64			CFG$K_TYPE_INT64
#define	FLT			CFG$K_TYPE_FLOAT

int	main (void)
{
	cfg$inimsg();

	/* A leading 0: octal, as libconfig 1.6 and later reads it */
	NUM("0027", CFG$_NORMAL, I32, 23);
	NUM("-027", CFG$_NORMAL, I32, -23);
	NUM("+027", CFG$_NORMAL, I32, 23);
	NUM("00", CFG$_NORMAL, I32, 0);
	NUM("0", CFG$_NORMAL, I32, 0);
	NUM("09", CFG$_BADNUM, 0, 0);
	NUM("0027L", CFG$_NORMAL, I64, 27);		/* libconfig: decimal with the L	*/
	NUM("08L", CFG$_NORMAL, I64, 8);
	NUM("037777777777", CFG$_NORMAL, I64, 4294967295LL);	/* past 32 bits: 64-bit, no sign bit	*/
	NUM("012.5", CFG$_NORMAL, FLT, 125);		/* a float stays decimal		*/
	NUM("01e1", CFG$_NORMAL, FLT, 100);

	/* The radix of DCL */
	NUM("%X1F", CFG$_NORMAL, I32, 31);
	NUM("%x1f", CFG$_NORMAL, I32, 31);
	NUM("%D27", CFG$_NORMAL, I32, 27);
	NUM("%d0027", CFG$_NORMAL, I32, 27);		/* %D: decimal, leading 0s or not	*/
	NUM("%B101", CFG$_NORMAL, I32, 5);
	NUM("%O27", CFG$_NORMAL, I32, 23);
	NUM("-%X10", CFG$_NORMAL, I32, -16);
	NUM("%XFFFFFFFF", CFG$_NORMAL, I32, -1);	/* as 0xFFFFFFFF			*/
	NUM("%X1FL", CFG$_NORMAL, I64, 31);
	NUM("%D5000000000", CFG$_NORMAL, I64, 5000000000LL);
	NUM("%Q1", CFG$_BADNUM, 0, 0);
	NUM("%X", CFG$_BADNUM, 0, 0);
	NUM("%D1.5", CFG$_BADNUM, 0, 0);
	NUM("%B102", CFG$_BADNUM, 0, 0);
	NUM("%O8", CFG$_BADNUM, 0, 0);
	NUM("%", CFG$_BADNUM, 0, 0);
	NUM("0x%X1F", CFG$_BADNUM, 0, 0);		/* one radix only			*/
	NUM("0b%D99", CFG$_BADNUM, 0, 0);

	/* A bad number is told whole */
	{
		const char	*txt = "c = 0x%X1F;\n";
		CFG$TREE	*t;

		cfg$parse_buf(txt, (uint32_t) strlen(txt), "<b>", 0, &t);
		CHECK((t->nerr == 1) && (t->err[0].len == 6) && !memcmp(t->err[0].text, "0x%X1F", 6), "the whole bad number: [%.*s]", (int) t->err[0].len, t->err[0].text);
		cfg$free(t);
	}

	/* The other forms, as before */
	NUM("0x1F", CFG$_NORMAL, I32, 31);
	NUM("0b101", CFG$_NORMAL, I32, 5);
	NUM("0o27", CFG$_NORMAL, I32, 23);
	NUM("27", CFG$_NORMAL, I32, 27);

	/* A value set keeps the form of the text */
	SET("0027", 0, 8, "0010");			/* the width of the text is kept	*/
	SET("0027", 0, 0, "0000");
	SET("-027", 0, -8, "-010");
	SET("07", 0, 0, "00");				/* 0 in octal: "00", not "0" - decimal	*/
	SET("0027", 1, 5, "0005");			/* fits 32 bits: stays a 32-bit octal	*/
	SET("0027L", 1, 5, "5L");			/* 64-bit: the L, decimal as read	*/
	SET("037777777777", 1, 68719476735LL, "0777777777777");
	SET("%X1F", 0, 255, "%XFF");
	SET("%x1f", 0, 255, "%XFF");
	SET("%D27", 0, -3, "-%D03");
	SET("%B101", 0, 6, "%B110");
	SET("%O27", 0, 8, "%O10");
	SET("%X1F", 1, 4294967296LL, "%X100000000L");
	SET("0x1F", 0, 255, "0xFF");
	SET("27", 0, -4, "-4");

	/* The width of the digits is kept; a wider value takes the room it needs */
	SET("%X0001", 0, 2, "%X0002");
	SET("%X0001", 0, 0x12345, "%X12345");
	SET("0x00FF", 0, 1, "0x0001");
	SET("%B0001", 0, 4, "%B0100");
	SET("%D0129", 0, 7, "%D0007");
	SET("%D0129", 0, -7, "-%D0007");
	SET("%O0777", 0, 8, "%O0010");
	SET("00777", 0, 8, "00010");			/* the leading 0 is the prefix		*/
	SET("0b0000L", 1, 3, "0b0011L");
	SET("129", 0, 5, "5");				/* decimal: never a leading 0		*/

	/* '!' comments to the end of the line, as in DCL; not inside a string */
	{
		const char	*txt = "! the head\na = 1; ! after\nb = \"x!y\";\n!c = 3;\n";
		CFG$TREE	*t;
		CFG$NODE	*r;
		const char	*s;
		uint32_t	sl;
		char		*out;
		uint32_t	len;

		CHECK(cfg$parse_buf(txt, (uint32_t) strlen(txt), "<c>", 0, &t) == CFG$_NORMAL, "! comments");
		cfg$root(t, &r);
		CHECK((cfg$get_string(r, "b", &s, &sl) & 1) && (sl == 3) && !memcmp(s, "x!y", 3), "! inside a string");
		CHECK(!(cfg$lookup(r, "c", &r) & 1), "!c = 3 is a comment");
		cfg$root(t, &r);
		CHECK((cfg$lookup(r, "a", &r) & 1) && (cfg$delete(r) & 1), "delete a");
		CHECK((cfg$write_buf(t, 0, &out, &len) & 1) && !strcmp(out, "b = \"x!y\";\n!c = 3;\n"), "the comments of a go with it: [%s]", out);
		free(out);
		cfg$free(t);
	}

	return t_report("t_numbers");
}
