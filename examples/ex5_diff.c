/*
**  yaLIBCFG example 5: compare two configurations and print the listing of the differences.
**
**	$ ex5_diff router.cfg router-new.cfg
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
