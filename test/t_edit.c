/*
**  yaLIBCFG test: changes through the API and the text written back.
*/
#include "t_util.h"
#include <math.h>

static CFG$TREE *	p (const char *txt, uint32_t opts)
{
	CFG$TREE	*t;
	unsigned	sts = cfg$parse_buf(txt, (uint32_t) strlen(txt), "<test>", opts, &t);

	if ( !(sts & 1) )
		{
		fprintf(stderr, "parse of the test input failed: sts=%08X\n---\n%s\n---\n", sts, txt);
		cfg$puterrs(t);
		}

	return t;
}

static void	expect (CFG$TREE *t, uint32_t src, const char *want, int line)
{
	char		*out;
	uint32_t	len;

	g_total++;

	if ( !(cfg$write_buf(t, src, &out, &len) & 1) )
		{
		g_fail++;
		fprintf(stderr, "FAIL line %d: write_buf failed\n", line);
		return;
		}

	if ( (len != strlen(want)) || memcmp(out, want, len) )
		{
		g_fail++;
		fprintf(stderr, "FAIL line %d:\n--- want\n%s\n--- got\n%s\n---\n", line, want, out);
		}

	free(out);
}

#define	EXPECT(t, want)		expect(t, 0, want, __LINE__)
#define	OK(x)			CHECK((x) & 1, "%s failed", #x)

