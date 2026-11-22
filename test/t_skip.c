/*
**  yaLIBCFG test: CFG$M_SKIPERRS - bad settings skipped, the rest of the file taken, the text kept.
*/
#include "t_util.h"
#include "../src/cfg_int.h"

static int	g_line;

/* Parse with SKIPERRS; check the status, the number skipped, the settings that must be there or not, the round trip */
static CFG$TREE *	sk (const char *txt, unsigned wsts, uint32_t wskip, int line)
{
	CFG$TREE	*t;
	unsigned	sts = cfg$parse_buf(txt, (uint32_t) strlen(txt), "<skip>", CFG$M_SKIPERRS, &t);
	uint32_t	n = 0, skipped = 0;
	char		*out;
	uint32_t	len;

	g_line = line;
	cfg$errcount(t, &n, &skipped);
	g_total++;

	if ( (sts != wsts) || (skipped != wskip) )
		{
		g_fail++;
		fprintf(stderr, "FAIL line %d: sts %08X want %08X, skipped %u want %u\n", line, sts, wsts, skipped, wskip);
		cfg$puterrs(t);
		}

	/* The text of the file back, byte for byte - the bad settings too */
	if ( 1 & sts )
		{
		g_total++;

		if ( !(1 & cfg$write_buf(t, 0, &out, &len)) || (len != strlen(txt)) || memcmp(out, txt, len) )
			{
			g_fail++;
			fprintf(stderr, "FAIL line %d: round trip differs\n", line);
			}
		else	free(out);
		}

	return t;
}

static void	has (CFG$TREE *t, const char *path, int want)
{
	CFG$NODE	*r, *n;

	cfg$root(t, &r);
	g_total++;

	if ( !!(1 & cfg$lookup(r, path, &n)) != want )
		{
		g_fail++;
		fprintf(stderr, "FAIL line %d: %s %s\n", g_line, path, want ? "missing" : "present");
		}
}

#define	SK(txt, wsts, wskip)	sk(txt, wsts, wskip, __LINE__)

