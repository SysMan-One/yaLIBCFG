/*
**  yaLIBCFG test: validation by a schema.
*/
#include "t_util.h"
#include "../src/cfg_int.h"

static const CFG$KEYWORD	speeds[] = { { "1G", 1000 }, { "10G", 10000 }, { NULL, 0 } };

static unsigned	even_check (CFG$NODE *n, void *arg, char *msg, uint32_t msgsz, uint32_t *msglen)
{
	int64_t	v;

	(*(int *) arg)++;
	cfg$get_int64(n, "", &v);

	if ( v % 2 )
		{
		*msglen = (uint32_t) snprintf(msg, msgsz, "must be even");
		return CFG$_CHECK;
		}

	return STS$K_SUCCESS;
}

static unsigned	fatal_check (CFG$NODE *n, void *arg, char *msg, uint32_t msgsz, uint32_t *msglen)
{
	*msglen = (uint32_t) snprintf(msg, msgsz, "never");
	return (CFG$_CHECK & ~0x7U) | STS$K_FATAL;
}

static const CFG$SCHEMA	port_s[] = {
	{ .name = "id",      .type = CFG$K_TYPE_INT,  .flags = CFG$M_KEY | CFG$M_RANGE, .min = 1, .max = 64 },
	{ .name = "speed",   .type = CFG$K_TYPE_KWD,  .kwd = speeds },
	{ .name = "mtu",     .type = CFG$K_TYPE_INT,  .flags = CFG$M_RANGE, .min = 64, .max = 9216 },
	{ .name = "enabled", .type = CFG$K_TYPE_BOOL },
	{ .name = "vlan",    .type = CFG$K_TYPE_INT,  .check = even_check },
	{ .name = NULL }
};

static const CFG$SCHEMA	port_elem = { .type = CFG$K_TYPE_GROUP, .sub = port_s };
static const CFG$SCHEMA	vlan_elem = { .type = CFG$K_TYPE_INT, .flags = CFG$M_RANGE, .min = 1, .max = 4094 };

static const CFG$SCHEMA	sys_s[] = {
	{ .name = "hostname", .type = CFG$K_TYPE_STRING, .flags = CFG$M_REQUIRED | CFG$M_LENGTH, .min = 1, .max = 16 },
	{ .name = "mgmt",     .type = CFG$K_TYPE_IPADDR },
	{ .name = "mac",      .type = CFG$K_TYPE_MAC },
	{ .name = "load",     .type = CFG$K_TYPE_FLOAT, .flags = CFG$M_RANGE, .fmin = 0.0, .fmax = 1.0 },
	{ .name = "old",      .type = CFG$K_TYPE_INT, .flags = CFG$M_DEPRECATED },
	{ .name = NULL }
};

static const CFG$SCHEMA	root_s[] = {
	{ .name = "system", .type = CFG$K_TYPE_GROUP, .flags = CFG$M_REQUIRED, .sub = sys_s },
	{ .name = "ports",  .type = CFG$K_TYPE_LIST,  .flags = CFG$M_COUNT, .min = 1, .max = 4, .sub = &port_elem },
	{ .name = "vlans",  .type = CFG$K_TYPE_ARRAY, .sub = &vlan_elem },
	{ .name = "extra",  .type = CFG$K_TYPE_GROUP, .flags = CFG$M_OPEN },
	{ .name = NULL }
};

/* A menu holds a title and a list of menus */
extern const CFG$SCHEMA	menu_s[];
static const CFG$SCHEMA	menu_elem = { .type = CFG$K_TYPE_GROUP, .sub = menu_s };
const CFG$SCHEMA	menu_s[] = {
	{ .name = "title", .type = CFG$K_TYPE_STRING, .flags = CFG$M_REQUIRED },
	{ .name = "items", .type = CFG$K_TYPE_LIST, .sub = &menu_elem },
	{ .name = NULL }
};

#define	OK(x)	CHECK((x) & 1, "%s failed", #x)

