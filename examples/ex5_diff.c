#define	__MODULE__	"EX5_DIFF"
#define	__IDENT__	"X.01-01"
#define	__REV__		"1.1.0"

/*
**++
**
**  FACILITY:	yaLIBCFG - configuration trees in the libconfig syntax
**
**  MODULE:	examples/ex5_diff.c
**
**  ABSTRACT:	Example 5 of the manual: compare two configurations.
**
**  DESCRIPTION: Compares two trees (CFG$DIFF) and prints the listing of the differences in the
**		manner of the DIFFERENCES utility.
**
**		Run it in the directory of the examples:
**
**			$ ./ex5_diff router.cfg router-new.cfg
**
**  BUILD:	By hand, out of the tree, with yaLIBCFG and StarLet in /opt (cmake --install):
**
**		$ cc -Wall -Wextra -o ex5_diff ex5_diff.c \
**			-I/opt/yalibcfg/include -I/opt/starlet/include \
**			-L/opt/yalibcfg/lib -L/opt/starlet/lib -lyalibcfg -lstarlet -lpthread -lm
**
**		The order of the libraries matters: libyalibcfg.a needs libstarlet.a after it.
**		Within the tree it is built by CMake with the tests (YALIBCFG_M_EXAMPLES).
**
**  AUTHOR:	StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
**
**  CREATION DATE:  22-NOV-2026
**
**  MODIFICATION HISTORY:
**
**	X.01-00		22-NOV-2026	RRL
**		First edition.
**
**	X.01-01		23-NOV-2026	RRL
**		The header of the house; the line to build it by hand.
**
**--
*/
#include <stdio.h>
#include <stdlib.h>
#include "yalibcfg.h"

int	main (int argc, char **argv)
{
	CFG$TREE	*old = NULL, *new = NULL;
	CFG$DIFF	*diff;
	char		*text;
	uint32_t	len, count = 0;
	unsigned	sts;

	if ( argc < 3 )
		{
		fprintf(stderr, "usage: ex5_diff old.cfg new.cfg [notime]\n");
		return 2;
		}

	cfg$inimsg();

	sts = cfg$parse_file(argv[1], 0, &old);

	if ( sts & 1 )
		sts = cfg$parse_file(argv[2], 0, &new);

	if ( sts & 1 )
		sts = cfg$diff(old, new, NULL, 0, &diff);

	if ( !(sts & 1) )
		{
		cfg$puterrs(old);
		cfg$puterrs(new);
		cfg$free(old);
		cfg$free(new);
		return 1;
		}

	if ( cfg$diff_listing(diff, (argc > 3) ? CFG$M_NOTIME : 0, &text, &len) & 1 )
		{
		fwrite(text, 1, len, stdout);
		free(text);
		}

	cfg$diff_count(diff, &count);

	cfg$diff_free(diff);
	cfg$free(old);
	cfg$free(new);

	return count ? 1 : 0;			/* as diff(1): 1 - they differ		*/
}
