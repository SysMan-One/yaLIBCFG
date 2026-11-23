#define	__MODULE__	"EX6_SCHEMA"
#define	__IDENT__	"X.01-01"
#define	__REV__		"1.1.0"

/*
**++
**
**  FACILITY:	yaLIBCFG - configuration trees in the libconfig syntax
**
**  MODULE:	examples/ex6_schema.c
**
**  ABSTRACT:	Example 6 of the manual: a schema and the validation by it.
**
**  DESCRIPTION: A schema for router.cfg: types, ranges, keywords, the key of a list, a check
**		routine; every problem of a file is told with its line and column.
**
**		Run it in the directory of the examples:
**
**			$ ./ex6_schema router.cfg
**			$ ./ex6_schema router-bad.cfg
**
**  BUILD:	By hand, out of the tree, with yaLIBCFG and StarLet in /opt (cmake --install):
**
**		$ cc -Wall -Wextra -o ex6_schema ex6_schema.c \
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
