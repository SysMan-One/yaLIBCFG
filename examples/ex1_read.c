/*
**  yaLIBCFG example 1: read values from a file.
**
**	$ ex1_read router.cfg
*/
#include <stdio.h>
#include "yalibcfg.h"

int	main (int argc, char **argv)
{
	CFG$TREE	*tree;
	CFG$NODE	*root;
	const char	*name;
	uint32_t	len;
	int32_t		mtu;
	unsigned	sts;

	cfg$inimsg();					/* the CFG message texts		*/

	sts = cfg$parse_file(argc > 1 ? argv[1] : "router.cfg", 0, &tree);

	if ( !(sts & 1) )				/* failure: the low bit is clear	*/
		{
		cfg$puterrs(tree);			/* %CFG-E-..., file, line, column	*/
		cfg$free(tree);
		return 1;
		}

	cfg$root(tree, &root);

	if ( cfg$get_string(root, "system.hostname", &name, &len) & 1 )
		printf("hostname: %.*s\n", (int) len, name);

	if ( cfg$get_int(root, "ports.[id=9].mtu", &mtu) & 1 )
		printf("port 9 MTU: %d\n", mtu);

	cfg$free(tree);
	return 0;
}
