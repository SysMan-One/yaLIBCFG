/*
**  yaLIBCFG example 6: a schema for router.cfg, and the validation of a file by it.
**
**	$ ex6_schema router.cfg
**	$ ex6_schema router-bad.cfg
*/
#include <stdio.h>
#include "yalibcfg.h"

/* The words SPEED may take */
static const CFG$KEYWORD	s_speeds[] = {
	{ "1G",  1000 },
	{ "10G", 10000 },
	{ NULL,  0 }
};

/* SYSTEM = { ... } */
static const CFG$SCHEMA	s_system[] = {
	{ .name = "hostname", .type = CFG$K_TYPE_STRING, .flags = CFG$M_REQUIRED | CFG$M_LENGTH, .min = 1, .max = 32,
	  .help = "name of the router, shown in the prompt" },
	{ .name = "location", .type = CFG$K_TYPE_STRING, .help = "where it stands" },
	{ .name = "snmp",     .type = CFG$K_TYPE_GROUP,  .flags = CFG$M_OPEN, .help = "SNMP agent, not checked here" },
	{ .name = NULL }
};

/* An element of PORTS: a group, its key ID */
static const CFG$SCHEMA	s_port[] = {
	{ .name = "id",      .type = CFG$K_TYPE_INT,  .flags = CFG$M_KEY | CFG$M_RANGE, .min = 1, .max = 48 },
	{ .name = "speed",   .type = CFG$K_TYPE_KWD,  .flags = CFG$M_REQUIRED, .kwd = s_speeds },
	{ .name = "enabled", .type = CFG$K_TYPE_BOOL },
	{ .name = "mtu",     .type = CFG$K_TYPE_INT,  .flags = CFG$M_RANGE, .min = 64, .max = 9216 },
	{ .name = NULL }
};

static const CFG$SCHEMA	s_port_elem = { .type = CFG$K_TYPE_GROUP, .sub = s_port };
static const CFG$SCHEMA	s_vlan_elem = { .type = CFG$K_TYPE_INT, .flags = CFG$M_RANGE, .min = 1, .max = 4094 };

/* The file */
static const CFG$SCHEMA	s_router[] = {
	{ .name = "system", .type = CFG$K_TYPE_GROUP, .flags = CFG$M_REQUIRED, .sub = s_system },
	{ .name = "ports",  .type = CFG$K_TYPE_LIST,  .flags = CFG$M_COUNT, .min = 1, .max = 48, .sub = &s_port_elem },
	{ .name = "vlans",  .type = CFG$K_TYPE_ARRAY, .sub = &s_vlan_elem },
	{ .name = NULL }
};

int	main (int argc, char **argv)
{
	CFG$TREE	*tree;
	uint32_t	nerr = 0;
	unsigned	sts;

	cfg$inimsg();

	if ( !((sts = cfg$parse_file(argc > 1 ? argv[1] : "router.cfg", 0, &tree)) & 1) )
		{
		cfg$puterrs(tree);
		cfg$free(tree);
		return 1;
		}

	sts = cfg$validate(tree, s_router, NULL, &nerr);

	if ( sts & 1 )
		printf("%s: valid\n", argc > 1 ? argv[1] : "router.cfg");
	else	{
		printf("%s: %u problem(s)\n", argc > 1 ? argv[1] : "router.cfg", nerr);
		cfg$puterrs(tree);			/* each with its line and column		*/
		}

	cfg$free(tree);
	return (sts & 1) ? 0 : 1;
}
