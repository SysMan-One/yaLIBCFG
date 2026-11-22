/*
**  yaLIBCFG test: the same files read by libconfig and by yaLIBCFG give the same tree.
*/
#include "t_util.h"
#include <libconfig.h>
#include <libgen.h>

static void	lc_dump (config_setting_t *s, const char *path, FILE *out)
{
	char	sub[1024];

	switch ( config_setting_type(s) )
		{
		case CONFIG_TYPE_INT:	fprintf(out, "%s INT %d\n", path, config_setting_get_int(s)); return;
		case CONFIG_TYPE_INT64:	fprintf(out, "%s INT %lld\n", path, config_setting_get_int64(s)); return;
		case CONFIG_TYPE_FLOAT:	fprintf(out, "%s FLOAT %.17g\n", path, config_setting_get_float(s)); return;
		case CONFIG_TYPE_BOOL:	fprintf(out, "%s BOOL %d\n", path, config_setting_get_bool(s)); return;
		case CONFIG_TYPE_STRING: fprintf(out, "%s STRING \"%s\"\n", path, config_setting_get_string(s)); return;
		}

	fprintf(out, "%s %s %d\n", path, config_setting_type(s) == CONFIG_TYPE_GROUP ? "GROUP" :
		config_setting_type(s) == CONFIG_TYPE_LIST ? "LIST" : "ARRAY", config_setting_length(s));

	for ( int i = 0; i < config_setting_length(s); i++ )
		{
		config_setting_t	*c = config_setting_get_elem(s, (unsigned) i);
		const char		*n = config_setting_name(c);

		if ( n && (config_setting_type(s) == CONFIG_TYPE_GROUP) )
			snprintf(sub, sizeof(sub), "%s%s%s", path, *path ? "." : "", n);
		else	snprintf(sub, sizeof(sub), "%s.[%d]", path, i);

		lc_dump(c, sub, out);
		}
}

int	main (int argc, char **argv)
{
	cfg$inimsg();

	for ( int i = 1; i < argc; i++ )
		{
		config_t	lc;
		CFG$TREE	*t;
		char		*a = NULL, *b, dir[4096];
		size_t		alen = 0;
		FILE		*f;

		snprintf(dir, sizeof(dir), "%s", argv[i]);
		config_init(&lc);
		config_set_include_dir(&lc, dirname(dir));

		if ( !config_read_file(&lc, argv[i]) )
			{
			CHECK(0, "%s: libconfig refused it: %s line %d", argv[i], config_error_text(&lc), config_error_line(&lc));
			config_destroy(&lc);
			continue;
			}

		f = open_memstream(&a, &alen);
		lc_dump(config_root_setting(&lc), "", f);
		fclose(f);
		config_destroy(&lc);

		if ( !(cfg$parse_file(argv[i], 0, &t) & 1) )
			{
			CHECK(0, "%s: yaLIBCFG refused it", argv[i]);
			cfg$puterrs(t);
			cfg$free(t);
			free(a);
			continue;
			}

		b = t_dump(t);
		CHECK(!strcmp(a, b), "%s: trees differ\n--- libconfig\n%s--- yaLIBCFG\n%s---", argv[i], a, b);
		free(a);
		free(b);
		cfg$free(t);
		}

	return t_report("t_libconfig");
}