int	main (void)
{
	CFG$TREE	*t;
	CFG$NODE	*r;
	char		*out;
	uint32_t	len;

	cfg$inimsg();

	/* A value missing: the setting goes, its neighbours stay; a change writes around it */
	t = SK("a = 1;\nb = ;\nc = 3;\n", CFG$_SKIPPED, 1);
	has(t, "a", 1); has(t, "b", 0); has(t, "c", 1);
	CHECK((t->nerr == 1) && (t->err[0].sts == CFG$_SYNTAX) && (t->err[0].line == 2) && (t->err[0].col == 5), "the diagnostic of b");
	cfg$root(t, &r);
	CHECK(cfg$set_int(r, "c", 4) & 1, "set c");
	CHECK((cfg$write_buf(t, 0, &out, &len) & 1) && !strcmp(out, "a = 1;\nb = ;\nc = 4;\n"), "write around the bad setting");
	free(out);
	cfg$free(t);

	/* Errors of the lexer: a bad character, a bad number, a string not closed on its line */
	t = SK("a = 1;\nb = $;\nc = 12abc;\nd = \"open\ne = 5;\n", CFG$_SKIPPED, 3);
	has(t, "a", 1); has(t, "b", 0); has(t, "c", 0); has(t, "d", 0); has(t, "e", 1);
	cfg$free(t);

	/* Inside a group: only the bad setting of the group goes */
	t = SK("g = { x = 1; y = @; z = 3; };\nw = 4;\n", CFG$_SKIPPED, 1);
	has(t, "g.x", 1); has(t, "g.y", 0); has(t, "g.z", 1); has(t, "w", 1);
	cfg$free(t);

	/* A group not closed before the end of the file: the whole group goes */
	t = SK("a = 1;\ng = { x = 1;\n", CFG$_SKIPPED, 1);
	has(t, "a", 1); has(t, "g", 0);
	cfg$free(t);

	/* A list with a bad element: the list goes, as one value */
	t = SK("l = ( 1, @, 3 );\nb = 2;\n", CFG$_SKIPPED, 1);
	has(t, "l", 0); has(t, "b", 1);
	cfg$free(t);

	/* A duplicate: the second goes, the first stays */
	t = SK("a = 1;\na = 2;\n", CFG$_SKIPPED, 1);
	{ int32_t v = 0; cfg$root(t, &r); CHECK((cfg$get_int(r, "a", &v) & 1) && (v == 1), "the first a stays"); }
	cfg$free(t);

	/* An include that cannot be read: the directive goes, the file stays */
	t = SK("@include \"no-such-file.cfg\"\na = 1;\n", CFG$_SKIPPED, 1);
	has(t, "a", 1);
	CHECK(t->nerr == 2, "OPENIN and INCLUDE recorded, %u", t->nerr);
	cfg$free(t);

	/* Not a setting at all where one should be */
	t = SK("a = 1;\n42;\nb = 2;\n", CFG$_SKIPPED, 1);
	has(t, "a", 1); has(t, "b", 1);
	cfg$free(t);

	/* Findings of the review of the skip */

	/* A stray closing bracket: taken as the bad statement, no endless loop */
	t = SK("a = 1;\n}\nb = 2;\n", CFG$_SKIPPED, 1);
	has(t, "a", 1); has(t, "b", 1);
	cfg$free(t);
	t = SK("g = { x = 1; ] y = 2; };\n", CFG$_SKIPPED, 1);
	has(t, "g.x", 1); has(t, "g.y", 1);
	cfg$free(t);
	t = SK("g = { ) };\n", CFG$_SKIPPED, 1);
	has(t, "g", 1);
	cfg$free(t);

	/* An error of the lexer in the token after a good setting does not take that setting */
	t = SK("a = 1;\n$\nc = 2;\n", CFG$_SKIPPED, 1);
	has(t, "a", 1); has(t, "c", 1);
	cfg$free(t);
	t = SK("g = { x = 1; };\n12abc = 3;\n", CFG$_SKIPPED, 1);
	has(t, "g.x", 1);
	cfg$free(t);
	t = SK("a = 1;\n/* open", CFG$_SKIPPED, 1);
	has(t, "a", 1);
	cfg$free(t);

	/* Too deep: the bracket is counted once, the group around it closes */
	{
		char	txt[2048] = "g = {\n x = ";

		for ( int k = 0; k < 300; k++ ) strcat(txt, "(");
		strcat(txt, "1");
		for ( int k = 0; k < 300; k++ ) strcat(txt, ")");
		strcat(txt, ";\n y = 2;\n};\nz = 3;\n");
		t = SK(txt, CFG$_SKIPPED, 1);
		has(t, "g.y", 1); has(t, "z", 1); has(t, "g.x", 0);
		CHECK(t->nerr == 1, "one diagnostic for the nesting, %u", t->nerr);
		cfg$free(t);
	}

	/* The bad text up to the end of the file belongs to the skipped setting whole */
	t = SK("a = 1;\nb = \"open", CFG$_SKIPPED, 1);
	cfg$root(t, &r);
	CHECK(cfg$set_int(r, "zz", 5) & 1, "set zz");
	CHECK((cfg$write_buf(t, 0, &out, &len) & 1) && !strcmp(out, "a = 1;\nzz = 5;\nb = \"open"), "a new setting goes before an open string: %s", out);
	free(out);
	cfg$free(t);

	/* An unclosed group at the end: a new setting goes before it and is read back */
	t = SK("a = 1;\ng = { x = 1;\n", CFG$_SKIPPED, 1);
	cfg$root(t, &r);
	CHECK(cfg$set_int(r, "zz", 5) & 1, "set zz");
	CHECK(cfg$write_buf(t, 0, &out, &len) & 1, "write");
	cfg$free(t);
	t = SK(out, CFG$_SKIPPED, 1);
	has(t, "zz", 1); has(t, "a", 1);
	free(out);
	cfg$free(t);

	/* Skips inside a setting that is skipped whole count once */
	t = SK("g = { x = ; y = 1;\n", CFG$_SKIPPED, 1);
	cfg$free(t);

	/* The unclosed bracket is named the same way whatever came before */
	t = SK("a = ;\ng = { x = 1;\n", CFG$_SKIPPED, 2);
	CHECK((t->nerr == 2) && (t->err[1].sts == CFG$_UNTERM), "UNTERM for the group, got %08X", t->nerr > 1 ? t->err[1].sts : 0);
	cfg$free(t);

	/* A directive without its file name: the name on the next line is a setting of its own */
	t = SK("@include\nzq = 7;\n", CFG$_SKIPPED, 1);
	has(t, "zq", 1);
	cfg$free(t);

	/* A comment open to the end of the file after a bad setting: a new setting goes before it */
	t = SK("a = 1;\ng$\n  /* open\n  k = 1;\n", CFG$_SKIPPED, 1);
	cfg$root(t, &r);
	CHECK(cfg$set_int(r, "zz", 5) & 1, "set zz");
	CHECK(cfg$write_buf(t, 0, &out, &len) & 1, "write");
	cfg$free(t);
	t = SK(out, CFG$_SKIPPED, 1);
	has(t, "zz", 1);
	free(out);
	cfg$free(t);

	/* A bracket of another kind closes nothing: the group stays open, a new setting goes before it */
	t = SK("grp = 1;\ngrp:\n{\n k = 1;\n);\n", CFG$_SKIPPED, 1);
	cfg$root(t, &r);
	CHECK(cfg$set_int(r, "zz", 5) & 1, "set zz");
	CHECK(cfg$write_buf(t, 0, &out, &len) & 1, "write");
	cfg$free(t);
	t = SK(out, CFG$_SKIPPED, 1);
	has(t, "zz", 1);
	free(out);
	cfg$free(t);

	/* Findings of the second review */

	/* A '\\' last in an open string at the end of the file: the skipped text ends at the end */
	t = SK("a = 1;\nb = \"x\\", CFG$_SKIPPED, 1);
	cfg$free(t);

	/* The error of the token after a skipped setting is told, and that statement is skipped too */
	t = SK("a = $;\n0q = 1;\nc = 2;\n", CFG$_SKIPPED, 2);
	has(t, "q", 0); has(t, "c", 1);
	CHECK(t->nerr == 2, "two diagnostics, %u", t->nerr);
	cfg$free(t);
	t = SK("x = 1;\na = $; /* open", CFG$_SKIPPED, 2);
	CHECK((t->nerr == 2) && (t->err[1].sts == CFG$_UNTERM), "UNTERM of the comment told");
	cfg$free(t);

	/* An @include swallowed by a skipped setting does not stop the file being included later */
	{
		FILE	*f = fopen("t_skip_a.cfg", "w");

		fputs("aa = 1;\n", f);
		fclose(f);
		t = SK("l = ( { @include \"t_skip_a.cfg\"; }, $ );\n@include \"t_skip_a.cfg\"\nq = 1;\n", CFG$_SKIPPED, 1);
		has(t, "aa", 1); has(t, "q", 1);
		cfg$free(t);
		remove("t_skip_a.cfg");
	}

	/* Without the option nothing changed: the first error, the outermost bracket */
	{
		static const struct { const char *txt; unsigned sts; uint32_t nerr, line, col; } s_old[] = {
			{ "@include \"nofile.cfg\" $", CFG$_SYNTAX, 1, 1, 23 },
			{ "a = \"\\q\" $", CFG$_SYNTAX, 1, 1, 10 },
			{ "g = {\n  h = [ 1, 2", CFG$_UNTERM, 1, 1, 1 },
			{ "g = ( 1, { a = 1;", CFG$_UNTERM, 1, 1, 1 },
		};

		for ( int k = 0; k < (int) (sizeof(s_old) / sizeof(s_old[0])); k++ )
			{
			unsigned	sts = cfg$parse_buf(s_old[k].txt, (uint32_t) strlen(s_old[k].txt), "<x>", 0, &t);

			CHECK((sts == s_old[k].sts) && (t->nerr == s_old[k].nerr) && (t->err[0].line == s_old[k].line) && (t->err[0].col == s_old[k].col),
				"as before: case %d sts %08X nerr %u at %u:%u", k, sts, t->nerr, t->err[0].line, t->err[0].col);
			cfg$free(t);
			}
	}

	/* Nothing wrong: NORMAL, nothing skipped */
	t = SK("a = 1;\n", CFG$_NORMAL, 0);
	cfg$free(t);

	/* Many errors: all skipped, past CFG$K_MAXERRS counted */
	{
		char	txt[4096] = "";

		for ( int k = 0; k < 40; k++ )
			{
			char	l[32];
			snprintf(l, sizeof(l), "s%d = ;\n", k);
			strcat(txt, l);
			}

		strcat(txt, "ok = 1;\n");
		t = SK(txt, CFG$_SKIPPED, 40);
		has(t, "ok", 1);
		{ uint32_t n = 0; cfg$errcount(t, &n, NULL); CHECK(n == 40, "40 diagnostics, %u", n); }
		cfg$free(t);
	}

	/* Without the option: the first error stops the parse, as before */
	{
		const char	*txt = "a = 1;\nb = ;\nc = 3;\n";
		unsigned	sts = cfg$parse_buf(txt, (uint32_t) strlen(txt), "<x>", 0, &t);

		CHECK(sts == CFG$_SYNTAX, "no skip without the option");
		CHECK(cfg$write_buf(t, 0, &out, &len) == CFG$_BROKEN, "broken without the option");
		cfg$free(t);
	}

	/* The skipped text is nothing to the comparison, the fingerprint, the schema */
	{
		CFG$TREE	*x, *y;
		CFG$DIFF	*d;
		uint8_t		u1[16], u2[16];
		uint32_t	n = 9;
		static const CFG$SCHEMA	s[] = { { .name = "a", .type = CFG$K_TYPE_INT }, { .name = NULL } };

		cfg$parse_buf("a = 1;\nb = ;\n", 14, "x", CFG$M_SKIPERRS, &x);
		cfg$parse_buf("a = 1;\n", 7, "y", 0, &y);
		cfg$fingerprint(x, u1); cfg$fingerprint(y, u2);
		CHECK(!memcmp(u1, u2, 16), "fingerprint ignores the skipped text");
		CHECK(cfg$diff(x, y, NULL, 0, &d) & 1, "diff");
		cfg$diff_count(d, &n);
		CHECK(n == 0, "no differences, %u", n);
		cfg$diff_free(d);
		CHECK(cfg$validate(x, s, NULL, NULL) & 1, "the schema does not see it");
		cfg$free(x); cfg$free(y);
	}

	return t_report("t_skip");
}
