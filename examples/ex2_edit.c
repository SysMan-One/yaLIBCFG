/*
**  yaLIBCFG example 2: change a file and write it back - the comments stay.
**
**	$ ex2_edit router.cfg
*/
#include <stdio.h>
#include "yalibcfg.h"

int	main (int argc, char **argv)
{
	CFG$TREE	*tree;
	CFG$NODE	*root, *port;
	unsigned	sts;

	cfg$inimsg();

	if ( !((sts = cfg$parse_file(argc > 1 ? argv[1] : "router.cfg", 0, &tree)) & 1) )
		{
		cfg$puterrs(tree);
		cfg$free(tree);
		return 1;
		}

	cfg$root(tree, &root);

	/* A new value for an old setting */
	sts = cfg$set_string(root, "system.location", "Lab, rack 4", 11);

	/* A setting that was not there: made, with the group on its way */
	if ( sts & 1 )
		sts = cfg$set_int(root, "system.snmp.port", 161);

	/* Port 10 goes away, with the comment above it if it had one */
	if ( (sts & 1) && (cfg$lookup(root, "ports.[id=10]", &port) & 1) )
		sts = cfg$delete(port);

	/* Into the file, in its place (a temporary file, then a rename) */
	if ( sts & 1 )
		sts = cfg$write(tree);

	if ( !(sts & 1) )
		{
		$PUTMSG_FAO(sts);			/* the condition value of the routine	*/
		cfg$puterrs(tree);			/* and what the tree recorded		*/
		}

	cfg$free(tree);
	return (sts & 1) ? 0 : 1;
}
