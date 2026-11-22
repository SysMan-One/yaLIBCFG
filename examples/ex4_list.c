/*
**  yaLIBCFG example 4: a list of groups - find by key, add an element, add to an array.
**
**	$ ex4_list router.cfg
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
