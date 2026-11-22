/*
**  yaLIBCFG tests - common helpers: a dump of a tree in a canonical form, file reading.
**  (Tests are written freely - the style rules do not bind them.)
*/
#ifndef T_UTIL_H
#define T_UTIL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "yalibcfg.h"

static int	g_fail = 0, g_total = 0;

#define	CHECK(cond, ...)	do { g_total++; if ( !(cond) ) { g_fail++; fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } while (0)

static inline const char *	t_typename (uint32_t t)
{
	static const char *n[] = { "NONE", "GROUP", "LIST", "ARRAY", "INT", "INT64", "FLOAT", "BOOL", "STRING", "INCLUDE" };
	return t < 10 ? n[t] : "?";
}

/* path TYPE value - one line per node, depth first */
static inline void	t_dump_node (CFG$NODE *n, const char *path, FILE *out)
{
	CFG$INFO	in;
	CFG$NODE	*c;
	char		sub[1024];
	int		i = 0;

	cfg$node_info(n, &in);

	switch ( in.type )
		{
		case CFG$K_TYPE_INT:
		case CFG$K_TYPE_INT64: { int64_t v; cfg$get_int64(n, "", &v); fprintf(out, "%s INT %" PRId64 "\n", path, v); return; }
		case CFG$K_TYPE_FLOAT: { double v; cfg$get_float(n, "", &v); fprintf(out, "%s FLOAT %.17g\n", path, v); return; }
		case CFG$K_TYPE_BOOL:  { int v; cfg$get_bool(n, "", &v); fprintf(out, "%s BOOL %d\n", path, v); return; }
		case CFG$K_TYPE_STRING: { const char *s; uint32_t l; cfg$get_string(n, "", &s, &l); fprintf(out, "%s STRING \"%.*s\"\n", path, (int) l, s); return; }
		}

	fprintf(out, "%s %s %u\n", path, t_typename(in.type), in.count);

	for ( unsigned s = cfg$first(n, &c); s & 1; s = cfg$next(c, &c), i++ )
		{
		CFG$INFO	ci;
		cfg$node_info(c, &ci);

		if ( ci.name )
			snprintf(sub, sizeof(sub), "%s%s%.*s", path, *path ? "." : "", (int) ci.namelen, ci.name);
		else	snprintf(sub, sizeof(sub), "%s.[%d]", path, i);

		t_dump_node(c, sub, out);
		}
}

static inline char *	t_dump (CFG$TREE *t)
{
	CFG$NODE	*r;
	char		*buf = NULL;
	size_t		len = 0;
	FILE		*f = open_memstream(&buf, &len);

	cfg$root(t, &r);
	t_dump_node(r, "", f);
	fclose(f);
	return buf;
}

static inline char *	t_readfile (const char *path, size_t *len)
{
	FILE	*f = fopen(path, "rb");
	char	*b;
	long	n;

	if ( !f )
		return NULL;

	fseek(f, 0, SEEK_END);
	n = ftell(f);
	fseek(f, 0, SEEK_SET);
	b = malloc((size_t) n + 1);
	*len = fread(b, 1, (size_t) n, f);
	b[*len] = 0;
	fclose(f);
	return b;
}

static inline int	t_report (const char *name)
{
	fprintf(stderr, "%s: %d check(s), %d failed\n", name, g_total, g_fail);
	return g_fail ? 1 : 0;
}

#endif
