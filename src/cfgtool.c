#define	__MODULE__	"CFGTOOL"
#define	__IDENT__	"X.01-01"
#define	__REV__		"1.1.0"
#define	__FAC__		"CFG"

/*
**++
**
**  FACILITY:	yaLIBCFG - configuration trees in the libconfig syntax
**
**  MODULE:	cfgtool.c
**
**  ABSTRACT:	CFGTOOL - the command-line utility of the library: check, format, compare,
**		read and change configuration files.
**
**  DESCRIPTION: A DCL-style command line, parsed by CLI_ROUTINES of StarLet:
**
**		    CFGTOOL CHECK   file                    [/NOCASE /NOINCLUDE /FINGERPRINT]
**		    CFGTOOL FORMAT  file                    [/OUTPUT=file /NOCASE /NOINCLUDE]
**		    CFGTOOL DIFF    old-file new-file       [/OUTPUT=file /POSITIONAL /NOTIME /NOCASE]
**		    CFGTOOL GET     file path               [/FLAT /NOCASE]
**		    CFGTOOL SET     file path value         [/TYPE=AUTO|STRING /OUTPUT=file /LOG /NOCASE]
**		    CFGTOOL DELETE  file path               [/OUTPUT=file /LOG /NOCASE]
**		    CFGTOOL HELP
**
**		Messages go to the standard error, through $PUTMSG_FAO; data - a value, a
**		listing, a canonical text, a fingerprint - to the standard output (or /OUTPUT).
**		SET and DELETE write the file back in place, keeping its comments and layout.
**
**		The exit status for the shell: 0 - success (DIFF: no differences), 1 - DIFF found
**		differences, 2 - an error.
**
**  AUTHOR:	StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
**
**  CREATION DATE:  22-NOV-2026
**
**  MODIFICATION HISTORY:
**
**	X.01-01		22-NOV-2026	RRL
**		First edition: CHECK, FORMAT, DIFF, GET, SET, DELETE, HELP.
**
**--
*/

#define	_GNU_SOURCE
#include	<stdlib.h>
#include	<string.h>
#include	<errno.h>
#include	<fcntl.h>
#include	<unistd.h>

#include	"yalibcfg.h"
#include	"cli_routines.h"

#define	CFGTOOL$K_EXIT_OK	0		/* success; DIFF: no differences		*/
#define	CFGTOOL$K_EXIT_DIFF	1		/* DIFF: there are differences			*/
#define	CFGTOOL$K_EXIT_ERROR	2		/* an error					*/

/*
**  A value of the command line, zero-terminated (an ASC holds up to 255 octets)
*/
typedef	struct cfgtool_arg_t {
	char		str[ASC$K_SZ + 1];
	uint32_t	len;
} CFGTOOL$ARG;

static	CLI_KEYWORD	s_types [] = {
	{ .name = {$ASCINI("AUTO")},   .val = 0 },
	{ .name = {$ASCINI("STRING")}, .val = 1 },
	{ .name = {0} }
	};

/* The parameters and qualifiers of every verb: each verb its own, CLI$PRESENT asks by address */

static	CLI_PQDESC	s_check_p [] = {
	{ .name = {$ASCINI("file")}, .type = CLI$K_FILE, .pn = CLI$K_P1, .flag = CLI$M_REQUIRED },
	{ .name = {0} }
	};
static	CLI_PQDESC	s_check_q [] = {
	{ .name = {$ASCINI("nocase")},      .type = CLI$K_OPT, .pn = CLI$K_QUAL },
	{ .name = {$ASCINI("include")},     .type = CLI$K_OPT, .pn = CLI$K_QUAL, .flag = CLI$M_NEGATABLE },
	{ .name = {$ASCINI("fingerprint")}, .type = CLI$K_OPT, .pn = CLI$K_QUAL },
	{ .name = {0} }
	};

static	CLI_PQDESC	s_format_p [] = {
	{ .name = {$ASCINI("file")}, .type = CLI$K_FILE, .pn = CLI$K_P1, .flag = CLI$M_REQUIRED },
	{ .name = {0} }
	};
