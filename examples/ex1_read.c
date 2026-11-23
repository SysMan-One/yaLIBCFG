#define	__MODULE__	"EX1_READ"
#define	__IDENT__	"X.01-01"
#define	__REV__		"1.1.0"

/*
**++
**
**  FACILITY:	yaLIBCFG - configuration trees in the libconfig syntax
**
**  MODULE:	examples/ex1_read.c
**
**  ABSTRACT:	Example 1 of the manual: read values from a file.
**
**  DESCRIPTION: Parses the file, reads the host name and the MTU of port 9 by their paths, and
**		shows what a failure of the parse looks like (CFG$PUTERRS).
**
**		Run it in the directory of the examples:
**
**			$ ./ex1_read router.cfg
**
**  BUILD:	By hand, out of the tree, with yaLIBCFG and StarLet in /opt (cmake --install):
**
**		$ cc -Wall -Wextra -o ex1_read ex1_read.c \
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
