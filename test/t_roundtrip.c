/*
**  yaLIBCFG test: round trip.  Every file given is parsed and written back without changes;
**  the text of every source must be the file byte for byte.
*/
#include "t_util.h"

int	main (int argc, char **argv)
{
	cfg$inimsg();

	for ( int i = 1; i < argc; i++ )
		{
		CFG$TREE	*t;
		unsigned	sts = cfg$parse_file(argv[i], 0, &t);

		CHECK(sts & 1, "%s: parse failed, sts=%08X", argv[i], sts);

		if ( !(sts & 1) )
			{
			cfg$puterrs(t);
			cfg$free(t);
			continue;
			}

		for ( uint32_t s = 0; ; s++ )
			{
			const char	*path;
			char		*out, *orig;
			uint32_t	olen;
			size_t		len;

			if ( !(cfg$source(t, s, &path) & 1) )
				break;

			CHECK(cfg$write_buf(t, s, &out, &olen) & 1, "%s: write_buf(%u) failed", argv[i], s);
			orig = t_readfile(path, &len);
			CHECK(orig && (len == olen) && !memcmp(orig, out, len), "%s: source %u (%s) differs after the round trip:\n---\n%s\n---\n%s", argv[i], s, path, orig ? orig : "(unreadable)", out);
			free(orig);
			free(out);
			}

		cfg$free(t);
		}

	return t_report("t_roundtrip");
}