int	main (int argc, char **argv)
{
	CFG$TREE	*t;
	CFG$NODE	*r, *n;
	int32_t		i32;
	const char	*s;
	uint32_t	l;

	cfg$inimsg();

	/* A value changes, the comment stays */
	t = p("a = 1; # c\nb = 2;\n", 0); cfg$root(t, &r);
	OK(cfg$set_int(r, "a", 5));
	EXPECT(t, "a = 5; # c\nb = 2;\n");
	cfg$free(t);

	/* A hexadecimal stays hexadecimal */
	t = p("m = 0x1F;\n", 0); cfg$root(t, &r);
	OK(cfg$set_int(r, "m", 255));
	EXPECT(t, "m = 0xFF;\n");
	cfg$free(t);

	/* Removed with its comment */
	t = p("# about a\na = 1;\n\n# about b\nb = 2;\n", 0); cfg$root(t, &r);
	OK(cfg$lookup(r, "a", &n)); OK(cfg$delete(n));
	EXPECT(t, "\n# about b\nb = 2;\n");
	cfg$free(t);

	/* Added at the end of the file */
	t = p("a = 1;\n", 0); cfg$root(t, &r);
	OK(cfg$add(r, "b", CFG$K_TYPE_INT, &n)); OK(cfg$set_int(n, "", 7));
	EXPECT(t, "a = 1;\nb = 7;\n");
	cfg$free(t);

	/* Into a group over lines, with the indentation of the siblings */
	t = p("g = {\n    x = 1;\n};\n", 0); cfg$root(t, &r);
	OK(cfg$set_string(r, "g.y", "s", 1));
	EXPECT(t, "g = {\n    x = 1;\n    y = \"s\";\n};\n");
	cfg$free(t);

	/* Into an empty group over lines */
	t = p("g = {\n};\n", 0); cfg$root(t, &r);
	OK(cfg$set_int(r, "g.x", 1));
	EXPECT(t, "g = {\n\tx = 1;\n};\n");
	cfg$free(t);

	/* Into a group on one line */
	t = p("g = { a = 1; };\n", 0); cfg$root(t, &r);
	OK(cfg$set_int(r, "g.b", 2));
	EXPECT(t, "g = { a = 1; b = 2; };\n");
	cfg$free(t);

	/* Out of a group on one line */
	t = p("g = { a = 1; b = 2; };\n", 0); cfg$root(t, &r);
	OK(cfg$lookup(r, "g.a", &n)); OK(cfg$delete(n));
	EXPECT(t, "g = { b = 2; };\n");
	cfg$free(t);

	/* Arrays */
	t = p("v = [1, 2];\n", 0); cfg$root(t, &r);
	OK(cfg$lookup(r, "v", &n)); OK(cfg$add(n, NULL, CFG$K_TYPE_INT, &n)); OK(cfg$set_int(n, "", 3));
	EXPECT(t, "v = [1, 2, 3];\n");
	cfg$free(t);

	t = p("v = [ 1, 2, 3 ];\n", 0); cfg$root(t, &r);
	OK(cfg$lookup(r, "v.[1]", &n)); OK(cfg$delete(n));
	EXPECT(t, "v = [ 1, 3 ];\n");
	cfg$free(t);

	t = p("v = [1, 2, 3];\n", 0); cfg$root(t, &r);
	OK(cfg$lookup(r, "v.[2]", &n)); OK(cfg$delete(n));
	EXPECT(t, "v = [1, 2];\n");
	CHECK(cfg$add(r, NULL, CFG$K_TYPE_INT, &n) == CFG$_BADARG, "add without a name into a group");
	cfg$free(t);

	t = p("v = [1, 2];\n", 0); cfg$root(t, &r);
	OK(cfg$lookup(r, "v", &n));
	CHECK(cfg$add(n, NULL, CFG$K_TYPE_STRING, NULL) == CFG$_TYPE, "a string into an array of integers");
	cfg$free(t);

	/* A list of groups over lines */
	t = p("ports = (\n\t{ id = 1; },\n\t{ id = 2; }\n);\n", 0); cfg$root(t, &r);
	OK(cfg$lookup(r, "ports.[id=2]", &n)); OK(cfg$delete(n));
	EXPECT(t, "ports = (\n\t{ id = 1; }\n);\n");
	cfg$free(t);

	t = p("ports = (\n\t{ id = 1; }\n);\n", 0); cfg$root(t, &r);
	OK(cfg$lookup(r, "ports", &n)); OK(cfg$add(n, NULL, CFG$K_TYPE_GROUP, &n)); OK(cfg$set_int(n, "id", 2));
	EXPECT(t, "ports = (\n\t{ id = 1; },\n\t{\n\t\tid = 2;\n\t}\n);\n");
	cfg$free(t);

	/* Groups made along a path */
	t = p("a = 1;\n", 0); cfg$root(t, &r);
	OK(cfg$set_string(r, "x.y.z", "v", 1));
	EXPECT(t, "a = 1;\nx = {\n\ty = {\n\t\tz = \"v\";\n\t};\n};\n");
	cfg$free(t);

	/* Escapes on the way out */
	t = p("s = \"x\";\n", 0); cfg$root(t, &r);
	OK(cfg$set_string(r, "s", "a\"b\n\x01", 5));
	EXPECT(t, "s = \"a\\\"b\\n\\x01\";\n");
	cfg$free(t);

	/* Floats */
	t = p("f = 1.5;\ng = 2.0;\nh = 0.0;\n", 0); cfg$root(t, &r);
	OK(cfg$set_float(r, "f", 0.1)); OK(cfg$set_float(r, "g", 3.0)); OK(cfg$set_float(r, "h", 1e20));
	EXPECT(t, "f = 0.1;\ng = 3.0;\nh = 1e+20;\n");
	CHECK(cfg$set_float(r, "f", 1.0 / 0.0) == CFG$_BADARG, "inf refused");
	cfg$free(t);

	/* Floats: the text read back is the same double */
	{
		double	v[] = { 0.1, -0.1, 1.0/3, 2.0/3, 1e-7, 1.5e-5, 123456.789, 1e15, 1e16, 1.7976931348623157e308, 1.7976931348623155e308, 2.2250738585072014e-308, 2.2250738585072009e-308, 5e-324, 1e23, 9007199254740993.0, 0.1 + 0.2, -2.5e10, 100.0, 0.30000000000000004 };

		for ( unsigned k = 0; k < sizeof(v) / sizeof(v[0]); k++ )
			{
			char		*out;
			uint32_t	len;
			double		back;

			t = p("f = 1.0;\n", 0); cfg$root(t, &r);
			OK(cfg$set_float(r, "f", v[k]));
			OK(cfg$write_buf(t, 0, &out, &len));
			cfg$free(t);

			t = p(out, 0); cfg$root(t, &r);
			OK(cfg$get_float(r, "f", &back));
			CHECK(back == v[k], "float %.17g written as %s", v[k], out);
			free(out);
			cfg$free(t);
			}
	}
	t = p("a = 0.0;\nb = 0.0;\nc = 0.0;\nd = 0.0;\n", 0); cfg$root(t, &r);
	OK(cfg$set_float(r, "a", 1e-5)); OK(cfg$set_float(r, "b", 1e-6)); OK(cfg$set_float(r, "c", 1234567890123456.0)); OK(cfg$set_float(r, "d", -2.5e16));
	EXPECT(t, "a = 0.00001;\nb = 1e-06;\nc = 1234567890123456.0;\nd = -2.5e+16;\n");
	cfg$free(t);

	/* Floats at random: 20000 bit patterns, every finite one must come back the same */
	{
		uint64_t	x = 88172645463325252ULL;
		char		in[64];

		for ( int k = 0; k < 20000; k++ )
			{
			double		v, back;
			char		*out;
			uint32_t	len;

			x ^= x << 13; x ^= x >> 7; x ^= x << 17;
			memcpy(&v, &x, sizeof(v));

			if ( !isfinite(v) )
				continue;

			t = p("f = 1.0;\n", 0); cfg$root(t, &r);
			OK(cfg$set_float(r, "f", v));
			OK(cfg$write_buf(t, 0, &out, &len));
			cfg$free(t);

			snprintf(in, sizeof(in), "%.*s", (int) len, out);
			t = p(in, 0); cfg$root(t, &r);
			back = 0;
			cfg$get_float(r, "f", &back);
			if ( memcmp(&back, &v, sizeof(v)) ) { g_fail++; fprintf(stderr, "FAIL random float %.17g written as %s", v, out); }
			g_total++;
			free(out);
			cfg$free(t);
			}
	}

	/* Decimal integers */
	t = p("a = 0;\nb = 0;\nc = 0;\n", 0); cfg$root(t, &r);
	OK(cfg$set_int(r, "a", -2147483647 - 1)); OK(cfg$set_int64(r, "b", INT64_MIN)); OK(cfg$set_int(r, "c", 0));
	EXPECT(t, "a = -2147483648;\nb = -9223372036854775808L;\nc = 0;\n");
	cfg$free(t);

	/* An integer grows to 64 bits */
	t = p("n = 1;\n", 0); cfg$root(t, &r);
	OK(cfg$set_int64(r, "n", 5000000000LL));
	EXPECT(t, "n = 5000000000L;\n");
	CHECK(cfg$get_int(r, "n", &i32) == CFG$_TYPE, "get_int of a 64-bit value");
	cfg$free(t);

	/* CRLF lines stay */
	t = p("a = 1;\r\nb = 2;\r\n", 0); cfg$root(t, &r);
	OK(cfg$set_int(r, "a", 3));
	EXPECT(t, "a = 3;\r\nb = 2;\r\n");
	cfg$free(t);

	/* Removed, then added again: goes to the end */
	t = p("a = 1;\nb = 2;\n", 0); cfg$root(t, &r);
	OK(cfg$lookup(r, "a", &n)); OK(cfg$delete(n)); OK(cfg$set_int(r, "a", 5));
	EXPECT(t, "b = 2;\na = 5;\n");
	cfg$free(t);

	/* Refusals */
	t = p("a = 1;\ng = { };\n", 0); cfg$root(t, &r);
	CHECK(cfg$add(r, "a", CFG$K_TYPE_INT, NULL) == CFG$_EXISTS, "duplicate add");
	CHECK(cfg$add(r, "9x", CFG$K_TYPE_INT, NULL) == CFG$_BADNAME, "bad name");
	CHECK(cfg$set_string(r, "a", "x", 1) == CFG$_TYPE, "string into an integer");
	CHECK(cfg$set_int(r, "g.[0]", 1) == CFG$_NOTFOUND, "an element is never made");
	CHECK(cfg$lookup(r, "a.[", &n) == CFG$_BADPATH, "bad path");
	CHECK(cfg$delete(r) == CFG$_BADARG, "root not removable");
	EXPECT(t, "a = 1;\ng = { };\n");
	cfg$free(t);

	/* Lookups */
	t = p("ports = ( { id = 1; name = \"mgmt\"; }, { id = 9; mtu = 9216; } );\nAlpha = true;\n", CFG$M_NOCASE); cfg$root(t, &r);
	OK(cfg$get_int(r, "ports.[id=9].mtu", &i32)); CHECK(i32 == 9216, "mtu %d", i32);
	OK(cfg$get_string(r, "ports.[name=\"mgmt\"].name", &s, &l)); CHECK((l == 4) && !memcmp(s, "mgmt", 4), "name");
	OK(cfg$get_int(r, "ports/[0]/id", &i32)); CHECK(i32 == 1, "id %d", i32);
	{ int b = 0; OK(cfg$get_bool(r, "ALPHA", &b)); CHECK(b == 1, "nocase"); }
	CHECK(cfg$get_int(r, "ports.[id=3].mtu", &i32) == CFG$_NOTFOUND, "no such key");
	cfg$free(t);

	return t_report("t_edit");
}