/* Validate; give "STATUS|msg1|msg2..." where msg = ident:line:col:text */
static char *	run (const char *txt, const CFG$SCHEMA *s, uint32_t opts, unsigned *sts)
{
	CFG$TREE	*t;
	char		*buf = NULL;
	size_t		len = 0;
	FILE		*f = open_memstream(&buf, &len);
	int		calls = 0;

	if ( !(cfg$parse_buf(txt, (uint32_t) strlen(txt), "s.cfg", opts, &t) & 1) )
		{ cfg$puterrs(t); fclose(f); cfg$free(t); return strdup("PARSE"); }

	*sts = cfg$validate(t, s, &calls, NULL);

	for ( uint32_t i = 0; i < t->nerr; i++ )
		{
		EMSG_RECORD	*m;
		const char	*id = "?";
		char		idb[32];

		if ( __util$getmsg(t->err[i].sts, &m) & 1 )
			{
			snprintf(idb, sizeof(idb), "%.*s", (int) (strchr(m->text, ',') - m->text), m->text);
			id = idb;
			}

		fprintf(f, "%s %u:%u %.*s\n", id, t->err[i].line, t->err[i].col, (int) t->err[i].len, t->err[i].text);
		}

	fclose(f);
	cfg$free(t);
	return buf;
}

#define	V(txt, s, opts, wsts, want)	do { unsigned st = 0; char *got = run(txt, s, opts, &st); \
	CHECK((st == (wsts)) && !strcmp(got, want), "sts %08X want %08X\n--- want\n%s--- got\n%s---", st, (unsigned) (wsts), want, got); free(got); } while (0)

