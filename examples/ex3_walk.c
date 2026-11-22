/*
**  yaLIBCFG example 3: walk a tree and show every setting with its place in the file.
**
**	$ ex3_walk router.cfg
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
