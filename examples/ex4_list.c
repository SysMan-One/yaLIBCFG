#define	__MODULE__	"EX4_LIST"
#define	__IDENT__	"X.01-01"
#define	__REV__		"1.1.0"

/*
**++
**
**  FACILITY:	yaLIBCFG - configuration trees in the libconfig syntax
**
**  MODULE:	examples/ex4_list.c
**
**  ABSTRACT:	Example 4 of the manual: a list of groups.
**
**  DESCRIPTION: Finds an element of a list by its key, adds an element to the list and a value to
**		an array.
**
**		Run it in the directory of the examples:
**
**			$ ./ex4_list router.cfg
**
**  BUILD:	By hand, out of the tree, with yaLIBCFG and StarLet in /opt (cmake --install):
**
**		$ cc -Wall -Wextra -o ex4_list ex4_list.c \
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
	CFG$TREE	*tree;
	CFG$NODE	*root, *ports, *port, *vlans, *vlan;
	char		*text;
	uint32_t	len;
	unsigned	sts;

	cfg$inimsg();

	if ( !(cfg$parse_file(argc > 1 ? argv[1] : "router.cfg", 0, &tree) & 1) )
		{
		cfg$puterrs(tree);
		cfg$free(tree);
		return 1;
		}

	cfg$root(tree, &root);

	/* Port 11: a new group at the end of the list */
	sts = cfg$lookup(root, "ports", &ports);

	if ( sts & 1 )
		sts = cfg$add(ports, NULL, CFG$K_TYPE_GROUP, &port);
	if ( sts & 1 )
		sts = cfg$set_int(port, "id", 11);
	if ( sts & 1 )
		sts = cfg$set_string(port, "speed", "10G", 3);

	/* VLAN 300 at the end of the array */
	if ( sts & 1 )
		sts = cfg$lookup(root, "vlans", &vlans);
	if ( sts & 1 )
		sts = cfg$add(vlans, NULL, CFG$K_TYPE_INT, &vlan);
	if ( sts & 1 )
		sts = cfg$set_int(vlan, "", 300);

	/* Show the text instead of saving it */
	if ( sts & 1 )
		sts = cfg$write_buf(tree, 0, &text, &len);

	if ( sts & 1 )
		{
		fwrite(text, 1, len, stdout);
		free(text);
		}
	else	$PUTMSG_FAO(sts);

	cfg$free(tree);
	return (sts & 1) ? 0 : 1;
}
