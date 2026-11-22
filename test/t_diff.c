/*
**  yaLIBCFG test: comparison of trees and the fingerprint.
*/
#include "t_util.h"

static CFG$TREE *	p (const char *txt, uint32_t opts)
{
	CFG$TREE	*t;

	if ( !(cfg$parse_buf(txt, (uint32_t) strlen(txt), "<test>", opts, &t) & 1) )
		cfg$puterrs(t);

	return t;
}

/* "OP path" lines of a comparison */
static char *	ops (const char *a, const char *b, uint32_t opts)
{
	CFG$TREE	*x = p(a, 0), *y = p(b, 0);
	CFG$DIFF	*d;
	char		*buf = NULL;
	size_t		len = 0;
	FILE		*f = open_memstream(&buf, &len);
	uint32_t	n = 0, op;
	const char	*path;

	if ( cfg$diff(x, y, NULL, opts, &d) & 1 )
		{
		cfg$diff_count(d, &n);

		for ( uint32_t i = 0; i < n; i++ )
			{
			cfg$diff_entry(d, i, &op, &path, NULL, NULL);
			fprintf(f, "%s %s\n", op == CFG$K_DIFF_MOD ? "MOD" : op == CFG$K_DIFF_ADD ? "ADD" : "DEL", path);
			}

		cfg$diff_free(d);
		}

	fclose(f);
	cfg$free(x);
	cfg$free(y);
	return buf;
}

static void	same_fp (const char *a, uint32_t oa, const char *b, uint32_t ob, int want, int line)
{
	CFG$TREE	*x = p(a, oa), *y = p(b, ob);
	uint8_t		u1[16], u2[16];

	g_total++;
	cfg$fingerprint(x, u1);
	cfg$fingerprint(y, u2);

	if ( (memcmp(u1, u2, 16) == 0) != want )
		{
		g_fail++;
		fprintf(stderr, "FAIL line %d: fingerprints %s\n", line, want ? "differ" : "are the same");
		}

	if ( ((u1[6] >> 4) != 5) || ((u1[8] & 0xC0) != 0x80) )
		{
		g_fail++;
		fprintf(stderr, "FAIL line %d: not a version 5 UUID\n", line);
		}

	cfg$free(x);
	cfg$free(y);
}

#define	OK(x)			CHECK((x) & 1, "%s failed", #x)
#define	OPS(a, b, opts, want)	do { char *s = ops(a, b, opts); CHECK(!strcmp(s, want), "\n--- want\n%s--- got\n%s---", want, s); free(s); } while (0)
#define	FP(a, oa, b, ob, want)	same_fp(a, oa, b, ob, want, __LINE__)

