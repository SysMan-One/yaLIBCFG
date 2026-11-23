#define	__MODULE__	"EX2_EDIT"
#define	__IDENT__	"X.01-01"
#define	__REV__		"1.1.0"

/*
**++
**
**  FACILITY:	yaLIBCFG - configuration trees in the libconfig syntax
**
**  MODULE:	examples/ex2_edit.c
**
**  ABSTRACT:	Example 2 of the manual: change a file and write it back.
**
**  DESCRIPTION: Sets a value, adds a setting and its group, removes port 10 and writes the file
**		back in its place - the comments and the layout stay.
**
**		Run it in the directory of the examples:
**
**			$ ./ex2_edit router.cfg
**
**  BUILD:	By hand, out of the tree, with yaLIBCFG and StarLet in /opt (cmake --install):
**
**		$ cc -Wall -Wextra -o ex2_edit ex2_edit.c \
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
