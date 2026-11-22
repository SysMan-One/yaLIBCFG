#ifndef	__CFG_INT$H__
#define	__CFG_INT$H__	1

/*
**++
**
**  FACILITY:	yaLIBCFG - configuration trees in the libconfig syntax
**
**  MODULE:	cfg_int.h
**
**  ABSTRACT:	Internal definitions shared by the modules of the library: the tree, the
**		node, the source file, the arena, the lexer.  Not installed.
**
**  DESCRIPTION: Offsets of a node into its source buffer (all are byte offsets):
**
**		    lead     beg      vbeg          vend   sep     end
**		    |        |        |             |      |       |
**		    # note\n  mtu   =  9216          ;      # c \n
**		    \________/ \_____/ \_____________/ \___________/
**		     leading    head      value         tail
**
**		LEAD..BEG	- the comment lines just above the node (no blank line between)
**		BEG..VBEG	- the name and '=' of a setting; empty for an element
**		VBEG..VEND	- the value; for a container - from its bracket to its bracket
**		VEND..END	- the terminator, a comment on the same line, the newline
**		SEP		- offset of the ',' after an element of a list or array, or NOSEP
**
**		A node removed by the program stays in the list of its parent as a tombstone
**		(CFG$M_DELETED) - the writer must know what text to skip.
**
**  AUTHOR:	StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
**
**  CREATION DATE:  22-NOV-2026
**
**  MODIFICATION HISTORY:
**
**	X.01-01		22-NOV-2026	RRL
**		First edition.
**
**	X.01-02		22-NOV-2026	RRL
**		JUNK, CFG$K_TOK_ERROR, CFG$M_OPENJUNK, the skip counters.
**
**	X.01-03		22-NOV-2026	RRL
**		CFG$K_FMT_OCTZ, CFG$K_FMT_PHEX/PDEC/PBIN/POCT.
**
**--
*/

#include	<stdint.h>
#include	<stddef.h>
#include	"yalibcfg.h"

#define	CFG$K_NOSEP		0xFFFFFFFFU		/* no separator after the element		*/

#define	CFG$K_ARENA		(64U * 1024U)		/* size of an arena chunk			*/

/*
**  Node flags
*/
#define	CFG$M_MODIFIED		0x01			/* the value has been set by the program	*/
#define	CFG$M_DELETED		0x02			/* tombstone					*/
#define	CFG$M_NEW		0x04			/* made by the program, no source text		*/
#define	CFG$M_DIRTY		0x08			/* something below has changed			*/
#define	CFG$M_SUFFIXL		0x10			/* an integer written with the L suffix		*/
#define	CFG$M_OPENJUNK		0x40			/* a JUNK that runs to the end of its file with	*/
							/* a bracket, a string or a comment open: new	*/
							/* settings are written before it, not after	*/

/*
**  How an integer was written - kept to write a changed value the same way
*/
enum	{
	CFG$K_FMT_DEC		= 0,
	CFG$K_FMT_HEX,					/* 0x1F						*/
	CFG$K_FMT_BIN,					/* 0b101					*/
	CFG$K_FMT_OCT,					/* 0o27						*/
	CFG$K_FMT_OCTZ,					/* 027 - octal, as libconfig			*/
	CFG$K_FMT_PHEX,					/* %X1F - the radix of DCL			*/
	CFG$K_FMT_PDEC,					/* %D27						*/
	CFG$K_FMT_PBIN,					/* %B101					*/
	CFG$K_FMT_POCT					/* %O27						*/
	};

typedef	struct cfg_arena_t {
	struct cfg_arena_t	*next;
	size_t			used,
				size;
	_Alignas(max_align_t) char data[];		/* aligned for any type: on a 32-bit	*/
} CFG$ARENA;						/* MIPS a double at 4 is a bus error	*/

typedef	struct cfg_src_t {
	char		*path;				/* as given or resolved, zero-terminated	*/
	char		*buf;				/* the whole file				*/
	uint32_t	len;
	uint16_t	no,				/* index in the tree				*/
			depth;				/* of @include					*/
	char		*real;				/* realpath() - for the loop check		*/
	int64_t		mtime;				/* time of the last change, 0 - not a file	*/
	uint32_t	ncomm;				/* comment spans, sorted			*/
	uint32_t	*comm;				/* pairs: begin, end				*/
	uint32_t	commmax;
	int		orphan;				/* its @include is in a skipped setting		*/
} CFG$SRC;

struct cfg_node_t {
	struct cfg_node_t	*parent,
				*next,
				*prev,
				*child,
				*last;
	struct cfg_tree_t	*tree;
	const char		*name;			/* in the arena or in the source buffer		*/
	uint8_t			namelen,
				type,
				flags,
				fmt;
	uint16_t		srcno,			/* source of the text of this node		*/
				incsrc;			/* CFG$K_TYPE_INCLUDE: the source it brings	*/
	uint32_t		line,
				col;
	uint32_t		lead,
				beg,
				vbeg,
				vend,
				sep,
				end;
	uint32_t		count;			/* live children				*/
	union	{
		int64_t		i;
		double		f;
		int		b;
		struct	{
			const char	*ptr;		/* decoded: escapes resolved, pieces joined	*/
			uint32_t	len;
			} s;
		} v;
};

typedef	struct cfg_err_t {
	unsigned	sts;
	uint16_t	srcno,
			haspos;
	uint32_t	line,
			col;
	int		err;				/* errno, for OPENIN, READERR, WRITERR		*/
	uint32_t	len;
	char		text[160];			/* the detail					*/
} CFG$ERR;

