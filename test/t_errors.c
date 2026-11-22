/*
**  yaLIBCFG test: diagnostics of bad input - the condition value and the position.
*/
#include "t_util.h"

/* Internal record of the diagnostics - the test looks at it directly */
#include "../src/cfg_int.h"

static void	bad (const char *txt, uint32_t opts, unsigned want, uint32_t line, uint32_t col, int srcline)
{
	CFG$TREE	*t;
	unsigned	sts = cfg$parse_buf(txt, (uint32_t) strlen(txt), "<test>", opts, &t);

	g_total++;

	if ( sts != want )
		{
		g_fail++;
		fprintf(stderr, "FAIL line %d: \"%s\": sts %08X, want %08X\n", srcline, txt, sts, want);
		cfg$puterrs(t);
		}
	else if ( line && (!t->nerr || (t->err[0].line != line) || (t->err[0].col != col)) )
		{
		g_fail++;
		fprintf(stderr, "FAIL line %d: \"%s\": position %u:%u, want %u:%u\n", srcline, txt,
			t->nerr ? t->err[0].line : 0, t->nerr ? t->err[0].col : 0, line, col);
		}

	cfg$free(t);
}

#define	BAD(txt, opts, want, line, col)	bad(txt, opts, want, line, col, __LINE__)

int	main (void)
{
	cfg$inimsg();

	BAD("a = ;",				0, CFG$_SYNTAX,		1, 5);
	BAD("a = 1;\nb = $;",			0, CFG$_SYNTAX,		2, 5);
	BAD("a 1;",				0, CFG$_SYNTAX,		1, 3);
	BAD("a = { b = 1; } }",			0, CFG$_SYNTAX,		1, 16);
	BAD("a = \"abc",			0, CFG$_UNTERM,		1, 5);
	BAD("/* x",				0, CFG$_UNTERM,		1, 1);
	BAD("a = { b = 1;",			0, CFG$_UNTERM,		1, 1);
	BAD("a = 0x;",				0, CFG$_BADNUM,		1, 5);
	BAD("a = 12abc;",			0, CFG$_BADNUM,		1, 5);
	BAD("a = 99999999999999999999;",	0, CFG$_BADNUM,		1, 5);
	BAD("a = 0b102;",			0, CFG$_BADNUM,		1, 5);
	BAD("a = 1;\n  a = 2;",		0, CFG$_DUPNAME,	2, 3);
	BAD("a = [1, \"x\"];",		0, CFG$_MIXARRAY,	1, 9);
	BAD("a = [ { b = 1; } ];",		0, CFG$_SYNTAX,		1, 7);
	BAD("a = \"\\q\";",			0, CFG$_BADESC,		1, 6);
	BAD("@include \"no-such-file.cfg\"",	0, CFG$_OPENIN,		0, 0);
	BAD("@include \"x.cfg\"",		CFG$M_NOINCLUDE, CFG$_SYNTAX, 1, 1);

	/* Accepted */
	BAD("a = 1",				0, CFG$_NORMAL,		0, 0);
	BAD("a = [1, 2L, 0x3];",		0, CFG$_NORMAL,		0, 0);
	BAD("a = ( 1, \"x\", { b = 2; }, [ 3 ] );", 0, CFG$_NORMAL,	0, 0);
	BAD("a = [ 1, 2, ];",			0, CFG$_SYNTAX,		1, 13);
	BAD("a = ( 1, );",			0, CFG$_SYNTAX,		1, 10);
	BAD("A = 1; a = 2;",			0, CFG$_NORMAL,		0, 0);
	BAD("A = 1; a = 2;",			CFG$M_NOCASE, CFG$_DUPNAME, 1, 8);
	BAD("",					0, CFG$_NORMAL,		0, 0);
	BAD("a = 1e-400;",			0, CFG$_NORMAL,		0, 0);
	BAD("a = 1e400;",			0, CFG$_BADNUM,		1, 5);

	return t_report("t_errors");
}