static	CLI_PQDESC	s_format_q [] = {
	{ .name = {$ASCINI("nocase")},  .type = CLI$K_OPT,  .pn = CLI$K_QUAL },
	{ .name = {$ASCINI("include")}, .type = CLI$K_OPT,  .pn = CLI$K_QUAL, .flag = CLI$M_NEGATABLE },
	{ .name = {$ASCINI("output")},  .type = CLI$K_FILE, .pn = CLI$K_QUAL },
	{ .name = {0} }
	};

static	CLI_PQDESC	s_diff_p [] = {
	{ .name = {$ASCINI("old-file")}, .type = CLI$K_FILE, .pn = CLI$K_P1, .flag = CLI$M_REQUIRED },
	{ .name = {$ASCINI("new-file")}, .type = CLI$K_FILE, .pn = CLI$K_P2, .flag = CLI$M_REQUIRED },
	{ .name = {0} }
	};
static	CLI_PQDESC	s_diff_q [] = {
	{ .name = {$ASCINI("nocase")},     .type = CLI$K_OPT,  .pn = CLI$K_QUAL },
	{ .name = {$ASCINI("output")},     .type = CLI$K_FILE, .pn = CLI$K_QUAL },
	{ .name = {$ASCINI("positional")}, .type = CLI$K_OPT,  .pn = CLI$K_QUAL },
	{ .name = {$ASCINI("notime")},     .type = CLI$K_OPT,  .pn = CLI$K_QUAL },
	{ .name = {0} }
	};

static	CLI_PQDESC	s_get_p [] = {
	{ .name = {$ASCINI("file")}, .type = CLI$K_FILE,    .pn = CLI$K_P1, .flag = CLI$M_REQUIRED },
	{ .name = {$ASCINI("path")}, .type = CLI$K_QSTRING, .pn = CLI$K_P2, .flag = CLI$M_REQUIRED },
	{ .name = {0} }
	};
static	CLI_PQDESC	s_get_q [] = {
	{ .name = {$ASCINI("nocase")}, .type = CLI$K_OPT, .pn = CLI$K_QUAL },
	{ .name = {$ASCINI("flat")},   .type = CLI$K_OPT, .pn = CLI$K_QUAL },
	{ .name = {0} }
	};

static	CLI_PQDESC	s_set_p [] = {
	{ .name = {$ASCINI("file")},  .type = CLI$K_FILE,    .pn = CLI$K_P1, .flag = CLI$M_REQUIRED },
	{ .name = {$ASCINI("path")},  .type = CLI$K_QSTRING, .pn = CLI$K_P2, .flag = CLI$M_REQUIRED },
	{ .name = {$ASCINI("value")}, .type = CLI$K_QSTRING, .pn = CLI$K_P3, .flag = CLI$M_REQUIRED },
	{ .name = {0} }
	};
static	CLI_PQDESC	s_set_q [] = {
	{ .name = {$ASCINI("nocase")}, .type = CLI$K_OPT,  .pn = CLI$K_QUAL },
	{ .name = {$ASCINI("type")},   .type = CLI$K_KWD,  .pn = CLI$K_QUAL, .kwd = s_types, .defval = {$ASCINI("AUTO")} },
	{ .name = {$ASCINI("output")}, .type = CLI$K_FILE, .pn = CLI$K_QUAL },
	{ .name = {$ASCINI("log")},    .type = CLI$K_OPT,  .pn = CLI$K_QUAL },
	{ .name = {0} }
	};

static	CLI_PQDESC	s_delete_p [] = {
	{ .name = {$ASCINI("file")}, .type = CLI$K_FILE,    .pn = CLI$K_P1, .flag = CLI$M_REQUIRED },
	{ .name = {$ASCINI("path")}, .type = CLI$K_QSTRING, .pn = CLI$K_P2, .flag = CLI$M_REQUIRED },
	{ .name = {0} }
	};
static	CLI_PQDESC	s_delete_q [] = {
	{ .name = {$ASCINI("nocase")}, .type = CLI$K_OPT,  .pn = CLI$K_QUAL },
	{ .name = {$ASCINI("output")}, .type = CLI$K_FILE, .pn = CLI$K_QUAL },
	{ .name = {$ASCINI("log")},    .type = CLI$K_OPT,  .pn = CLI$K_QUAL },
	{ .name = {0} }
	};