struct cfg_tree_t {
	uint32_t	opts;
	int		broken;				/* the parse failed: the tree is for CFG$PUTERRS only */
	int		mute;				/* CFG$$ERROR records nothing (a skip is going on) */
	uint32_t	nskipped;			/* bad settings skipped (CFG$M_SKIPERRS)	*/
	CFG$ARENA	*arena;
	CFG$NODE	*root;
	uint32_t	nsrc;
	CFG$SRC		*src[CFG$K_MAXSRC];
	uint32_t	nerr,
			ndropped;			/* diagnostics past CFG$K_MAXERRS, not kept	*/
	CFG$ERR		err[CFG$K_MAXERRS];
};

/*
**  CFG_TREE.C
*/
void *		cfg$$alloc	(CFG$TREE *a_tree, size_t a_size);
char *		cfg$$strdup	(CFG$TREE *a_tree, const char *a_str, size_t a_len);
CFG$NODE *	cfg$$node	(CFG$TREE *a_tree, CFG$NODE *a_parent, uint32_t a_type);
void		cfg$$dirty	(CFG$NODE *a_node);
int		cfg$$namecmp	(CFG$TREE *a_tree, const char *a_a, uint32_t a_alen, const char *a_b, uint32_t a_blen);
CFG$NODE *	cfg$$find	(CFG$NODE *a_group, const char *a_name, uint32_t a_len);
int		cfg$$isname	(const char *a_name, uint32_t a_len);

/*
**  CFG_MSG.C
*/
unsigned	cfg$$error	(CFG$TREE *a_tree, unsigned a_sts, uint32_t a_srcno, uint32_t a_line, uint32_t a_col, const char *a_text, size_t a_len);
unsigned	cfg$$syserr	(CFG$TREE *a_tree, unsigned a_sts, const char *a_what, int a_err);

/*
**  CFG_LEX.C
*/
enum	{
	CFG$K_TOK_EOF		= 0,
	CFG$K_TOK_NAME,
	CFG$K_TOK_INT,
	CFG$K_TOK_INT64,
	CFG$K_TOK_FLOAT,
	CFG$K_TOK_BOOL,
	CFG$K_TOK_STRING,
	CFG$K_TOK_EQ,				/* '=' or ':'					*/
	CFG$K_TOK_SEMI,
	CFG$K_TOK_COMMA,
	CFG$K_TOK_LBRACE,
	CFG$K_TOK_RBRACE,
	CFG$K_TOK_LBRACK,
	CFG$K_TOK_RBRACK,
	CFG$K_TOK_LPAREN,
	CFG$K_TOK_RPAREN,
	CFG$K_TOK_INCLUDE,			/* @include					*/
	CFG$K_TOK_ERROR				/* the lexer met an error: no token		*/
	};

typedef	struct cfg_lex_t {
	CFG$TREE	*tree;
	CFG$SRC		*src;
	uint32_t	pos,				/* next octet to look at			*/
			line,
			lpos;				/* offset of the beginning of the line		*/
	/* The current token */
	uint32_t	tok,
			tbeg,
			tend,
			tline,
			tcol;
	uint8_t		fmt,
			suffixl;
	int64_t		ival;
	double		fval;
} CFG$LEX;

unsigned	cfg$$lex_init	(CFG$LEX *a_lex, CFG$TREE *a_tree, CFG$SRC *a_src);
unsigned	cfg$$lex_next	(CFG$LEX *a_lex);
unsigned	cfg$$lex_string	(CFG$LEX *a_lex, uint32_t a_beg, uint32_t a_end, char *a_out, uint32_t *a_outlen);
int		cfg$$lex_comment(CFG$SRC *a_src, uint32_t a_beg, uint32_t a_end);
const char *	cfg$$tokname	(uint32_t a_tok);
void *		cfg$$cloc	(void);
int		cfg$$span_at	(CFG$SRC *a_src, uint32_t a_pos, uint32_t *a_beg, uint32_t *a_end);

/*
**  CFG_WRITE.C helpers used by the other modules
*/
unsigned	cfg$$format_string(const char *a_str, uint32_t a_len, char **a_out, uint32_t *a_outlen);
unsigned	cfg$$text_flat	(CFG$NODE *a_node, int a_name, char **a_buf, uint32_t *a_len);
unsigned	cfg$$text_double(double a_val, char *a_buf, uint32_t a_bufsz, uint32_t *a_len);

/*
**  CFG_SCHEMA.C helpers used by CFG_DIFF.C
*/
const	CFG$SCHEMA *	cfg$$schema_entry	(const CFG$SCHEMA *a_table, CFG$TREE *a_tree, const char *a_name, uint32_t a_len);
const	char *		cfg$$schema_key		(const CFG$SCHEMA *a_elem);

/*
**  CFG_SHA1.C
*/
typedef	struct cfg_sha1_t {
	uint32_t	h[5];
	uint8_t		blk[64];
	uint32_t	used;
	uint64_t	total;
} CFG$SHA1;

void		cfg$$sha1_init	(CFG$SHA1 *a_ctx);
void		cfg$$sha1_update(CFG$SHA1 *a_ctx, const void *a_data, size_t a_len);
void		cfg$$sha1_final	(CFG$SHA1 *a_ctx, uint8_t *a_out);

#endif	/* __CFG_INT$H__ */
