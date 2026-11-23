#define	__MODULE__	"EX3_WALK"
#define	__IDENT__	"X.01-01"
#define	__REV__		"1.1.0"

/*
**++
**
**  FACILITY:	yaLIBCFG - configuration trees in the libconfig syntax
**
**  MODULE:	examples/ex3_walk.c
**
**  ABSTRACT:	Example 3 of the manual: walk a tree.
**
**  DESCRIPTION: Goes over every node with CFG$FIRST and CFG$NEXT and shows each setting with its
**		type and its place in the file (CFG$NODE_INFO).
**
**		Run it in the directory of the examples:
**
**			$ ./ex3_walk router.cfg
**
**  BUILD:	By hand, out of the tree, with yaLIBCFG and StarLet in /opt (cmake --install):
**
**		$ cc -Wall -Wextra -o ex3_walk ex3_walk.c \
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
#include <inttypes.h>
#include "yalibcfg.h"

static void	walk (CFG$NODE *node, int depth)
{
	CFG$NODE	*child;
	CFG$INFO	info;
	unsigned	sts;

	for ( sts = cfg$first(node, &child); sts & 1; sts = cfg$next(child, &child) )
		{
		cfg$node_info(child, &info);

		printf("%*s%-10.*s line %3u  ", depth * 2, "", (int) (info.name ? info.namelen : 3), info.name ? info.name : "[ ]", info.line);

		switch ( info.type )
			{
			case CFG$K_TYPE_INT:
			case CFG$K_TYPE_INT64:
				{ int64_t v; cfg$get_int64(child, "", &v); printf("%" PRId64 "\n", v); break; }

			case CFG$K_TYPE_BOOL:
				{ int v; cfg$get_bool(child, "", &v); printf("%s\n", v ? "true" : "false"); break; }

			case CFG$K_TYPE_FLOAT:
				{ double v; cfg$get_float(child, "", &v); printf("%g\n", v); break; }

			case CFG$K_TYPE_STRING:
				{ const char *s; uint32_t l; cfg$get_string(child, "", &s, &l); printf("\"%.*s\"\n", (int) l, s); break; }

			default:
				printf("%s of %u\n", info.type == CFG$K_TYPE_GROUP ? "group" : info.type == CFG$K_TYPE_LIST ? "list" : "array", info.count);
				walk(child, depth + 1);
			}
		}
}

int	main (int argc, char **argv)
{
	CFG$TREE	*tree;
	CFG$NODE	*root;

	cfg$inimsg();

	if ( !(cfg$parse_file(argc > 1 ? argv[1] : "router.cfg", 0, &tree) & 1) )
		{
		cfg$puterrs(tree);
		cfg$free(tree);
		return 1;
		}

	cfg$root(tree, &root);
	walk(root, 0);
	cfg$free(tree);
	return 0;
}