static	int	s_do_check	(CLI_CTX *a_ctx, void *a_arg);
static	int	s_do_format	(CLI_CTX *a_ctx, void *a_arg);
static	int	s_do_diff	(CLI_CTX *a_ctx, void *a_arg);
static	int	s_do_get	(CLI_CTX *a_ctx, void *a_arg);
static	int	s_do_set	(CLI_CTX *a_ctx, void *a_arg);
static	int	s_do_delete	(CLI_CTX *a_ctx, void *a_arg);
static	int	s_do_help	(CLI_CTX *a_ctx, void *a_arg);

static	CLI_VERB	s_verbs [] = {
	{ .name = {$ASCINI("check")},  .params = s_check_p,  .quals = s_check_q,  .act_rtn = s_do_check },
	{ .name = {$ASCINI("format")}, .params = s_format_p, .quals = s_format_q, .act_rtn = s_do_format },
	{ .name = {$ASCINI("diff")},   .params = s_diff_p,   .quals = s_diff_q,   .act_rtn = s_do_diff },
	{ .name = {$ASCINI("get")},    .params = s_get_p,    .quals = s_get_q,    .act_rtn = s_do_get },
	{ .name = {$ASCINI("set")},    .params = s_set_p,    .quals = s_set_q,    .act_rtn = s_do_set },
	{ .name = {$ASCINI("delete")}, .params = s_delete_p, .quals = s_delete_q, .act_rtn = s_do_delete },
	{ .name = {$ASCINI("help")},   .act_rtn = s_do_help },
	{ .name = {0} }
	};



/*
**  DESCRIPTION: The value of a parameter or a qualifier, zero-terminated.
**
**  RETURNS:
**	the condition value of __CLI$GET_VALUE
*/
static unsigned	s_arg	(
		CLI_CTX		*a_ctx,
		CLI_PQDESC	*a_pq,
		CFGTOOL$ARG	*a_arg
			)
{
ASC		l_val;
unsigned	l_sts;

	a_arg->len = 0;
	a_arg->str[0] = '\0';

	if ( !(1 & (l_sts = (unsigned) __cli$get_value(a_ctx, a_pq, &l_val))) )
		return	l_sts;

	memcpy(a_arg->str, l_val.sts, l_val.len);
	a_arg->str[l_val.len] = '\0';
	a_arg->len = l_val.len;

	return	STS$K_SUCCESS;
}

static int	s_present	(
		CLI_CTX		*a_ctx,
		CLI_PQDESC	*a_pq
			)
{
	return	1 & __cli$present(a_ctx, a_pq, NULL);
}

/*
**  DESCRIPTION: Is a negatable qualifier given in its /NO form?
*/
static int	s_negated	(
		CLI_CTX		*a_ctx,
		CLI_PQDESC	*a_pq
			)
{
unsigned	l_state = 0;

	return	(1 & __cli$present(a_ctx, a_pq, &l_state)) && (l_state & CLI$M_NEGATED);
}



/*
**  DESCRIPTION: Write data to a file descriptor, all of it.
**
**  RETURNS:
**	STS$K_SUCCESS, CFG$_WRITERR (put out)
*/
static unsigned	s_write	(
		int		a_fd,
	const	char		*a_buf,
		uint32_t	a_len,
	const	char		*a_name
			)
{
ssize_t		l_rc;

	while ( a_len )
		{
		if ( 0 > (l_rc = write(a_fd, a_buf, a_len)) )
			{
			if ( errno == EINTR )
				continue;

			return	$PUTMSG_FAO(CFG$_WRITERR, strlen(a_name), a_name, errno);
			}

		a_buf += l_rc;
		a_len -= (uint32_t) l_rc;
		}

	return	STS$K_SUCCESS;
}