int	main (void)
{
	cfg$inimsg();

	/* No differences: comments, layout, order in a group */
	OPS("a = 1; # x\nb = { c = 2; d = 3; };", "b = {\n d = 3;\n c = 2;\n};\n# other\na = 1;", 0, "");

	/* Integers of both sizes compare by value */
	OPS("a = 5;", "a = 5L;", 0, "");

	/* Scalars, a type change, added and removed settings */
	OPS("a = 1; b = \"x\"; c = true; d = 1.5;", "a = 2; b = 1; c = true; e = 0;", 0,
		"MOD a\nMOD b\nDEL d\nADD e\n");

	/* Groups go down */
	OPS("g = { h = { i = 1; }; };", "g = { h = { i = 2; j = 3; }; };", 0, "MOD g.h.i\nADD g.h.j\n");

	/* Arrays are compared whole */
	OPS("v = [1, 2, 3];", "v = [1, 3];", 0, "MOD v\n");

	/* Lists of groups by key */
	OPS("p = ( { id = 1; m = 1; }, { id = 2; m = 1; }, { id = 3; } );",
	    "p = ( { id = 3; }, { id = 1; m = 2; }, { id = 4; } );", 0,
	    "MOD p.[id=1].m\nDEL p.[id=2]\nADD p.[id=4]\n");

	/* ... by name, a string key */
	OPS("u = ( { name = \"bob\"; uid = 1; } );", "u = ( { name = \"bob\"; uid = 2; } );", 0, "MOD u.[name=\"bob\"].uid\n");

	/* ... by position when asked, or when there is no key */
	OPS("p = ( { id = 1; }, { id = 2; } );", "p = ( { id = 2; }, { id = 1; } );", CFG$M_POSITIONAL, "MOD p.[0].id\nMOD p.[1].id\n");
	OPS("l = ( 1, \"a\", 3 );", "l = ( 1, \"b\" );", 0, "MOD l.[1]\nDEL l.[2]\n");
	OPS("p = ( { id = 1; }, { id = 1; } );", "p = ( { id = 1; } );", 0, "DEL p.[1]\n");

	/* A group becomes a list */
	OPS("x = { a = 1; };", "x = ( 1 );", 0, "MOD x\n");

	/* Every path a comparison reports leads somewhere with CFG$LOOKUP */
	{
		const char	*o = "l = ( { name = \"a\\\"b\"; v = 1; }, { name = \"x]y\"; v = 1; } );\n"
				     "h = ( { id = 0xFFFFFFFF; v = 1; } );\nf = ( { id = 1.5; v = 1; } );\n";
		const char	*n = "l = ( { name = \"a\\\"b\"; v = 2; }, { name = \"x]y\"; v = 2; } );\n"
				     "h = ( { id = 0xFFFFFFFF; v = 2; } );\nf = ( { id = 1.5; v = 2; } );\n";
		CFG$TREE	*x = p(o, 0), *y = p(n, 0);
		CFG$NODE	*rx, *ry, *nx;
		CFG$DIFF	*d;
		uint32_t	cnt = 0;
		const char	*path;

		cfg$root(x, &rx); cfg$root(y, &ry);
		OK(cfg$diff(x, y, NULL, 0, &d));
		cfg$diff_count(d, &cnt);
		CHECK(cnt == 4, "4 differences, got %u", cnt);

		for ( uint32_t i = 0; i < cnt; i++ )
			{
			cfg$diff_entry(d, i, NULL, &path, NULL, NULL);
			CHECK(cfg$lookup(ry, path, &nx) & 1, "path %s not found in the new tree", path);
			CHECK(cfg$lookup(rx, path, &nx) & 1, "path %s not found in the old tree", path);
			}

		cfg$diff_free(d); cfg$free(x); cfg$free(y);
	}

	/* A listing of files with very long names */
	{
		static char	name[1200];
		CFG$TREE	*x, *y;
		CFG$DIFF	*d;
		char		*txt;
		uint32_t	len;

		memset(name, 'n', sizeof(name) - 1);
		cfg$parse_buf("a = 1;", 6, name, 0, &x);
		cfg$parse_buf("a = 2;", 6, name, 0, &y);
		OK(cfg$diff(x, y, NULL, 0, &d));
		OK(cfg$diff_listing(d, CFG$M_NOTIME, &txt, &len));
		CHECK(strstr(txt, name) != NULL, "long name not in the listing");
		free(txt); cfg$diff_free(d); cfg$free(x); cfg$free(y);
	}

	/* The fingerprint */
	FP("l = ( { id = 1; a = 1; }, { id = 2; a = 2; } );", 0, "l = ( { id = 2; a = 2; }, { id = 1; a = 1; } );", 0, 1);
	FP("l = ( { a = 1; }, { a = 2; } );", 0, "l = ( { a = 2; }, { a = 1; } );", 0, 0);
	FP("a = 1; b = 2;", 0, "b = 2;\n# note\na = 1;", 0, 1);
	FP("a = 1;", 0, "a = 1L;", 0, 1);
	FP("a = 1;", 0, "a = 2;", 0, 0);
	FP("a = 1;", 0, "a = \"1\";", 0, 0);
	FP("l = ( 1, 2 );", 0, "l = ( 2, 1 );", 0, 0);
	FP("A = 1;", CFG$M_NOCASE, "a = 1;", CFG$M_NOCASE, 1);
	FP("A = 1;", 0, "a = 1;", 0, 0);
	FP("", 0, "# nothing", 0, 1);

	/* Known fingerprints, checked by Python (uuid, hashlib): they must never change */
	for ( int k = 0; k < 2; k++ )
	{
		const char	*txt[] = { "", "a = 1;" }, *want[] = { "7b6d66a7-5b7b-5f45-8072-b9bfd2711fa0", "1acc6d4e-04d6-5cd7-9e3f-8588fda68b52" };
		CFG$TREE	*t = p(txt[k], 0);
		uint8_t		u[16];
		char		s[64];

		cfg$fingerprint(t, u);
		snprintf(s, sizeof(s), "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
			u[0], u[1], u[2], u[3], u[4], u[5], u[6], u[7], u[8], u[9], u[10], u[11], u[12], u[13], u[14], u[15]);
		CHECK(!strcmp(s, want[k]), "fingerprint of \"%s\" changed: %s", txt[k], s);
		cfg$free(t);
	}

	return t_report("t_diff");
}