int	main (void)
{
	cfg$inimsg();

	/* Valid */
	V("system = { hostname = \"edge-1\"; mgmt = \"10.0.0.1\"; mac = \"00:1b:44:11:3a:b7\"; load = 1; };\n"
	  "ports = ( { id = 1; speed = \"1g\"; mtu = 1500; enabled = true; vlan = 10; } );\n"
	  "vlans = [ 1, 4094 ];\nextra = { anything = \"goes\"; };\n",
	  root_s, 0, STS$K_SUCCESS, "");

	/* Every kind of error, all reported, with positions */
	V("system = {\n"
	  "  hostname = \"a-very-long-hostname\";\n"
	  "  mgmt = \"10.0.0.300\";\n"
	  "  mac = \"00:1b:44:11:3a\";\n"
	  "  load = 1.5;\n"
	  "  old = 1;\n"
	  "  typo = 2;\n"
	  "};\n"
	  "ports = (\n"
	  "  { id = 1; speed = \"7G\"; mtu = 99999; vlan = 3; },\n"
	  "  { id = 1; enabled = \"yes\"; },\n"
	  "  { speed = \"10G\"; }\n"
	  ");\n"
	  "vlans = [ 0, 5000 ];\n",
	  root_s, 0, CFG$_INVALID,
	  "LENGTH 2:3 system.hostname = \"a-very-long-hostname\" is 20 long, allowed 1..16\n"
	  "BADADDR 3:3 system.mgmt = \"10.0.0.300\" is not an IP address\n"
	  "BADADDR 4:3 system.mac = \"00:1b:44:11:3a\" is not a MAC address\n"
	  "RANGE 5:3 system.load = 1.5, allowed 0.0..1.0\n"
	  "DEPRECATED 6:3 system.old\n"
	  "UNKNOWN 7:3 system.typo\n"
	  "BADKWD 10:13 ports.[id=1].speed = \"7G\", allowed: 1G, 10G\n"
	  "RANGE 10:27 ports.[id=1].mtu = 99999, allowed 64..9216\n"
	  "CHECK 10:40 ports.[id=1].vlan: must be even\n"
	  "DUPKEY 11:3 ports.[id=1], the same as line 10\n"
	  "VTYPE 11:13 ports.[id=1].enabled: a boolean expected, found a string\n"
	  "REQUIRED 12:3 ports.[2].id\n"
	  "RANGE 14:11 vlans.[0] = 0, allowed 1..4094\n"
	  "RANGE 14:14 vlans.[1] = 5000, allowed 1..4094\n");

	/* Required at the top, a type that stops the look inside, counts */
	V("ports = ( );\nvlans = 5;\n", root_s, 0, CFG$_INVALID,
	  "COUNT 1:1 ports has 0, allowed 1..4\n"
	  "VTYPE 2:1 vlans: an array expected, found an integer\n"
	  "REQUIRED 1:1 system\n");

	/* A 64-bit value where a 32-bit one is wanted */
	V("system = { hostname = \"h\"; };\nports = ( { id = 5000000000; } );\n", root_s, 0, CFG$_INVALID,
	  "VTYPE 2:13 ports.[id=5000000000L].id: an integer expected, found a 64-bit integer\n");

	/* Names regardless of case when the tree is so */
	V("SYSTEM = { HostName = \"h\"; };\n", root_s, CFG$M_NOCASE, STS$K_SUCCESS, "");

	/* A wrong schema is the programmer's, found before the file is blamed */
	{
		static const CFG$SCHEMA	bad1[] = { { .name = "a", .type = CFG$K_TYPE_KWD }, { .name = NULL } };
		static const CFG$SCHEMA	bad2[] = { { .name = "a", .type = CFG$K_TYPE_INT, .flags = CFG$M_KEY }, { .name = NULL } };
		static const CFG$SCHEMA	bad3[] = { { .name = "a", .type = CFG$K_TYPE_LIST }, { .name = NULL } };
		static const CFG$SCHEMA	bad4[] = { { .name = "a", .type = CFG$K_TYPE_INT, .flags = CFG$M_RANGE, .min = 5, .max = 1 }, { .name = NULL } };
		const CFG$SCHEMA	*bads[] = { bad1, bad2, bad3, bad4 };

		for ( int k = 0; k < 4; k++ )
			V("a = 1;", bads[k], 0, CFG$_BADSCHEMA, "");
	}

	/* More errors than are kept */
	{
		char		txt[4096] = "vlans = [ ";
		CFG$TREE	*t;
		uint32_t	n = 0;

		for ( int k = 0; k < 40; k++ )
			strcat(txt, k ? ", 0" : "0");

		strcat(txt, " ];\nsystem = { hostname = \"h\"; };\n");
		cfg$parse_buf(txt, (uint32_t) strlen(txt), "s.cfg", 0, &t);
		CHECK(cfg$validate(t, root_s, NULL, &n) == CFG$_INVALID, "invalid");
		CHECK(n == 40, "40 diagnostics, got %u", n);
		CHECK((t->nerr == CFG$K_MAXERRS) && (t->ndropped == 40 - CFG$K_MAXERRS), "kept %u dropped %u", t->nerr, t->ndropped);
		cfg$free(t);
	}

	/* The comparison takes the key from the schema */
	{
		static const CFG$SCHEMA	u_s[] = { { .name = "login", .type = CFG$K_TYPE_STRING, .flags = CFG$M_KEY }, { .name = "uid", .type = CFG$K_TYPE_INT }, { .name = NULL } };
		static const CFG$SCHEMA	u_e = { .type = CFG$K_TYPE_GROUP, .sub = u_s };
		static const CFG$SCHEMA	r_s[] = { { .name = "users", .type = CFG$K_TYPE_LIST, .sub = &u_e }, { .name = NULL } };
		const char	*a = "users = ( { login = \"bob\"; uid = 1; }, { login = \"amy\"; uid = 2; } );";
		const char	*b = "users = ( { login = \"amy\"; uid = 2; }, { login = \"bob\"; uid = 3; } );";
		CFG$TREE	*x, *y;
		CFG$DIFF	*d;
		uint32_t	n = 0, op;
		const char	*path;

		cfg$parse_buf(a, (uint32_t) strlen(a), "a", 0, &x);
		cfg$parse_buf(b, (uint32_t) strlen(b), "b", 0, &y);

		CHECK(cfg$diff(x, y, r_s, 0, &d) & 1, "diff");
		cfg$diff_count(d, &n);
		cfg$diff_entry(d, 0, &op, &path, NULL, NULL);
		CHECK((n == 1) && (op == CFG$K_DIFF_MOD) && !strcmp(path, "users.[login=\"bob\"].uid"), "by schema key: n=%u %s", n, n ? path : "");
		cfg$diff_free(d);

		/* Without the schema: no id/name/key - by position */
		CHECK(cfg$diff(x, y, NULL, 0, &d) & 1, "diff");
		cfg$diff_count(d, &n);
		CHECK(n == 4, "by position: n=%u", n);
		cfg$diff_free(d);
		cfg$free(x);
		cfg$free(y);
	}

	/* Findings of the review of the schema code */
	{
		/* A schema entry of another type than the node: no walk past the table */
		static const CFG$SCHEMA	el = { .name = "port", .type = CFG$K_TYPE_GROUP };
		static const CFG$SCHEMA	rs[] = { { .name = "ports", .type = CFG$K_TYPE_LIST, .sub = &el }, { .name = NULL } };
		static const CFG$SCHEMA	gs[] = { { .name = "ports", .type = CFG$K_TYPE_GROUP, .sub = rs }, { .name = NULL } };
		CFG$TREE	*x, *y;
		CFG$DIFF	*d;
		uint32_t	n = 0, op;
		const char	*path;
		char		*txt;
		uint32_t	len;

		cfg$parse_buf("ports = { a = 1; };", (uint32_t) strlen("ports = { a = 1; };"), "a", 0, &x);
		cfg$parse_buf("ports = { a = 2; };", (uint32_t) strlen("ports = { a = 2; };"), "b", 0, &y);
		CHECK(cfg$diff(x, y, rs, 0, &d) & 1, "diff");
		cfg$diff_count(d, &n);
		CHECK(n == 1, "n=%u", n);
		OK(cfg$diff_listing(d, CFG$M_NOTIME, &txt, &len));
		CHECK(strstr(txt, "/SCHEMA-") != NULL, "listing names the schema");
		free(txt);
		cfg$diff_free(d); cfg$free(x); cfg$free(y);

		cfg$parse_buf("ports = ( { a = 1; } );", (uint32_t) strlen("ports = ( { a = 1; } );"), "a", 0, &x);
		cfg$parse_buf("ports = ( { a = 2; } );", (uint32_t) strlen("ports = ( { a = 2; } );"), "b", 0, &y);
		CHECK(cfg$diff(x, y, gs, 0, &d) & 1, "diff");
		cfg$diff_free(d); cfg$free(x); cfg$free(y);

		/* Keys of the schema that repeat: by position */
		cfg$parse_buf("ports = ( { id = 1; mtu = 100; }, { id = 1; mtu = 200; } );", (uint32_t) strlen("ports = ( { id = 1; mtu = 100; }, { id = 1; mtu = 200; } );"), "a", 0, &x);
		cfg$parse_buf("ports = ( { id = 1; mtu = 100; }, { id = 1; mtu = 300; } );", (uint32_t) strlen("ports = ( { id = 1; mtu = 100; }, { id = 1; mtu = 300; } );"), "b", 0, &y);
		CHECK(cfg$diff(x, y, root_s, 0, &d) & 1, "diff");
		cfg$diff_count(d, &n);
		cfg$diff_entry(d, 0, &op, &path, NULL, NULL);
		CHECK((n == 1) && !strcmp(path, "ports.[1].mtu"), "repeated keys: n=%u %s", n, n ? path : "");
		cfg$diff_free(d); cfg$free(x); cfg$free(y);
	}

	/* Keys that are not scalars: their type is wrong, but they are not "the same" */
	V("system = { hostname = \"h\"; };\nports = ( { id = { }; }, { id = { }; }, { id = [ 1 ]; }, { id = [ 2 ]; } );\n", root_s, 0, CFG$_INVALID,
	  "VTYPE 2:13 ports.[0].id: an integer expected, found a group\n"
	  "VTYPE 2:28 ports.[1].id: an integer expected, found a group\n"
	  "VTYPE 2:43 ports.[2].id: an integer expected, found an array\n"
	  "VTYPE 2:60 ports.[3].id: an integer expected, found an array\n");

	/* The check routine does not see a node that failed already */
	{
		static const CFG$SCHEMA	cs[] = { { .name = "v", .type = CFG$K_TYPE_INT, .flags = CFG$M_RANGE, .min = 1, .max = 10, .check = even_check }, { .name = NULL } };
		CFG$TREE	*t;
		int		calls = 0;

		cfg$parse_buf("v = 99;", (uint32_t) strlen("v = 99;"), "c", 0, &t);
		CHECK(cfg$validate(t, cs, &calls, NULL) == CFG$_INVALID, "invalid");
		CHECK((calls == 0) && (t->nerr == 1), "check called %d times, %u diagnostics", calls, t->nerr);
		cfg$free(t);
	}

	/* DUPKEY with an element the program added */
	{
		CFG$TREE	*t;
		CFG$NODE	*r, *pl, *e;

		cfg$parse_buf("system = { hostname = \"h\"; };\nports = ( { id = 1; } );", (uint32_t) strlen("system = { hostname = \"h\"; };\nports = ( { id = 1; } );"), "s.cfg", 0, &t);
		cfg$root(t, &r);
		cfg$lookup(r, "ports", &pl);
		OK(cfg$add(pl, NULL, CFG$K_TYPE_GROUP, &e)); OK(cfg$set_int(e, "id", 2));
		OK(cfg$add(pl, NULL, CFG$K_TYPE_GROUP, &e)); OK(cfg$set_int(e, "id", 2));
		CHECK(cfg$validate(t, root_s, NULL, NULL) == CFG$_INVALID, "invalid");
		CHECK((t->nerr == 1) && strstr(t->err[0].text, "the same as an element added by the program"), "%.*s", (int) t->err[0].len, t->err[0].text);
		cfg$free(t);
	}

	/* A long key keeps the path whole (as far as the record of a diagnostic keeps it) */
	{
		static const CFG$SCHEMA	ns[] = { { .name = "name", .type = CFG$K_TYPE_STRING, .flags = CFG$M_KEY }, { .name = "zz", .type = CFG$K_TYPE_BOOL }, { .name = NULL } };
		static const CFG$SCHEMA	ne = { .type = CFG$K_TYPE_GROUP, .sub = ns };
		static const CFG$SCHEMA	ls[] = { { .name = "l", .type = CFG$K_TYPE_LIST, .sub = &ne }, { .name = NULL } };
		char		txt[512], key[201];
		CFG$TREE	*t;

		memset(key, '0', 200); key[200] = 0;
		snprintf(txt, sizeof(txt), "l = ( { name = \"%s\"; zz = 1; } );", key);
		cfg$parse_buf(txt, (uint32_t) strlen(txt), "k", 0, &t);
		CHECK(cfg$validate(t, ls, NULL, NULL) == CFG$_INVALID, "invalid");
		CHECK((t->nerr == 1) && (t->err[0].len == 160), "cut to the record");
		cfg$free(t);
	}

	/* A schema that refers to itself: menus of menus */
	{
		extern const CFG$SCHEMA	menu_s[];
		static const CFG$SCHEMA	item = { .type = CFG$K_TYPE_GROUP, .sub = menu_s };
		static const CFG$SCHEMA	top[] = { { .name = "menu", .type = CFG$K_TYPE_GROUP, .sub = menu_s }, { .name = NULL } };
		const char	*txt = "menu = { title = \"a\"; items = ( { title = \"b\"; items = ( { title = \"c\"; bad = 1; } ); } ); };";
		CFG$TREE	*t;

		(void) item;
		cfg$parse_buf(txt, (uint32_t) strlen(txt), "m", 0, &t);
		CHECK(cfg$validate(t, top, NULL, NULL) == CFG$_INVALID, "self-referencing schema");
		CHECK((t->nerr == 1) && strstr(t->err[0].text, "menu.items.[0].items.[0].bad"), "%.*s", (int) t->err[0].len, t->err[0].text);
		cfg$free(t);
	}

	/* A table of 16 groups that all refer to the table: checked in no time */
	{
		static CFG$SCHEMA	wide[17];
		static char		names[16][4];
		CFG$TREE		*t;

		for ( int k = 0; k < 16; k++ )
			{
			snprintf(names[k], sizeof(names[k]), "m%d", k);
			wide[k].name = names[k];
			wide[k].type = CFG$K_TYPE_GROUP;
			wide[k].sub = wide;
			}

		cfg$parse_buf("m0 = { m1 = { }; };", 19, "w", 0, &t);
		CHECK(cfg$validate(t, wide, NULL, NULL) & 1, "wide self-referencing schema");
		cfg$free(t);
	}

	/* The check routine of an element whose key repeats is not called */
	{
		static const CFG$SCHEMA	ks[] = { { .name = "id", .type = CFG$K_TYPE_INT, .flags = CFG$M_KEY }, { .name = NULL } };
		static const CFG$SCHEMA	ke = { .type = CFG$K_TYPE_GROUP, .sub = ks, .check = even_check };
		static const CFG$SCHEMA	kl[] = { { .name = "l", .type = CFG$K_TYPE_LIST, .sub = &ke }, { .name = NULL } };
		CFG$TREE	*t;
		int		calls = 0;

		cfg$parse_buf("l = ( { id = 2; }, { id = 2; } );", 33, "k", 0, &t);
		CHECK(cfg$validate(t, kl, &calls, NULL) == CFG$_INVALID, "dupkey");
		CHECK(calls == 1, "check called %d times, want 1", calls);
		cfg$free(t);
	}

	/* CFG$M_FALLBACK: a bad value is a warning, GET gives CFG$_FALLBACK, the rest is checked as before */
	{
		static const CFG$KEYWORD	par[] = { { "none", 0 }, { "even", 2 }, { NULL, 0 } };
		static const CFG$SCHEMA	fs[] = {
			{ .name = "speed",  .type = CFG$K_TYPE_INT, .flags = CFG$M_FALLBACK | CFG$M_RANGE, .min = 300, .max = 4000000 },
			{ .name = "parity", .type = CFG$K_TYPE_KWD, .flags = CFG$M_FALLBACK, .kwd = par },
			{ .name = "tmo",    .type = CFG$K_TYPE_INT, .flags = CFG$M_FALLBACK, .check = even_check },
			{ .name = "addr",   .type = CFG$K_TYPE_IPV4, .flags = CFG$M_FALLBACK },
			{ .name = "port",   .type = CFG$K_TYPE_INT, .flags = CFG$M_RANGE, .min = 1, .max = 65535 },
			{ .name = NULL } };
		static const CFG$SCHEMA	fr[] = { { .name = "s", .type = CFG$K_TYPE_GROUP, .sub = fs }, { .name = NULL } };
		const char	*good = "s = { speed = 9600; parity = \"even\"; tmo = 4; addr = \"10.0.0.1\"; port = 502; };";
		const char	*soft = "s = { speed = 12; parity = \"odd\"; tmo = 3; addr = \"fast\"; port = 502; };";
		const char	*vtyp = "s = { speed = \"fast\"; };";
		const char	*hard = "s = { speed = 12; port = 0; };";
		CFG$TREE	*t;
		CFG$NODE	*r;
		int32_t		v = -1;
		const char	*str;
		uint32_t	len, ne = 9, nw = 9, nd = 0;
		int		calls = 0;

		V(soft, fr, 0, STS$K_SUCCESS,
		  "RANGE 1:7 s.speed = 12, allowed 300..4000000 - the default is taken\n"
		  "BADKWD 1:19 s.parity = \"odd\", allowed: none, even - the default is taken\n"
		  "CHECK 1:35 s.tmo: must be even - the default is taken\n"
		  "BADADDR 1:44 s.addr = \"fast\" is not an IPv4 address - the default is taken\n");

		/* The status of each: warnings; GET: CFG$_FALLBACK for the bad ones, the good one read */
		cfg$parse_buf(soft, (uint32_t) strlen(soft), "f", 0, &t);
		cfg$root(t, &r);
		CHECK(cfg$validate(t, fr, &calls, &nd) == STS$K_SUCCESS, "soft: success");
		CHECK(nd == 4, "soft: 4 diagnostics, %u", nd);
		CHECK((cfg$diagcount(t, &ne, &nw) & 1) && !ne && (nw == 4), "diagcount soft: %u errors, %u warnings", ne, nw);
		for ( uint32_t i = 0; i < t->nerr; i++ )
			CHECK($SEV(t->err[i].sts) == STS$K_WARN, "soft: diagnostic %u is a warning, %08X", i, t->err[i].sts);
		CHECK(cfg$get_int(r, "s.speed", &v) == CFG$_FALLBACK, "get speed: FALLBACK");
		CHECK(v == 12, "get speed: the value is given all the same, %d", v);
		CHECK((cfg$get_string(r, "s.parity", &str, &len) == CFG$_FALLBACK) && (len == 3) && !memcmp(str, "odd", 3), "get parity: FALLBACK, \"odd\"");
		CHECK(cfg$get_bool(r, "s.speed", &calls) == CFG$_FALLBACK, "get speed as a boolean: FALLBACK, not TYPE");
		CHECK(cfg$get_int(r, "s.tmo", &v) == CFG$_FALLBACK, "get tmo: FALLBACK");
		CHECK(cfg$get_string(r, "s.addr", &str, &len) == CFG$_FALLBACK, "get addr: FALLBACK");
		CHECK((cfg$get_int(r, "s.port", &v) & 1) && (v == 502), "get port: 502");

		/* A value the program sets is good again; a validation of a mended tree forgets the marks */
		OK(cfg$set_int(r, "s.speed", 19200));
		CHECK((cfg$get_int(r, "s.speed", &v) & 1) && (v == 19200), "set speed: read back");
		OK(cfg$set_string(r, "s.parity", "even", 4));
		OK(cfg$set_int(r, "s.tmo", 6));
		OK(cfg$set_string(r, "s.addr", "10.1.1.1", 8));
		CHECK(cfg$get_string(r, "s.addr", &str, &len) & 1, "set addr: read back");
		CHECK((cfg$validate(t, fr, &calls, &nd) & 1) && !nd, "mended: no diagnostics, %u", nd);
		cfg$free(t);

		/* A type of another kind: the same, and it stays marked till a validation finds it good */
		cfg$parse_buf(vtyp, (uint32_t) strlen(vtyp), "f", 0, &t);
		cfg$root(t, &r);
		CHECK(cfg$validate(t, fr, NULL, NULL) == STS$K_SUCCESS, "vtype: success");
		CHECK((t->nerr == 1) && strstr(t->err[0].text, "an integer expected, found a string - the default is taken"), "%.*s", (int) t->err[0].len, t->err[0].text);
		v = -1;
		CHECK((cfg$get_int(r, "s.speed", &v) == CFG$_FALLBACK) && (v == -1), "vtype get: FALLBACK, no value");
		cfg$free(t);

		/* A setting without the flag is still an error, the soft one beside it still a warning */
		V(hard, fr, 0, CFG$_INVALID,
		  "RANGE 1:7 s.speed = 12, allowed 300..4000000 - the default is taken\n"
		  "RANGE 1:19 s.port = 0, allowed 1..65535\n");
		cfg$parse_buf(hard, (uint32_t) strlen(hard), "f", 0, &t);
		cfg$validate(t, fr, NULL, NULL);
		CHECK((cfg$diagcount(t, &ne, &nw) & 1) && (ne == 1) && (nw == 1), "diagcount hard: %u errors, %u warnings", ne, nw);
		cfg$free(t);

		/* All good: nothing told, nothing marked */
		V(good, fr, 0, STS$K_SUCCESS, "");

		/* The flag is for a scalar, not for the key of an element; a required setting may have it */
		{
			static const CFG$SCHEMA	b1[] = { { .name = "g", .type = CFG$K_TYPE_GROUP, .flags = CFG$M_FALLBACK, .sub = fs }, { .name = NULL } };
			static const CFG$SCHEMA	k2[] = { { .name = "id", .type = CFG$K_TYPE_INT, .flags = CFG$M_FALLBACK | CFG$M_KEY }, { .name = NULL } };
			static const CFG$SCHEMA	ke2 = { .type = CFG$K_TYPE_GROUP, .sub = k2 };
			static const CFG$SCHEMA	b2[] = { { .name = "l", .type = CFG$K_TYPE_LIST, .sub = &ke2 }, { .name = NULL } };
			static const CFG$SCHEMA	rq[] = { { .name = "x", .type = CFG$K_TYPE_INT, .flags = CFG$M_FALLBACK | CFG$M_REQUIRED | CFG$M_RANGE, .min = 1, .max = 9 }, { .name = NULL } };

			cfg$parse_buf(good, (uint32_t) strlen(good), "f", 0, &t);
			CHECK(cfg$validate(t, b1, NULL, NULL) == CFG$_BADSCHEMA, "FALLBACK on a group: BADSCHEMA");
			CHECK(cfg$validate(t, b2, NULL, NULL) == CFG$_BADSCHEMA, "FALLBACK on a key: BADSCHEMA");
			cfg$free(t);

			V("x = 0;", rq, 0, STS$K_SUCCESS, "RANGE 1:1 x = 0, allowed 1..9 - the default is taken\n");
			V("y = 1;", rq, 0, CFG$_INVALID, "UNKNOWN 1:1 y\nREQUIRED 1:1 x\n");
		}
	}

	/* A long text of a CFG$M_FALLBACK setting is cut so that its end - what comes of it - stays */
	{
		static const CFG$KEYWORD	lk[] = { { "alpha-alpha-alpha-alpha", 1 }, { "bravo-bravo-bravo-bravo", 2 }, { "charlie-charlie-charlie", 3 },
						 { "delta-delta-delta-delta", 4 }, { "echo-echo-echo-echo-echo", 5 }, { "foxtrot-foxtrot-foxtrot", 6 }, { NULL, 0 } };
		static const CFG$SCHEMA	ls[] = { { .name = "k", .type = CFG$K_TYPE_KWD, .flags = CFG$M_FALLBACK, .kwd = lk }, { .name = NULL } };
		static const char	sfx[] = "... - the default is taken";
		CFG$TREE	*t;

		cfg$parse_buf("k = \"zulu\";", 11, "c", 0, &t);
		CHECK(cfg$validate(t, ls, NULL, NULL) == STS$K_SUCCESS, "long soft: success");
		CHECK((t->nerr == 1) && (t->err[0].len == 160) && !memcmp(t->err[0].text + 160 - (sizeof(sfx) - 1), sfx, sizeof(sfx) - 1),
			"long soft: %u %.*s", t->err[0].len, (int) t->err[0].len, t->err[0].text);
		cfg$free(t);
	}

	/* A full record of warnings: an error still gets in, in the place of the last warning */
	{
		static const CFG$SCHEMA	ws[] = { { .name = "*", .type = CFG$K_TYPE_INT, .flags = CFG$M_FALLBACK | CFG$M_RANGE, .min = 0, .max = 1 }, { .name = NULL } };
		static const CFG$SCHEMA	wr[] = { { .name = "g", .type = CFG$K_TYPE_GROUP, .sub = ws }, { .name = "e", .type = CFG$K_TYPE_BOOL }, { .name = NULL } };
		char		txt[2048];
		size_t		l = 0;
		uint32_t	ne = 0, nw = 0;
		CFG$TREE	*t;

		l += (size_t) snprintf(txt + l, sizeof(txt) - l, "g = { ");
		for ( int k = 0; k < 37; k++ )
			l += (size_t) snprintf(txt + l, sizeof(txt) - l, "s%d = 5; ", k);
		l += (size_t) snprintf(txt + l, sizeof(txt) - l, "};\ne = 1;\n");
		cfg$parse_buf(txt, (uint32_t) l, "w", 0, &t);
		CHECK(cfg$validate(t, wr, NULL, NULL) == CFG$_INVALID, "full: INVALID");
		CHECK((t->nerr == CFG$K_MAXERRS) && ($SEV(t->err[CFG$K_MAXERRS - 1].sts) == STS$K_ERROR), "full: the error is kept last");
		CHECK((cfg$diagcount(t, &ne, &nw) & 1) && (ne == 1) && (nw == 37) && (t->ndropped == 6), "full: %u errors, %u warnings, %u dropped", ne, nw, t->ndropped);
		cfg$free(t);
	}

	/* The cut of a long soft text does not split a UTF-8 character */
	{
		static const CFG$SCHEMA	us[] = { { .name = "u", .type = CFG$K_TYPE_STRING, .flags = CFG$M_FALLBACK | CFG$M_LENGTH, .min = 0, .max = 4 }, { .name = NULL } };
		char		txt[512];
		size_t		l = 0;
		CFG$TREE	*t;

		l += (size_t) snprintf(txt + l, sizeof(txt) - l, "u = \"");
		for ( int k = 0; k < 90; k++ )
			l += (size_t) snprintf(txt + l, sizeof(txt) - l, "\xD0\x96");		/* Zhe, two octets */
		l += (size_t) snprintf(txt + l, sizeof(txt) - l, "\";");
		cfg$parse_buf(txt, (uint32_t) l, "u", 0, &t);
		CHECK(cfg$validate(t, us, NULL, NULL) == STS$K_SUCCESS, "utf8: success");
		{
			const char	*e = strstr(t->err[0].text, "...");
			CHECK(e && ((uint8_t) e[-1] == 0x96), "utf8: the cut is after a whole character");
		}
		cfg$free(t);
	}

	/* A FATAL status of a check routine refuses even a CFG$M_FALLBACK setting */
	{
		static const CFG$SCHEMA	fs[] = { { .name = "x", .type = CFG$K_TYPE_INT, .flags = CFG$M_FALLBACK, .check = fatal_check }, { .name = NULL } };
		CFG$TREE	*t;

		cfg$parse_buf("x = 1;", 6, "f", 0, &t);
		CHECK(cfg$validate(t, fs, NULL, NULL) == CFG$_INVALID, "fatal check: INVALID");
		CHECK((t->nerr == 1) && !strstr(t->err[0].text, "default"), "%.*s", (int) t->err[0].len, t->err[0].text);
		cfg$free(t);
	}

	return t_report("t_schema");
}