/*
**  DESCRIPTION: Put data out: into the file of /OUTPUT when given, else to the standard output.
**
**  RETURNS:
**	STS$K_SUCCESS, CFG$_OPENOUT, CFG$_WRITERR (put out)
*/
static unsigned	s_output	(
		CLI_CTX		*a_ctx,
		CLI_PQDESC	*a_qout,
	const	char		*a_buf,
		uint32_t	a_len
			)
{
CFGTOOL$ARG	l_out;
int		l_fd;
unsigned	l_sts;

	if ( !a_qout || !s_present(a_ctx, a_qout) || !(1 & s_arg(a_ctx, a_qout, &l_out)) )
		return	s_write(STDOUT_FILENO, a_buf, a_len, "SYS$OUTPUT");

	if ( 0 > (l_fd = open(l_out.str, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644)) )
		return	$PUTMSG_FAO(CFG$_OPENOUT, l_out.str, errno);

	l_sts = s_write(l_fd, a_buf, a_len, l_out.str);

	if ( close(l_fd) && (1 & l_sts) )
		l_sts = $PUTMSG_FAO(CFG$_WRITERR, strlen(l_out.str), l_out.str, errno);

	return	l_sts;
}



/*
**  DESCRIPTION: Parse a file; the diagnostics put out when it fails.
**
**  INPUTS:
**	a_path:		the file
**	a_ctx:		the command, for /NOCASE and /NOINCLUDE
**	a_qnocase:	the /NOCASE qualifier of the verb
**	a_qinclude:	the /[NO]INCLUDE qualifier, or NULL
**
**  OUTPUTS:
**	a_tree:		the tree; NULL after a failure (freed here)
**
**  RETURNS:
**	the condition value of the parse
*/
static unsigned	s_load	(
	const	char		*a_path,
		CLI_CTX		*a_ctx,
		CLI_PQDESC	*a_qnocase,
		CLI_PQDESC	*a_qinclude,
		CFG$TREE	**a_tree
			)
{
uint32_t	l_opts = 0;
unsigned	l_sts;

	if ( a_qnocase && s_present(a_ctx, a_qnocase) )
		l_opts |= CFG$M_NOCASE;

	if ( a_qinclude && s_negated(a_ctx, a_qinclude) )
		l_opts |= CFG$M_NOINCLUDE;

	if ( !(1 & (l_sts = cfg$parse_file(a_path, l_opts, a_tree))) )
		{
		cfg$puterrs(*a_tree);
		cfg$free(*a_tree);
		*a_tree = NULL;
		}

	return	l_sts;
}



/*
**  DESCRIPTION: The number of the settings of a tree: named nodes, at every level.
*/
static uint32_t	s_count	(
		CFG$NODE	*a_node
			)
{
CFG$NODE	*l_c;
CFG$INFO	l_info;
uint32_t	l_n = 0;

	for ( unsigned l_sts = cfg$first(a_node, &l_c); 1 & l_sts; l_sts = cfg$next(l_c, &l_c) )
		{
		cfg$node_info(l_c, &l_info);

		if ( l_info.name )
			l_n++;

		if ( (l_info.type == CFG$K_TYPE_GROUP) || (l_info.type == CFG$K_TYPE_LIST) )
			l_n += s_count(l_c);
		}

	return	l_n;
}



/*
**  DESCRIPTION: CHECK file - parse it, say it is correct, with its fingerprint.
*/
static int	s_do_check	(
		CLI_CTX		*a_ctx,
		void		*a_arg
			)
{
CFGTOOL$ARG	l_file;
CFG$TREE	*l_tree;
CFG$NODE	*l_root;
uint8_t		l_uuid[16];
char		l_line[64];
fao_desc_t	l_dsc = $FAO_DESC(l_line);
unsigned short	l_len = 0;
unsigned	l_sts;

	s_arg(a_ctx, &s_check_p[0], &l_file);

	if ( !(1 & (l_sts = s_load(l_file.str, a_ctx, &s_check_q[0], &s_check_q[1], &l_tree))) )
		return	(int) l_sts;

	cfg$root(l_tree, &l_root);

	if ( 1 & (l_sts = cfg$fingerprint(l_tree, l_uuid)) )
		{
		$PUTMSG_FAO(CFG$_CORRECT, l_file.str, s_count(l_root), l_uuid);

		/* The fingerprint alone, for a script */
		if ( s_present(a_ctx, &s_check_q[2]) )
			{
			$FAO("!%U\n", &l_len, &l_dsc, l_uuid);
			l_sts = s_write(STDOUT_FILENO, l_line, l_len, "SYS$OUTPUT");
			}
		}

	cfg$free(l_tree);

	return	(int) l_sts;

	(void) a_arg;
}



/*
**  DESCRIPTION: FORMAT file - the canonical text of the settings (comments are not kept).
*/
static int	s_do_format	(
		CLI_CTX		*a_ctx,
		void		*a_arg
			)
{
CFGTOOL$ARG	l_file;
CFG$TREE	*l_tree;
char		*l_text;
uint32_t	l_len;
unsigned	l_sts;

	s_arg(a_ctx, &s_format_p[0], &l_file);

	if ( !(1 & (l_sts = s_load(l_file.str, a_ctx, &s_format_q[0], &s_format_q[1], &l_tree))) )
		return	(int) l_sts;

	if ( 1 & (l_sts = cfg$write_canonical(l_tree, &l_text, &l_len)) )
		{
		l_sts = s_output(a_ctx, &s_format_q[2], l_text, l_len);
		free(l_text);
		}
	else	$PUTMSG_FAO(l_sts);

	cfg$free(l_tree);

	return	(int) l_sts;

	(void) a_arg;
}



/*
**  DESCRIPTION: DIFF old new - the listing of the differences of the settings.
**
**  RETURNS:
**	CFG$_IDENTICAL, CFG$_DIFFERENT, or an error
*/
static int	s_do_diff	(
		CLI_CTX		*a_ctx,
		void		*a_arg
			)
{
CFGTOOL$ARG	l_old, l_new;
CFG$TREE	*l_told = NULL, *l_tnew = NULL;
CFG$DIFF	*l_diff = NULL;
char		*l_text;
uint32_t	l_len, l_count = 0;
unsigned	l_sts;

	s_arg(a_ctx, &s_diff_p[0], &l_old);
	s_arg(a_ctx, &s_diff_p[1], &l_new);

	if ( !(1 & (l_sts = s_load(l_old.str, a_ctx, &s_diff_q[0], NULL, &l_told))) )
		return	(int) l_sts;

	if ( !(1 & (l_sts = s_load(l_new.str, a_ctx, &s_diff_q[0], NULL, &l_tnew))) )
		goto	l_done;

	if ( !(1 & (l_sts = cfg$diff(l_told, l_tnew, NULL, s_present(a_ctx, &s_diff_q[2]) ? CFG$M_POSITIONAL : 0, &l_diff))) )
		{
		$PUTMSG_FAO(l_sts);
		goto	l_done;
		}

	cfg$diff_count(l_diff, &l_count);

	if ( !(1 & (l_sts = cfg$diff_listing(l_diff, s_present(a_ctx, &s_diff_q[3]) ? CFG$M_NOTIME : 0, &l_text, &l_len))) )
		{
		$PUTMSG_FAO(l_sts);
		goto	l_done;
		}

	l_sts = s_output(a_ctx, &s_diff_q[1], l_text, l_len);
	free(l_text);

	if ( 1 & l_sts )
		l_sts = l_count ? $PUTMSG_FAO(CFG$_DIFFERENT, l_count, l_old.str, l_new.str) : $PUTMSG_FAO(CFG$_IDENTICAL, l_old.str, l_new.str);

l_done:
	cfg$diff_free(l_diff);
	cfg$free(l_told);
	cfg$free(l_tnew);

	return	(int) l_sts;

	(void) a_arg;
}



/*
**  DESCRIPTION: GET file path - the value: a string as it is (escapes resolved, no quotes), any
**		other value in its canonical text, a group or a list over lines (/FLAT - on one).
*/
static int	s_do_get	(
		CLI_CTX		*a_ctx,
		void		*a_arg
			)
{
CFGTOOL$ARG	l_file, l_path;
CFG$TREE	*l_tree;
CFG$NODE	*l_root, *l_node;
CFG$INFO	l_info;
char		*l_text;
const	char	*l_str;
uint32_t	l_len;
unsigned	l_sts;

	s_arg(a_ctx, &s_get_p[0], &l_file);
	s_arg(a_ctx, &s_get_p[1], &l_path);

	if ( !(1 & (l_sts = s_load(l_file.str, a_ctx, &s_get_q[0], NULL, &l_tree))) )
		return	(int) l_sts;

	cfg$root(l_tree, &l_root);

	if ( !(1 & cfg$lookup(l_root, l_path.str, &l_node)) )
		{
		l_sts = $PUTMSG_FAO(CFG$_NOSETTING, l_path.str, l_file.str);
		goto	l_done;
		}

	cfg$node_info(l_node, &l_info);

	if ( l_info.type == CFG$K_TYPE_STRING )
		{
		cfg$get_string(l_node, "", &l_str, &l_len);

		if ( 1 & (l_sts = s_write(STDOUT_FILENO, l_str, l_len, "SYS$OUTPUT")) )
			l_sts = s_write(STDOUT_FILENO, "\n", 1, "SYS$OUTPUT");

		goto	l_done;
		}

	if ( !(1 & (l_sts = cfg$node_text(l_node, s_present(a_ctx, &s_get_q[1]) ? CFG$M_TEXT_FLAT : 0, &l_text, &l_len))) )
		{
		$PUTMSG_FAO(l_sts);
		goto	l_done;
		}

	if ( 1 & (l_sts = s_write(STDOUT_FILENO, l_text, l_len, "SYS$OUTPUT")) )
		l_sts = s_write(STDOUT_FILENO, "\n", 1, "SYS$OUTPUT");

	free(l_text);

l_done:
	cfg$free(l_tree);

	return	(int) l_sts;

	(void) a_arg;
}



/*
**  DESCRIPTION: Write a changed tree back: in place, or the main file into /OUTPUT.
*/
static unsigned	s_save	(
		CLI_CTX		*a_ctx,
		CLI_PQDESC	*a_qout,
		CFG$TREE	*a_tree
			)
{
CFGTOOL$ARG	l_out;
unsigned	l_sts;

	if ( s_present(a_ctx, a_qout) && (1 & s_arg(a_ctx, a_qout, &l_out)) )
		l_sts = cfg$write_file(a_tree, l_out.str);
	else	l_sts = cfg$write(a_tree);

	if ( !(1 & l_sts) )
		cfg$puterrs(a_tree);

	return	l_sts;
}



/*
**  DESCRIPTION: SET file path value - the value from its text (/TYPE=AUTO, the default), or as a
**		string (/TYPE=STRING); missing groups on the path are made; the file is written
**		back with its comments.
*/
static int	s_do_set	(
		CLI_CTX		*a_ctx,
		void		*a_arg
			)
{
CFGTOOL$ARG	l_file, l_path, l_value;
CFG$TREE	*l_tree;
CFG$NODE	*l_root;
CLI_KEYWORD	*l_kwd = NULL;
unsigned	l_sts;

	s_arg(a_ctx, &s_set_p[0], &l_file);
	s_arg(a_ctx, &s_set_p[1], &l_path);
	s_arg(a_ctx, &s_set_p[2], &l_value);

	if ( !(1 & (l_sts = s_load(l_file.str, a_ctx, &s_set_q[0], NULL, &l_tree))) )
		return	(int) l_sts;

	cfg$root(l_tree, &l_root);
	__cli$get_keyword(a_ctx, &s_set_q[1], NULL, &l_kwd);

	l_sts = (l_kwd && (l_kwd->val == 1)) ? cfg$set_string(l_root, l_path.str, l_value.str, l_value.len)
		: cfg$set_value(l_root, l_path.str, l_value.str, l_value.len);

	if ( !(1 & l_sts) )
		{
		$PUTMSG_FAO(CFG$_CANTSET, l_path.str, l_file.str);
		$PUTMSG_FAO(l_sts);
		goto	l_done;
		}

	if ( (1 & (l_sts = s_save(a_ctx, &s_set_q[2], l_tree))) && s_present(a_ctx, &s_set_q[3]) )
		$PUTMSG_FAO(CFG$_MODIFIED, l_path.str, l_file.str);

l_done:
	cfg$free(l_tree);

	return	(int) l_sts;

	(void) a_arg;
}



/*
**  DESCRIPTION: DELETE file path - the setting goes, with its comments; the file is written back.
*/
static int	s_do_delete	(
		CLI_CTX		*a_ctx,
		void		*a_arg
			)
{
CFGTOOL$ARG	l_file, l_path;
CFG$TREE	*l_tree;
CFG$NODE	*l_root, *l_node;
unsigned	l_sts;

	s_arg(a_ctx, &s_delete_p[0], &l_file);
	s_arg(a_ctx, &s_delete_p[1], &l_path);

	if ( !(1 & (l_sts = s_load(l_file.str, a_ctx, &s_delete_q[0], NULL, &l_tree))) )
		return	(int) l_sts;

	cfg$root(l_tree, &l_root);

	if ( !(1 & cfg$lookup(l_root, l_path.str, &l_node)) )
		{
		l_sts = $PUTMSG_FAO(CFG$_NOSETTING, l_path.str, l_file.str);
		goto	l_done;
		}

	if ( !(1 & (l_sts = cfg$delete(l_node))) )
		{
		$PUTMSG_FAO(CFG$_CANTSET, l_path.str, l_file.str);
		goto	l_done;
		}

	if ( (1 & (l_sts = s_save(a_ctx, &s_delete_q[1], l_tree))) && s_present(a_ctx, &s_delete_q[2]) )
		$PUTMSG_FAO(CFG$_DELETED, l_path.str, l_file.str);

l_done:
	cfg$free(l_tree);

	return	(int) l_sts;

	(void) a_arg;
}



/*
**  DESCRIPTION: HELP - the summary of the commands.  The full description is the help library:
**		HELP CFGTOOL (the HELP package), and the manual of yaLIBCFG.
*/
static int	s_do_help	(
		CLI_CTX		*a_ctx,
		void		*a_arg
			)
{
static	const char	s_text[] =
	"CFGTOOL - check, format, compare, read and change configuration files (yaLIBCFG " __IDENT__ ")\n"
	"\n"
	"  CFGTOOL CHECK   file                [/FINGERPRINT /NOCASE /NOINCLUDE]\n"
	"  CFGTOOL FORMAT  file                [/OUTPUT=file /NOCASE /NOINCLUDE]\n"
	"  CFGTOOL DIFF    old-file new-file   [/OUTPUT=file /POSITIONAL /NOTIME /NOCASE]\n"
	"  CFGTOOL GET     file path           [/FLAT /NOCASE]\n"
	"  CFGTOOL SET     file path value     [/TYPE=AUTO|STRING /OUTPUT=file /LOG /NOCASE]\n"
	"  CFGTOOL DELETE  file path           [/OUTPUT=file /LOG /NOCASE]\n"
	"\n"
	"A path: system.hostname, vlans.[1], ports.[id=9].mtu.  A qualifier may be written --name too.\n"
	"Exit status: 0 - success (DIFF: no differences), 1 - DIFF found differences, 2 - an error.\n"
	"The parameters and qualifiers of a command: CFGTOOL <command> /HELP.  All of it: HELP CFGTOOL.\n";

	return	(int) s_write(STDOUT_FILENO, s_text, sizeof(s_text) - 1, "SYS$OUTPUT");

	(void) a_ctx;
	(void) a_arg;
}



int	main	(
		int		argc,
		char		**argv
			)
{
void		*l_ctx = NULL;
unsigned	l_sts;

	__util$setlogfd(STDERR_FILENO);
	cfg$inimsg();

	l_sts = (unsigned) __cli$parse(s_verbs, CLI$M_OPSIGNAL, argc - 1, argv + 1, &l_ctx);

	if ( l_sts == STS$K_WARN )				/* help has been printed		*/
		return	CFGTOOL$K_EXIT_OK;

	if ( !(1 & l_sts) )
		return	CFGTOOL$K_EXIT_ERROR;

	l_sts = (unsigned) __cli$dispatch(l_ctx);
	__cli$cleanup(l_ctx);

	if ( l_sts == CFG$_DIFFERENT )
		return	CFGTOOL$K_EXIT_DIFF;

	return	(1 & l_sts) ? CFGTOOL$K_EXIT_OK : CFGTOOL$K_EXIT_ERROR;
}
