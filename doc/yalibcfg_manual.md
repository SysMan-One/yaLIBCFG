# yaLIBCFG Programmer's Manual

**Configuration Trees in the libconfig Syntax**

Software Version: yaLIBCFG X.01-01

StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)

November 2026

---

## Preface

### Intended Audience

This manual is for programmers who write C programs that keep their settings in a file. You
should know C. You do not need to know libconfig, and you do not need to know OpenVMS - but if
you do, a lot here will look familiar.

### Document Structure

This manual has three parts and four appendixes.

* **Part I, Concepts and Tutorial**, tells what the library is for, how a configuration file is
  written, and how a program reads and changes one. Read it from the start.
* **Part II, Reference**, describes every routine in the same order of headings: Format,
  Returns, Arguments, Description, Condition Values Returned, Example. Use it when you write
  code.
* **Part III, The CFGTOOL Utility**, describes the command-line utility of the library.
* **Appendix A** lists the messages, with what each one means and what to do.
* **Appendix B** lists the limits of the library.
* **Appendix C** lists the differences from libconfig.
* **Appendix D** defines the fingerprint exactly.

### Conventions

| Convention | Meaning |
|---|---|
| `cfg$parse_file` | A name in the C source: a routine, a type, a constant. |
| CFG$PARSE_FILE | The same routine named in the text, in the OpenVMS way. |
| `$ command` | A command you type at the shell. |
| CFG$_NOTFOUND | A condition value: facility CFG, message NOTFOUND. |
| `...` | Something left out of an example. |

All the examples of this manual are in the `examples` directory of the kit. They are built and
run with the tests, so the code you read here is code that works.

---

# Part I. Concepts and Tutorial

## 1 Introduction

### 1.1 What yaLIBCFG Is

yaLIBCFG is a C library that reads configuration files into a **tree**, lets a program look
up and change the settings in the tree, and writes the tree back into the file.

The files are written in the **libconfig syntax**: any file libconfig reads, yaLIBCFG reads
into the same tree, and what yaLIBCFG writes, libconfig reads.

### 1.2 Why Not libconfig Itself

libconfig is a good library, but a program that both reads **and writes** its configuration
runs into two problems with it:

1. **libconfig loses everything that is not a value.** It forgets the comments, the blank lines,
   the order of your thoughts. When the program saves the file, the notes a person left there
   are gone.
2. **libconfig cannot tell what changed.** A program that applies its configuration to live
   equipment wants to know "what is different between the old file and the new one".

yaLIBCFG keeps the text. Every node of the tree remembers where in the file it came from. When
the program writes the tree back:

* a setting nothing touched is copied as it stood, comments and all;
* a changed value gets new text, and its name, its `=`, its `;` and its comment stay;
* a removed setting leaves with the comment lines that described it;
* a new setting is put after its siblings, with their indentation and the line end the file
  uses (LF or CR LF).

A file read and written without changes is the **same file byte for byte**. The tests of the
kit check this on every build.

yaLIBCFG also **compares two configurations** - the settings, not the text - and prints the
differences as a listing in the manner of the OpenVMS DIFFERENCES utility. Every configuration
gets a **fingerprint**, a UUID made of its settings, so that a listing tells whose
configurations it compares.

And it **validates** a file by a **schema** - a table in the program that says which settings
there are, of what types, in what ranges - and tells the user everything that is wrong with
the file in one go, each problem with its line and column.

### 1.3 A First Look

Here is a file:

```
# router.cfg - the sample file of the yaLIBCFG manual

# Who we are
system = {
	hostname = "edge-1";		# shown in the prompt
	location = "Lab, rack 3";
};

# The ports of the switch
ports = (
	# management
	{ id = 1;  speed = "1G";  enabled = true; },

	# uplinks
	{ id = 9;  speed = "10G"; enabled = true;  mtu = 9216; },
	{ id = 10; speed = "10G"; enabled = false; mtu = 9216; }
);

vlans = [ 1, 100, 200 ];
```

And here is a program that reads two values from it (`examples/ex1_read.c`):

```c
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
```

```
$ ex1_read router.cfg
hostname: edge-1
port 9 MTU: 9216
```

Note the path `ports.[id=9].mtu`: "in the list PORTS, the element whose ID is 9, its setting
MTU". Section 3.3 tells all about paths.

### 1.4 Building and Linking

yaLIBCFG needs the StarLet library (`libstarlet.a` and its headers) - the messages and the
FAO engine come from there.

```
$ cmake -S . -B build -DCMAKE_PREFIX_PATH=/opt/starlet
$ cmake --build build
$ (cd build && ctest)
$ cmake --install build --prefix /opt/yalibcfg
```

The kit installs `lib/libyalibcfg.a`, `include/yalibcfg.h` and `include/yalibcfgdef.h`.
A program includes `yalibcfg.h` and links with `-lyalibcfg -lstarlet -lpthread -lm`.

---

## 2 The Configuration File

### 2.1 Settings

A file is a sequence of **settings**. A setting is a name, an `=` (or a `:`), a value and an
optional `;` (or `,`):

```
hostname = "edge-1";
mtu      : 9216
```

A **name** starts with a letter or `*` and goes on with letters, digits, `-`, `_` and `*`.
Names are case sensitive, unless the program asks otherwise (CFG$M_NOCASE, see CFG$PARSE_FILE).
Within one group a name may appear only once.

### 2.2 Values

| Kind | Examples | Notes |
|---|---|---|
| Integer | `42`, `-17`, `0x1F`, `0b1011`, `0o755` | 32 bits. A leading `0` alone is NOT octal: `0027` is 27. |
| 64-bit integer | `9223372036854775807L`, `0xFFFFFFFFFFL`, `5000000000` | The `L` (or `LL`) suffix, or a decimal number that does not fit 32 bits. |
| Float | `0.75`, `1e-3`, `-2.5E+10`, `.5` | A double. There is no text for infinity or NaN. |
| Boolean | `true`, `false`, `TRUE`, `False` | Any case. |
| String | `"text"`, `"a\tb"`, `"one " "two"` | Escapes `\\`, `\"`, `\n`, `\r`, `\t`, `\f`, `\xHH`. Strings side by side are joined. |
| Group | `{ w = 640; h = 480; }` | Named settings. |
| Array | `[ 1, 100, 200 ]` | Scalars of one type (32- and 64-bit integers count as one). |
| List | `( "abc", 123, { x = 1; } )` | Values of any kind, groups and lists too. |

A hexadecimal, binary or octal number without the `L` suffix that fits 32 bits as an
unsigned number is a 32-bit integer with its sign bit: `0xFFFFFFFF` is -1. This is what
libconfig does.

### 2.3 Comments

```
# to the end of the line
// to the end of the line
/* anywhere,
   over lines */
```

**Comments belong to settings.** The comment lines right above a setting (with no blank line
between) and a comment on the same line after it are that setting's. This matters when a
program removes the setting: its comments go with it.

```
# this comment belongs to BETA - it goes when BETA goes
beta = 2;	# and so does this one

# this one is ALONE - the blank line below cuts it off

gamma = 3;
```

### 2.4 Include Files

```
@include "ports.cfg"
```

The directive stands where a setting may stand - at the top of a file or inside a group. The
settings of the included file become settings of that group. A relative name is taken from the
directory of the file the directive is in.

The included file must hold **whole settings**. (libconfig pastes the included file in as
text, so half a group there works; here it does not.) A file may be included **once** in a
tree: if it stood in two places, a change made in one of them could not be written back
without losing the other (CFG$_INCTWICE).

When a program changes a setting that came from an included file, CFG$WRITE writes the change
into **that** file; the `@include` line of the main file stays as it was.

---

## 3 Programming Concepts

### 3.1 Condition Values

Every routine returns a **condition value**, an `unsigned` in the OpenVMS layout:

```
 31            16 15                 3 2   0
+----------------+--------------------+-----+
|   facility     |   message number   | sev |
+----------------+--------------------+-----+
```

The facility of yaLIBCFG is CFG, number 0x0114. The **low bit** tells success from failure:
set - success (SUCCESS, INFO), clear - failure (WARNING, ERROR, FATAL). So the test is always:

```c
if ( !(sts & 1) )
	... failure ...
```

Never compare with `== CFG$_NORMAL`: a routine may return `STS$K_SUCCESS` as well. Compare
with a particular code only when you look for that one, as in:

```c
sts = cfg$get_int(root, "system.snmp.port", &port);

if ( sts == CFG$_NOTFOUND )
	port = 161;			/* not in the file - the default		*/
else if ( !(sts & 1) )
	return sts;			/* there, but not an integer: an error		*/
```

The results of a routine come back through its **output arguments**, never through the
function value.

### 3.2 Trees and Nodes

CFG$PARSE_FILE makes a **tree**: a `CFG$TREE`. The tree owns all its nodes and strings; one
call of CFG$FREE releases all of them.

A **node** (`CFG$NODE`) is a setting or an element. The **root** node is an unnamed group: the
file itself. Both types are opaque: you never look inside, you call routines.

```
CFG$TREE ── root (group) ──┬── system (group) ──┬── hostname "edge-1"
                           │                    └── location "Lab, rack 3"
                           ├── ports (list) ────┬── [0] (group) ── id, speed, enabled
                           │                    ├── [1] (group) ── id, speed, enabled, mtu
                           │                    └── [2] (group) ── id, speed, enabled, mtu
                           └── vlans (array) ── [0] 1, [1] 100, [2] 200
```

A pointer to a node stays good until CFG$FREE of its tree. A removed node is not freed - but
do not use it any more.

A tree belongs to **one thread at a time**. Different trees are independent and may be used by
different threads at once.

### 3.3 Paths

Most routines take a **base node** and a **path** from it:

| Path | Leads to |
|---|---|
| `system.hostname` | the setting HOSTNAME of the group SYSTEM |
| `system/hostname` | the same: `/` is taken as `.` |
| `vlans.[1]` | element 1 (from 0) of the array VLANS: 100 |
| `ports.[id=9]` | the element of the list PORTS whose setting ID is 9 |
| `ports.[id=9].mtu` | ... and its setting MTU |
| `ports.[speed="10G"]` | the first element whose SPEED is the string "10G" |
| `""` or NULL | the base node itself |

An element selected by `[n]` or `[key=value]` is looked for among the **live** elements:
those removed by the program do not count. The value in `[key=value]` is written as in a
file - `[id=0x1F]`, `[ratio=1.5]`, `[name="a\"b"]` - and a string may also go without
quotes: `[name=mgmt]`. Every path CFG$DIFF reports can be given to CFG$LOOKUP.

The path `""` is handy when you already hold the node:

```c
CFG$NODE	*port;
int32_t		mtu;

cfg$lookup(root, "ports.[id=9]", &port);
cfg$get_int(port, "mtu", &mtu);		/* from the port			*/
cfg$set_int(port, "", 1);		/* no: PORT is a group - CFG$_TYPE	*/
```

### 3.4 Diagnostics

The library **puts nothing out by itself**. When a parse fails, the routine returns the
condition value of the first error, and the tree keeps a record of it with the file, the line
and the column. Your program decides whether to show it: CFG$PUTERRS puts the record out
through `$PUTMSG_FAO` of StarLet:

```
%CFG-E-SYNTAX, syntax error: expected ';' or a setting name, found integer, router.cfg line 12 column 18
```

Call CFG$INIMSG once, at the start of the program, so that the message texts are there.

### 3.5 Writing Back

There are three ways to get the text of a tree:

| Routine | What it does |
|---|---|
| CFG$WRITE_BUF | gives you the text of one source file in a buffer |
| CFG$WRITE | writes every source file whose text changed back into its place |
| CFG$WRITE_FILE | writes the main file into another file (a copy, "Save As") |

CFG$WRITE and CFG$WRITE_FILE never leave a half-written file: they write a temporary file in
the same directory, sync it to the disk, and rename it over the old one. The access mode of the
old file is kept.

### 3.6 Comparing and Fingerprints

CFG$DIFF compares two trees **by their settings**. Comments, layout and the order of the
settings in a group do not count. The rules:

* In a group, settings are matched by name.
* In a list, elements are matched by a **key** when there is one: the setting a schema flags
  CFG$M_KEY (section 3.7), when the program gives the schema; else `id` (or `name`, or
  `key`) when every element of both lists is a group holding it as a scalar, unique in its
  list. Such an element is named by its key: `ports.[id=9]`. Otherwise - or always, with
  CFG$M_POSITIONAL - by position: `list.[2]`.
* An array is one value: `[1, 2, 3]` and `[1, 3]` differ as a whole.
* A node whose type changed is a change of the whole node. A 32-bit and a 64-bit integer of the
  same value are equal.

Every difference is one of three kinds: CFG$K_DIFF_MOD (the value differs), CFG$K_DIFF_ADD (only
in the new tree), CFG$K_DIFF_DEL (only in the old one).

The **fingerprint** (CFG$FINGERPRINT) of a tree is a UUID of version 5 (RFC 9562), made by
SHA-1 from the settings - in a canonical form that, like the comparison (without
CFG$M_POSITIONAL), does not see comments, layout, the order of the settings in a group or the
order of the elements of a list with a key. So:

* two files with the same settings have the same fingerprint, whatever their comments;
* any change of a setting changes the fingerprint;
* the fingerprint is the same on every machine and in every version of yaLIBCFG (Appendix D).

A listing names the fingerprints of both trees. Keep listings, and you can always tell which
configuration was compared with which.

### 3.7 Schemas

A **schema** tells what a file may hold. It is a table in your program, one entry per setting,
in the spirit of the verb tables of `cli_routines`:

```c
typedef struct cfg_schema_t {
	const char	*name;		/* the setting; "*" - any other; NULL - end of the table	*/
	uint32_t	type;		/* CFG$K_TYPE_*						*/
	uint32_t	flags;		/* CFG$M_REQUIRED, CFG$M_KEY, CFG$M_RANGE, ...		*/
	int64_t		min, max;	/* integer range, string length, number of elements	*/
	double		fmin, fmax;	/* float range						*/
	const CFG$KEYWORD *kwd;		/* the words of a CFG$K_TYPE_KWD			*/
	const struct cfg_schema_t *sub;	/* a group: its table; a list, an array: one entry for	*/
					/* the elements						*/
	cfg_check_t	check;		/* a routine of the program, or NULL			*/
	const char	*help;		/* what the setting is for				*/
} CFG$SCHEMA;
```

Besides the types of the file, a schema has types that are strings with a meaning:

| Type | The string must be |
|---|---|
| CFG$K_TYPE_KWD | one of the words of KWD; case does not matter unless CFG$M_CASEKWD |
| CFG$K_TYPE_IPV4 | an IPv4 address: `"192.168.1.1"` |
| CFG$K_TYPE_IPV6 | an IPv6 address: `"2001:db8::1"` |
| CFG$K_TYPE_IPADDR | either of them |
| CFG$K_TYPE_MAC | a MAC address: `"00:1b:44:11:3a:b7"` or with `-` |

A CFG$K_TYPE_FLOAT entry takes an integer too (`load = 1;`); a CFG$K_TYPE_INT entry takes a
value that fits 32 bits, a CFG$K_TYPE_INT64 one any integer.

The flags:

| Flag | Meaning |
|---|---|
| CFG$M_REQUIRED | The setting must be there. |
| CFG$M_KEY | In a group that is an element of a list: the key of the elements - required, unique in the list. CFG$DIFF given the schema matches the elements by it, and the diagnostics name an element by it: `ports.[id=9]`. |
| CFG$M_RANGE | MIN..MAX apply to an integer, FMIN..FMAX to a float. |
| CFG$M_LENGTH | MIN..MAX apply to the length of a string. |
| CFG$M_COUNT | MIN..MAX apply to the number of elements of a list or an array. |
| CFG$M_DEPRECATED | Still taken, with a warning. |
| CFG$M_OPEN | A group: settings its table does not name are taken without a word. |
| CFG$M_CASEKWD | Keywords are compared with regard to case. |

A setting the table of its group does not name is an error (CFG$_UNKNOWN) - so a typo in a
file never passes in silence. The name `"*"` in a table stands for any setting the table does
not name otherwise, for groups whose setting names are data: `users = { amy = {...}; bob = {...}; }`.

The entry of a list or an array is a single entry in SUB, for all its elements; its name is
not used.

When the rules of the table are not enough - "the MTU of an uplink must be bigger than of an
access port" - the entry gets a **check routine**. It sees the node only when all the other
checks of the node and of what is inside it passed, and says what is wrong in its own words:

```c
typedef unsigned (*cfg_check_t) (CFG$NODE *a_node, void *a_arg, char *a_msg, uint32_t a_msgsz, uint32_t *a_msglen);
```

---

## 4 Tutorial

### 4.1 Changing a File

`examples/ex2_edit.c` changes one value, adds a new setting, removes port 10 and saves:

```c
	cfg$root(tree, &root);

	/* A new value for an old setting */
	sts = cfg$set_string(root, "system.location", "Lab, rack 4", 11);

	/* A setting that was not there: made, with the group on its way */
	if ( sts & 1 )
		sts = cfg$set_int(root, "system.snmp.port", 161);

	/* Port 10 goes away, with the comment above it if it had one */
	if ( (sts & 1) && (cfg$lookup(root, "ports.[id=10]", &port) & 1) )
		sts = cfg$delete(port);

	/* Into the file, in its place (a temporary file, then a rename) */
	if ( sts & 1 )
		sts = cfg$write(tree);
```

The file afterwards - compare it with section 1.3: every comment is in place, the new group
SNMP has the indentation of its neighbours, and the `,` after port 9 is gone because port 9 is
now the last element:

```
# Who we are
system = {
	hostname = "edge-1";		# shown in the prompt
	location = "Lab, rack 4";
	snmp = {
		port = 161;
	};
};

# The ports of the switch
ports = (
	# management
	{ id = 1;  speed = "1G";  enabled = true; },

	# uplinks
	{ id = 9;  speed = "10G"; enabled = true;  mtu = 9216; }
);
```

### 4.2 Walking a Tree

CFG$FIRST and CFG$NEXT go over the children of a node; CFG$NODE_INFO tells what a node is
(`examples/ex3_walk.c`, shortened):

```c
static void	walk (CFG$NODE *node, int depth)
{
	CFG$NODE	*child;
	CFG$INFO	info;
	unsigned	sts;

	for ( sts = cfg$first(node, &child); sts & 1; sts = cfg$next(child, &child) )
		{
		cfg$node_info(child, &info);

		printf("%*s%-10.*s line %3u  ", depth * 2, "",
			(int) (info.name ? info.namelen : 3), info.name ? info.name : "[ ]", info.line);

		switch ( info.type )
			{
			case CFG$K_TYPE_STRING:
				{ const char *s; uint32_t l; cfg$get_string(child, "", &s, &l); printf("\"%.*s\"\n", (int) l, s); break; }
			...
			default:			/* a group, a list or an array: go down */
				printf("... of %u\n", info.count);
				walk(child, depth + 1);
			}
		}
}
```

```
$ ex3_walk router.cfg
system     line   4  group of 2
  hostname   line   5  "edge-1"
  location   line   6  "Lab, rack 3"
ports      line  10  list of 3
  [ ]        line  12  group of 3
    id         line  12  1
...
```

### 4.3 Lists and Arrays

CFG$ADD puts a new node at the end of a group, a list or an array
(`examples/ex4_list.c`):

```c
	/* Port 11: a new group at the end of the list */
	sts = cfg$lookup(root, "ports", &ports);

	if ( sts & 1 )
		sts = cfg$add(ports, NULL, CFG$K_TYPE_GROUP, &port);
	if ( sts & 1 )
		sts = cfg$set_int(port, "id", 11);
	if ( sts & 1 )
		sts = cfg$set_string(port, "speed", "10G", 3);

	/* VLAN 300 at the end of the array */
	if ( sts & 1 )
		sts = cfg$lookup(root, "vlans", &vlans);
	if ( sts & 1 )
		sts = cfg$add(vlans, NULL, CFG$K_TYPE_INT, &vlan);
	if ( sts & 1 )
		sts = cfg$set_int(vlan, "", 300);
```

```
	{ id = 10; speed = "10G"; enabled = false; mtu = 9216; },
	{
		id = 11;
		speed = "10G";
	}
);

vlans = [ 1, 100, 200, 300 ];
```

Elements of a list or an array have no names: give NULL. An array takes only scalars of the
type of its elements - CFG$ADD of a string into an array of integers returns CFG$_TYPE.

### 4.4 Handling Errors

A good program tells the user what is wrong **and where**:

```c
sts = cfg$parse_file(path, 0, &tree);

if ( !(sts & 1) )
	{
	cfg$puterrs(tree);		/* every error the parse recorded, with its position */
	cfg$free(tree);			/* the tree is there even after a failure		*/
	return sts;
	}
```

```
%CFG-E-DUPNAME, duplicate setting mtu, router.cfg line 15 column 50
```

A failed CFG$SET_*, CFG$ADD or CFG$DELETE leaves the tree as it was and records nothing in it -
only the condition value says what went wrong; put it out with `$PUTMSG_FAO(sts)` if you like.

A tree whose parse failed is good only for CFG$PUTERRS and CFG$FREE: CFG$WRITE_BUF, CFG$WRITE,
CFG$WRITE_FILE, CFG$DIFF and CFG$FINGERPRINT refuse it with CFG$_BROKEN.

### 4.5 Comparing Two Configurations

`examples/ex5_diff.c` compares two files and prints the listing:

```c
	sts = cfg$parse_file(argv[1], 0, &old);

	if ( sts & 1 )
		sts = cfg$parse_file(argv[2], 0, &new);

	if ( sts & 1 )
		sts = cfg$diff(old, new, NULL, 0, &diff);
	...
	if ( cfg$diff_listing(diff, (argc > 3) ? CFG$M_NOTIME : 0, &text, &len) & 1 )
		{
		fwrite(text, 1, len, stdout);
		free(text);
		}

	cfg$diff_count(diff, &count);

	cfg$diff_free(diff);
	cfg$free(old);
	cfg$free(new);

	return count ? 1 : 0;			/* as diff(1): 1 - they differ		*/
```

`examples/router-new.cfg` is `router.cfg` edited by hand: port 9 has another MTU, port 10 is
gone, port 11 is new, a VLAN and a syslog server are added, and two settings of SYSTEM changed
places (which is not a difference):

```
$ ex5_diff router.cfg router-new.cfg notime
CONFIGURATION DIFFERENCES

Old: router.cfg
     UUID a0f0b155-8348-5765-97a7-4dfb5164faba
New: router-new.cfg
     UUID 675a82eb-f1b2-5bc6-9b79-4de26c7f01c6

************
File router.cfg
    15   ports.[id=9].mtu = 9216;
******
File router-new.cfg
    11   ports.[id=9].mtu = 1500;
************
File router.cfg
    16   ports.[id=10] = { id = 10; speed = "10G"; enabled = false; mtu = 9216; };
******
File router-new.cfg
        (none)
************
File router.cfg
        (none)
******
File router-new.cfg
    12   ports.[id=11] = { id = 11; speed = "10G"; enabled = true; };
************
File router.cfg
    19   vlans = [ 1, 100, 200 ];
******
File router-new.cfg
    15   vlans = [ 1, 100, 200, 300 ];
************
File router.cfg
        (none)
******
File router-new.cfg
    16   syslog = "10.0.0.5";
************

Number of difference sections found: 5
   changed: 2, added: 2, removed: 1

CONFIGURATION DIFFERENCES /KEYS=(ID,NAME,KEY)-
    router.cfg-
    router-new.cfg
```

The line numbers are those of each file. Without `notime` the listing carries the time it
was made and the times of the last change of both files (UTC):

```
CONFIGURATION DIFFERENCES                                                        22-NOV-2026 14:38:54.00

Old: router.cfg
     22-NOV-2026 14:31:30.00  UUID a0f0b155-8348-5765-97a7-4dfb5164faba
```

A program that does not want the text walks the differences itself:

```c
uint32_t	n, op;
const char	*path;
CFG$NODE	*was, *now;

cfg$diff_count(diff, &n);

for ( uint32_t i = 0; i < n; i++ )
	{
	cfg$diff_entry(diff, i, &op, &path, &was, &now);

	if ( op == CFG$K_DIFF_DEL )
		... take PATH down on the equipment ...
	}
```

### 4.6 Validating a File

`examples/ex6_schema.c` describes `router.cfg` by a schema and checks a file by it:

```c
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

```

```c
	sts = cfg$validate(tree, s_router, NULL, &nerr);

	if ( sts & 1 )
		printf("%s: valid\n", ...);
	else	{
		printf("%s: %u problem(s)\n", ..., nerr);
		cfg$puterrs(tree);			/* each with its line and column		*/
		}
```

A file with mistakes, `examples/router-bad.cfg`:

```
# router-bad.cfg - router.cfg with mistakes, for example 6

system = {
	hostnme = "edge-1";			# a typo
};

ports = (
	{ id = 1;  speed = "100M"; enabled = true; },
	{ id = 9;  speed = "10G";  enabled = "yes"; mtu = 9216; },
	{ id = 9;  speed = "10G";  enabled = false; mtu = 65000; }
);

vlans = [ 1, 100, 5000 ];
```

One run tells everything:

```
$ ex6_schema router-bad.cfg
%CFG-E-UNKNOWN, unknown setting: system.hostnme, router-bad.cfg line 4 column 2
%CFG-E-REQUIRED, missing setting: system.hostname, router-bad.cfg line 3 column 1
%CFG-E-BADKWD, not an allowed value: ports.[id=1].speed = "100M", allowed: 1G, 10G, router-bad.cfg line 8 column 13
%CFG-E-VTYPE, wrong type: ports.[id=9].enabled: a boolean expected, found a string, router-bad.cfg line 9 column 29
%CFG-E-DUPKEY, duplicate key: ports.[id=9], the same as line 9, router-bad.cfg line 10 column 2
%CFG-E-RANGE, out of range: ports.[id=9].mtu = 65000, allowed 64..9216, router-bad.cfg line 10 column 46
%CFG-E-RANGE, out of range: vlans.[2] = 5000, allowed 1..4094, router-bad.cfg line 13 column 19
router-bad.cfg: 7 problem(s)
```

(A program built without `NDEBUG` puts the time, the thread and the place in the code before
every message, as all StarLet messages do.)

Validate a file **before** you apply it, and give the user the whole list: it is much kinder
than stopping at the first mistake.

---

# Part II. Reference

Every routine returns an `unsigned` condition value. In the argument tables, *access* says
whether the routine reads the argument (read), writes it (write) or both; *mechanism* says how
it is passed.

---

## CFG$INIMSG - Register the Message Texts

**Format**

```c
unsigned cfg$inimsg (void);
```

**Returns** - condition value.

**Description**

Registers the message catalogue of the CFG facility with StarLet, so that `$PUTMSG_FAO` of a
CFG$_ value and CFG$PUTERRS put out the texts of Appendix A. Call it once, before the first
message; it is not thread-safe. A second call does no harm.

**Condition Values Returned**

| Value | Meaning |
|---|---|
| STS$K_SUCCESS | Registered. |
| STS$K_WARN | Registered before. Not an error. |

---

## CFG$PARSE_FILE - Parse a File into a Tree

**Format**

```c
unsigned cfg$parse_file (const char *a_path, uint32_t a_opts, CFG$TREE **a_tree);
```

**Returns** - condition value.

**Arguments**

| Argument | Type | Access | Mechanism | Description |
|---|---|---|---|---|
| a_path | char | read | by reference, zero-terminated | The file name. |
| a_opts | uint32_t | read | by value | Options: 0, or a sum of CFG$M_NOCASE (names are looked up regardless of case; `A` and `a` in one group are then duplicates) and CFG$M_NOINCLUDE (`@include` is refused). |
| a_tree | CFG$TREE * | write | by reference | The new tree. |

**Description**

Reads the whole file and parses it, with the files it includes, into a new tree.

The tree is made **even when the parse fails**: it holds the diagnostics for CFG$PUTERRS. So
call CFG$FREE whenever `*a_tree` is not NULL. It is NULL only when there was no memory.

The parse stops at the first error.

**Condition Values Returned**

| Value | Meaning |
|---|---|
| CFG$_NORMAL | Parsed. |
| CFG$_OPENIN, CFG$_READERR | The file (or an included one) could not be read. |
| CFG$_SYNTAX, CFG$_UNTERM, CFG$_BADNUM, CFG$_BADESC, CFG$_DUPNAME, CFG$_MIXARRAY | An error in the text, see Appendix A. |
| CFG$_INCLUDE, CFG$_INCDEPTH, CFG$_INCLOOP, CFG$_INCTWICE | A problem with `@include`. |
| CFG$_TOOBIG | A file, a name or the nesting is too big (Appendix B). |
| CFG$_NOMEM | No memory. |
| CFG$_BADARG | `a_path` is NULL. |

**Example**

```c
CFG$TREE	*tree;
unsigned	sts = cfg$parse_file("/etc/router.cfg", CFG$M_NOCASE, &tree);

if ( !(sts & 1) )
	cfg$puterrs(tree);
```

---

## CFG$PARSE_BUF - Parse a Buffer into a Tree

**Format**

```c
unsigned cfg$parse_buf (const char *a_buf, uint32_t a_len, const char *a_name, uint32_t a_opts, CFG$TREE **a_tree);
```

**Arguments**

| Argument | Type | Access | Mechanism | Description |
|---|---|---|---|---|
| a_buf | char | read | by reference | The text; copied - the buffer may go after the call. |
| a_len | uint32_t | read | by value | Its length. |
| a_name | char | read | by reference, zero-terminated | A name for the diagnostics, and the base of relative `@include` names; NULL gives "<buffer>". |
| a_opts | uint32_t | read | by value | As for CFG$PARSE_FILE. |
| a_tree | CFG$TREE * | write | by reference | The new tree. |

**Description**

As CFG$PARSE_FILE, but the text comes from memory. CFG$WRITE of such a tree writes the main
text into a file named `a_name` - give a real file name if you mean to save.

**Condition Values Returned** - as CFG$PARSE_FILE.

---

## CFG$PUTERRS - Put Out the Diagnostics of a Tree

**Format**

```c
unsigned cfg$puterrs (CFG$TREE *a_tree);
```

**Description**

Puts out every diagnostic recorded in the tree, in the order met, through `$PUTMSG_FAO`. At
most CFG$K_MAXERRS are kept. NULL is accepted and does nothing.

**Condition Values Returned** - STS$K_SUCCESS.

---

## CFG$FREE - Free a Tree

**Format**

```c
unsigned cfg$free (CFG$TREE *a_tree);
```

**Description**

Releases the tree, all its nodes and strings, and the text of its source files. Every node
pointer and every string got from the tree is invalid afterwards. NULL is accepted.

**Condition Values Returned** - STS$K_SUCCESS.

---

## CFG$SOURCE - Tell the File of a Source

**Format**

```c
unsigned cfg$source (CFG$TREE *a_tree, uint32_t a_srcno, const char **a_path);
```

**Arguments**

| Argument | Type | Access | Mechanism | Description |
|---|---|---|---|---|
| a_srcno | uint32_t | read | by value | 0 - the main file; 1, 2, ... - the included files, in the order they were met. |
| a_path | char * | write | by reference | The file name, owned by the tree. |

**Condition Values Returned**

| Value | Meaning |
|---|---|
| STS$K_SUCCESS | Done. |
| CFG$_NOTFOUND | No such source - a loop `for (s = 0; cfg$source(t, s, &p) & 1; s++)` ends here. |

---

## CFG$ROOT - Get the Root Group

**Format**

```c
unsigned cfg$root (CFG$TREE *a_tree, CFG$NODE **a_node);
```

**Description**

Gives the root group of the tree - the starting point of every path.

---

## CFG$LOOKUP - Find a Node by a Path

**Format**

```c
unsigned cfg$lookup (CFG$NODE *a_base, const char *a_path, CFG$NODE **a_node);
```

**Arguments**

| Argument | Type | Access | Mechanism | Description |
|---|---|---|---|---|
| a_base | CFG$NODE | read | by reference | Where the path starts. |
| a_path | char | read | by reference, zero-terminated | The path (section 3.3); NULL or "" - the base itself. |
| a_node | CFG$NODE * | write | by reference | The node found. |

**Condition Values Returned**

| Value | Meaning |
|---|---|
| STS$K_SUCCESS | Found. |
| CFG$_NOTFOUND | Nothing on the path (a warning: the low bit is clear). |
| CFG$_BADPATH | The path is malformed: an unclosed `[`, `[x]` with a non-digit, a bad key name. |

---

## CFG$NODE_INFO - Tell What a Node Is

**Format**

```c
unsigned cfg$node_info (CFG$NODE *a_node, CFG$INFO *a_info);
```

**Description**

Fills a `CFG$INFO`:

| Field | Meaning |
|---|---|
| name, namelen | The name - **not zero-terminated**; NULL for an element of a list or an array. |
| type | CFG$K_TYPE_GROUP, _LIST, _ARRAY, _INT, _INT64, _FLOAT, _BOOL, _STRING. |
| count | Live children of a group, a list or an array. |
| file | The source file of the node. |
| line, col | Where it starts in the file, from 1; 0 for a node the program made. |

---

## CFG$FIRST, CFG$NEXT - Go Over Children

**Format**

```c
unsigned cfg$first (CFG$NODE *a_parent, CFG$NODE **a_node);
unsigned cfg$next  (CFG$NODE *a_node,   CFG$NODE **a_next);
```

**Description**

CFG$FIRST gives the first live child of a group, a list or an array; CFG$NEXT the live sibling
after a node. Removed nodes are skipped. The usual loop:

```c
for ( sts = cfg$first(group, &n); sts & 1; sts = cfg$next(n, &n) )
	...
```

**Condition Values Returned**

| Value | Meaning |
|---|---|
| STS$K_SUCCESS | There is one. |
| CFG$_NOTFOUND | No (more) children. |

---

## CFG$GET_INT, _INT64, _FLOAT, _BOOL, _STRING - Read a Value

**Format**

```c
unsigned cfg$get_int    (CFG$NODE *a_base, const char *a_path, int32_t *a_val);
unsigned cfg$get_int64  (CFG$NODE *a_base, const char *a_path, int64_t *a_val);
unsigned cfg$get_float  (CFG$NODE *a_base, const char *a_path, double *a_val);
unsigned cfg$get_bool   (CFG$NODE *a_base, const char *a_path, int *a_val);
unsigned cfg$get_string (CFG$NODE *a_base, const char *a_path, const char **a_str, uint32_t *a_len);
```

**Description**

Find the node on the path (as CFG$LOOKUP) and give its value.

* CFG$GET_INT takes an integer of either size whose value fits 32 bits.
* CFG$GET_INT64 takes an integer of either size.
* CFG$GET_FLOAT takes a float or an integer (converted).
* CFG$GET_STRING gives the string with the escapes resolved. It is zero-terminated, but it may
  hold zeros (`\x00`) - use the length. It belongs to the tree.

**Condition Values Returned**

| Value | Meaning |
|---|---|
| STS$K_SUCCESS | Done. |
| CFG$_NOTFOUND, CFG$_BADPATH | As CFG$LOOKUP. |
| CFG$_TYPE | The node is of another type, or a 64-bit value does not fit CFG$GET_INT. |

---

## CFG$SET_INT, _INT64, _FLOAT, _BOOL, _STRING - Write a Value

**Format**

```c
unsigned cfg$set_int    (CFG$NODE *a_base, const char *a_path, int32_t a_val);
unsigned cfg$set_int64  (CFG$NODE *a_base, const char *a_path, int64_t a_val);
unsigned cfg$set_float  (CFG$NODE *a_base, const char *a_path, double a_val);
unsigned cfg$set_bool   (CFG$NODE *a_base, const char *a_path, int a_val);
unsigned cfg$set_string (CFG$NODE *a_base, const char *a_path, const char *a_str, uint32_t a_len);
```

**Description**

Set the value of the node on the path. If the path names settings that are not there, they are
**made**: the missing ones in the middle as groups, the last one as a setting of the type. An
element (`[n]`, `[key=value]`) is never made - add it with CFG$ADD.

The new text keeps the form of the old: a number written in hexadecimal stays hexadecimal, an
integer that no longer fits 32 bits becomes 64-bit and gets the `L` suffix. A float is
written with the fewest digits that read back as the same double. A string is quoted and
escaped.

An integer and a 64-bit integer may replace each other; any other change of type is refused.
CFG$SET_FLOAT refuses infinity and NaN - the syntax has no text for them.

**Condition Values Returned**

| Value | Meaning |
|---|---|
| STS$K_SUCCESS | Done. |
| CFG$_TYPE | The node is of another type, or a group/list/array stands on the path where a setting should be made. |
| CFG$_NOTFOUND | An element on the path is not there. |
| CFG$_BADNAME | A setting to be made has a name the syntax does not allow. |
| CFG$_BADPATH, CFG$_BADARG, CFG$_NOMEM | As the names say. |

**Example**

```c
cfg$set_int(root, "system.snmp.port", 161);	/* makes SNMP and PORT if missing */
cfg$set_bool(root, "ports.[id=10].enabled", 1);
```

---

## CFG$ADD - Add a Node

**Format**

```c
unsigned cfg$add (CFG$NODE *a_parent, const char *a_name, uint32_t a_type, CFG$NODE **a_node);
```

**Arguments**

| Argument | Type | Access | Mechanism | Description |
|---|---|---|---|---|
| a_parent | CFG$NODE | read | by reference | A group, a list or an array. |
| a_name | char | read | by reference, zero-terminated | The name in a group; NULL in a list or an array. |
| a_type | uint32_t | read | by value | CFG$K_TYPE_*. |
| a_node | CFG$NODE * | write | by reference | The new node; may be NULL. |

**Description**

Puts a new node at the end of the container. A scalar starts as 0, 0.0, false or "". The node is
written after the last sibling, with its indentation.

**Condition Values Returned**

| Value | Meaning |
|---|---|
| STS$K_SUCCESS | Added. |
| CFG$_EXISTS | The group has a setting of that name. |
| CFG$_BADNAME | Not a valid name. |
| CFG$_TYPE | The parent is a scalar; or a group/list into an array; or a scalar of another type into an array. |
| CFG$_BADARG | No name for a group, a name for a list or an array, a bad type. |

---

## CFG$DELETE - Remove a Node

**Format**

```c
unsigned cfg$delete (CFG$NODE *a_node);
```

**Description**

Removes the node with all below it. Its text leaves the file at the next write, with the
comment lines above it and the comment after it. In a list or an array the commas are fixed up.

**Condition Values Returned**

| Value | Meaning |
|---|---|
| STS$K_SUCCESS | Removed. |
| CFG$_BADARG | The root, a node removed already. |

---

## CFG$WRITE_BUF - Get the Text of a Source

**Format**

```c
unsigned cfg$write_buf (CFG$TREE *a_tree, uint32_t a_srcno, char **a_buf, uint32_t *a_len);
```

**Description**

Builds the text of one source file of the tree (0 - the main one) into a new buffer. The buffer
is zero-terminated; free it with `free()`. Nothing is written to disk.

**Condition Values Returned** - STS$K_SUCCESS, CFG$_BADARG (no such source), CFG$_NOMEM.

---

## CFG$WRITE - Write Back the Changed Files

**Format**

```c
unsigned cfg$write (CFG$TREE *a_tree);
```

**Description**

For every source of the tree, builds its text; if the text differs from what was read, writes
it into the file in its place - a temporary file `<name>.yalibcfg-XXXXXX` in the same
directory, synced, renamed over the old file. The mode and the owner of the old file are kept
(the owner as far as the process may change it); a symbolic link is followed - the link stays,
the file it points to is replaced. A file that did not change is not touched.

**Condition Values Returned**

| Value | Meaning |
|---|---|
| STS$K_SUCCESS | Done. |
| CFG$_WRITERR | A file could not be written; the tree records which one and the errno. |
| CFG$_BROKEN | The tree comes from a failed parse. |
| CFG$_NOMEM | No memory. |

---

## CFG$WRITE_FILE - Write the Main File Elsewhere

**Format**

```c
unsigned cfg$write_file (CFG$TREE *a_tree, const char *a_path);
```

**Description**

Writes the text of the main source into `a_path`, the same safe way as CFG$WRITE. The
`@include` directives are written as they are; the included files are not copied. When an
included file has changed, the call is refused with CFG$_INCMODIFIED: that change would go
nowhere - write it in place with CFG$WRITE.

---

## CFG$SET_VALUE - Write a Value from Its Text

**Format**

```c
unsigned cfg$set_value (CFG$NODE *a_base, const char *a_path, const char *a_text, uint32_t a_len);
```

**Description**

Sets a scalar from its text as it is written in a file - for a program that has the value as
text, a command line for one (CFGTOOL SET). `9216`, `0x1F`, `12L`, `1.5`, `true`,
`"text"` give their types; any text that is not exactly one such value (`10.0.0.5`, `12abc`,
`1 2`, `12 # note`, an empty one) is set as a string. A setting that is there already keeps
its type where the text allows: a string takes the text as it is, a float takes an integer.
An integer keeps the form of its text: hexadecimal stays hexadecimal, `L` makes it 64-bit.

**Condition Values Returned** - as CFG$SET_*.

---

## CFG$NODE_TEXT - Get the Text of a Node

**Format**

```c
unsigned cfg$node_text (CFG$NODE *a_node, uint32_t a_opts, char **a_buf, uint32_t *a_len);
```

**Description**

Gives the canonical text of a node in a new buffer (free it with `free()`): the value, or with
CFG$M_TEXT_NAME `name = value;`; a group or a list over lines with a tab of indentation, or on
one line with CFG$M_TEXT_FLAT.

**Condition Values Returned** - STS$K_SUCCESS, CFG$_NOMEM, CFG$_BADARG (a removed node).

---

## CFG$WRITE_CANONICAL - Get the Canonical Text of a Tree

**Format**

```c
unsigned cfg$write_canonical (CFG$TREE *a_tree, char **a_buf, uint32_t *a_len);
```

**Description**

Gives the settings of the whole tree in the canonical layout: one setting on a line, groups
over lines with a tab of indentation, the settings of included files in place of the
`@include` lines. **Comments and the layout of the files are not kept.** The text reads back
to the same settings - the same fingerprint. This is what CFGTOOL FORMAT writes.

**Condition Values Returned** - STS$K_SUCCESS, CFG$_BROKEN, CFG$_NOMEM, CFG$_BADARG.

---

## CFG$VALIDATE - Validate a Tree by a Schema

**Format**

```c
unsigned cfg$validate (CFG$TREE *a_tree, const CFG$SCHEMA *a_schema, void *a_arg, uint32_t *a_nerr);
```

**Arguments**

| Argument | Type | Access | Mechanism | Description |
|---|---|---|---|---|
| a_tree | CFG$TREE | modify | by reference | The tree. The record of its diagnostics is started afresh. |
| a_schema | CFG$SCHEMA | read | by reference | The table of the settings of the root group, ended by an entry with a NULL name (section 3.7). |
| a_arg | void | read | by reference | Passed to the check routines of the schema, as it is. |
| a_nerr | uint32_t | write | by reference | The number of diagnostics - errors and warnings, kept or not; may be NULL. |

**Description**

Checks every node of the tree against its entry of the schema: the type, the range, the length,
the keywords, the addresses, the number of elements, the required settings, the settings the
schema does not know, the uniqueness of the keys of lists, and last the check routines of the
program.

The validation **does not stop at the first error**. Every problem is recorded in the tree with
its position and the path of the setting, for CFG$PUTERRS; at most CFG$K_MAXERRS are kept, the
rest are counted and CFG$PUTERRS says how many were not kept (CFG$_MOREERRS). A node of a wrong
type is reported once, and what is inside it is not looked at.

The schema itself is checked first: a KWD entry without words, a key outside a list element or
not a scalar, two keys, MIN greater than MAX, a list without the entry for its elements, a
schema deeper than 64 levels - all are the programmer's mistakes, returned as CFG$_BADSCHEMA
before any file is blamed. A schema may refer to itself (a menu whose items are menus): each
entry is checked once.

**Condition Values Returned**

| Value | Meaning |
|---|---|
| STS$K_SUCCESS | The tree matches the schema. There may be warnings (DEPRECATED) in the record. |
| CFG$_INVALID | It does not: the diagnostics are in the tree. |
| CFG$_BADSCHEMA | The schema is wrong. |
| CFG$_BROKEN | The tree comes from a failed parse. |
| CFG$_BADARG, CFG$_NOMEM | As the names say. |

---

## CFG$FINGERPRINT - Get the Fingerprint of a Tree

**Format**

```c
unsigned cfg$fingerprint (CFG$TREE *a_tree, uint8_t a_uuid[16]);
```

**Description**

Computes the fingerprint of the tree (section 3.6, Appendix D): 16 octets of a UUID of version
5, in the order of RFC 9562. Put it out with the FAO directive `!%U`:

```c
uint8_t	uuid[16];

cfg$fingerprint(tree, uuid);
$LOG_FAO(STS$K_INFO, "configuration !%U", uuid);
```

The fingerprint is of the tree as it is now: after a change, compute it again.

**Condition Values Returned** - STS$K_SUCCESS, CFG$_NOMEM, CFG$_BADARG.

---

## CFG$DIFF - Compare Two Trees

**Format**

```c
unsigned cfg$diff (CFG$TREE *a_old, CFG$TREE *a_new, const CFG$SCHEMA *a_schema, uint32_t a_opts, CFG$DIFF **a_diff);
```

**Arguments**

| Argument | Type | Access | Mechanism | Description |
|---|---|---|---|---|
| a_old, a_new | CFG$TREE | read | by reference | The trees. They must live as long as the comparison. |
| a_schema | CFG$SCHEMA | read | by reference | NULL, or the schema of the trees: lists are then matched by the key it flags (CFG$M_KEY) - when every element has it and no two share it; else by position. |
| a_opts | uint32_t | read | by value | 0, or CFG$M_POSITIONAL - lists by position only. |
| a_diff | CFG$DIFF * | write | by reference | The comparison. Free it with CFG$DIFF_FREE. |

**Description**

Compares the settings of the trees by the rules of section 3.6. The differences are in the
order of the old tree, the additions after the settings of their group.

**Condition Values Returned**

| Value | Meaning |
|---|---|
| STS$K_SUCCESS | Compared - whether there are differences or not: ask CFG$DIFF_COUNT. |
| CFG$_NOMEM, CFG$_BADARG | As the names say. |

---

## CFG$DIFF_COUNT, CFG$DIFF_ENTRY - Read the Differences

**Format**

```c
unsigned cfg$diff_count (CFG$DIFF *a_diff, uint32_t *a_count);
unsigned cfg$diff_entry (CFG$DIFF *a_diff, uint32_t a_idx, uint32_t *a_op, const char **a_path,
			 CFG$NODE **a_old, CFG$NODE **a_new);
```

**Description**

CFG$DIFF_COUNT gives the number of differences, CFG$DIFF_ENTRY one of them (from 0):

| Output | Meaning |
|---|---|
| a_op | CFG$K_DIFF_MOD, CFG$K_DIFF_ADD or CFG$K_DIFF_DEL. |
| a_path | The path, owned by the comparison; it works with CFG$LOOKUP on either tree. |
| a_old, a_new | The nodes; NULL on the side the setting is missing from. |

Any output argument may be NULL.

**Condition Values Returned** - STS$K_SUCCESS; CFG$_NOTFOUND past the last entry.

---

## CFG$DIFF_LISTING - Make the Listing of a Comparison

**Format**

```c
unsigned cfg$diff_listing (CFG$DIFF *a_diff, uint32_t a_opts, char **a_buf, uint32_t *a_len);
```

**Arguments**

| Argument | Type | Access | Mechanism | Description |
|---|---|---|---|---|
| a_opts | uint32_t | read | by value | 0, or CFG$M_NOTIME - no times: the same comparison gives the same text on every run (for tests, for listings kept under version control). |
| a_buf, a_len | char *, uint32_t | write | by reference | The text, zero-terminated; free it with `free()`. |

**Description**

Makes the listing shown in section 4.5: a header with both files and their fingerprints, a
section for every difference with the line in each file and the setting in its canonical
form, a summary, and a trailer naming the files and how lists were matched: `/KEYS=(ID,NAME,KEY)`,
`/SCHEMA` or `/POSITIONAL`. Values are written on one line: a group as `{ a = 1; b = 2; }`.

**Condition Values Returned** - STS$K_SUCCESS, CFG$_NOMEM, CFG$_BADARG.

---

## CFG$DIFF_FREE - Free a Comparison

**Format**

```c
unsigned cfg$diff_free (CFG$DIFF *a_diff);
```

**Description**

Frees the comparison and its paths. The trees are not touched. NULL is accepted.

---

# Part III. The CFGTOOL Utility

CFGTOOL is the command-line utility of the library: it checks, formats, compares, reads and
changes configuration files from the shell and from scripts, with a DCL-style command line.
It is in the package `cfgtool`; `HELP CFGTOOL` shows its help library (the HELP package).

## CFGTOOL -- Format

```
CFGTOOL command parameters [/qualifiers]
```

| Command | What it does |
|---|---|
| CHECK file | Checks a file; tells its fingerprint |
| FORMAT file | Writes the settings in the canonical form |
| DIFF old-file new-file | Compares two configurations, writes the listing |
| GET file path | Writes the value of a setting |
| SET file path value | Changes a setting, writes the file back with its comments |
| DELETE file path | Deletes a setting, writes the file back |
| HELP | The summary of the commands |

A command and a qualifier may be abbreviated while the abbreviation is unambiguous; a
qualifier may be written in the Unix long form, `--fingerprint` for `/FINGERPRINT`.
`CFGTOOL command /HELP` lists the parameters and qualifiers of a command.

Messages go to the standard error, through `$PUTMSG_FAO`; data - a value, a listing, a text, a
fingerprint - to the standard output. The **exit status**:

| Status | Meaning |
|---|---|
| 0 | Success; for DIFF - no differences |
| 1 | DIFF found differences |
| 2 | An error |

A path is written as in the library (section 3.3): `system.hostname`, `vlans.[1]`,
`ports.[id=9].mtu`. Quote it against the shell when it holds brackets or blanks.

Two rules of the command line. An argument is at most 255 octets (CFG$_ARGLONG otherwise). A
value or a path that begins with `/` or `--` would be taken for a qualifier: put it in double
quotes inside the quotes of the shell - `cfgtool set r.cfg system.logdir '"/var/log"'`.

/OUTPUT writes through a temporary file and a rename: an old file is never left half written.

Qualifiers that several commands take:

| Qualifier | Meaning |
|---|---|
| /NOCASE | Names of settings are compared regardless of case (CFG$M_NOCASE) |
| /NOINCLUDE | An `@include` is an error (CFG$M_NOINCLUDE) |
| /OUTPUT=file | Data goes into the file, not to the standard output |
| /LOG | SET and DELETE report what they did |

## CHECK -- Check a File

```
CFGTOOL CHECK file [/FINGERPRINT /NOCASE /NOINCLUDE]
```

Parses the file with the files it includes. A correct file is announced with the number of its
settings and its fingerprint (section 3.6); every error - with the file, the line and the
column. `/FINGERPRINT` writes the fingerprint alone to the standard output, for a script.

```
$ cfgtool check router.cfg
%CFG-S-CORRECT, router.cfg is correct: 16 settings, UUID a0f0b155-8348-5765-97a7-4dfb5164faba

$ id=$(cfgtool check router.cfg /fingerprint 2>/dev/null)
```

## FORMAT -- Write the Canonical Form

```
CFGTOOL FORMAT file [/OUTPUT=file /NOCASE /NOINCLUDE]
```

Writes the settings of the file in the canonical layout (CFG$WRITE_CANONICAL): one setting on a
line, groups over lines with a tab of indentation, the settings of included files in place of
the `@include` lines. **The comments are not kept** - this is the text of the settings alone,
for comparing files as texts or for giving a file a clean look. The canonical text has the same
fingerprint as the file.

## DIFF -- Compare Two Configurations

```
CFGTOOL DIFF old-file new-file [/OUTPUT=file /POSITIONAL /NOTIME /NOCASE]
```

Compares the settings (CFG$DIFF) and writes the listing of section 4.5 (CFG$DIFF_LISTING).
`/POSITIONAL` matches the elements of lists by position only, `/NOTIME` leaves the times out
of the listing. The exit status is 1 when there are differences, so a script can ask:

```
$ if ! cfgtool diff running.cfg saved.cfg /notime /output=changes.lis ; then
>     echo "the configuration has changed, see changes.lis"
> fi
```

## GET -- Read a Setting

```
CFGTOOL GET file path [/FLAT /NOCASE]
```

Writes the value: a string as it is (escapes resolved, no quotes), another scalar as in a file,
a group or a list over lines - or on one line with `/FLAT`.

```
$ cfgtool get router.cfg ports.[id=9].mtu
9216
$ cfgtool get router.cfg system /flat
{ hostname = "edge-1"; location = "Lab, rack 3"; }
```

## SET -- Change a Setting

```
CFGTOOL SET file path value [/TYPE={AUTO|STRING} /OUTPUT=file /LOG /NOCASE]
```

Sets the value and writes the file back in place (CFG$WRITE): only the value changes, the
comments and the layout stay. A setting that is not there is made with the groups on its path.

With `/TYPE=AUTO`, the default, the type comes from the text as in a file (CFG$SET_VALUE):
`1500` is an integer, `0x1F` a hexadecimal one (written back in hexadecimal), `12L` a 64-bit
one, `1.5` a float, `true` a boolean, `"text"` a string. Whatever is not exactly one such
value - `10.0.0.5`, `12abc`, `12 # note` - is a string, as it stands. A setting that is there
already keeps its type where the text allows: a string takes the text as it is (`12` stays the
string "12"), a float takes an integer (`2` becomes 2.0). `/TYPE=STRING` sets the text as a
string always.

`/OUTPUT` writes the changed main file elsewhere and leaves the original as it was. It is
refused (CFG$_INCMODIFIED) when the change falls into an included file: that change would be
lost. Written in place, a file keeps its mode and owner; a symbolic link stays a link, the file
it points to is changed.

```
$ cfgtool set router.cfg ports.[id=9].mtu 1500
$ cfgtool set router.cfg system.snmp.community public /log
%CFG-S-MODIFIED, system.snmp.community set in router.cfg
```

## DELETE -- Delete a Setting

```
CFGTOOL DELETE file path [/OUTPUT=file /LOG /NOCASE]
```

Deletes the setting with the comment lines above it (CFG$DELETE) and writes the file back.

---

# Appendix A. Messages

Every message has the form `%CFG-s-IDENT, text`, where *s* is the severity: S - success,
I - information, W - warning, E - error, F - fatal. The messages of a parse end with the file,
the line and the column.

**BROKEN, the tree comes from a failed parse**
*Explanation:* A write, a comparison or a fingerprint was asked of a tree whose parse failed.
*User Action:* Put out the diagnostics of the parse (CFG$PUTERRS), correct the file, parse it
again.

**BADARG, bad argument**
*Explanation:* A routine got a NULL pointer, a type out of range, a name where none is
allowed. *User Action:* A program error - check the call.

**BADESC, bad escape sequence** *esc*
*Explanation:* A string holds a backslash followed by something other than `\ " n r t f x`,
or `\x` without two hexadecimal digits. *User Action:* Write `\\` for a backslash.

**BADNAME, not a valid setting name**
*Explanation:* A name must start with a letter or `*` and hold letters, digits, `-`, `_`,
`*`. *User Action:* Choose another name.

**BADNUM, bad number** *text*
*Explanation:* A number is malformed (`0x` without digits, `12abc`, a binary digit 2) or
out of range (beyond 64 bits; a float beyond the double range). *User Action:* Correct the
number; add `L` for a 64-bit integer.

**BADPATH, malformed path**
*Explanation:* A path given to a routine is malformed. *User Action:* See section 3.3.

**DUPNAME, duplicate setting** *name*
*Explanation:* A group holds two settings of one name (with CFG$M_NOCASE - of one name in any
case). *User Action:* Remove one of them.

**EXISTS, a setting of that name exists already**
*Explanation:* CFG$ADD of a name the group has. *User Action:* Use CFG$SET_* to change it,
or remove it first.

**INCDEPTH, include files nested too deep:** *name*
*Explanation:* More than CFG$K_MAXINCDEPTH levels of `@include`. *User Action:* Flatten the
files.

**INCLOOP, file includes itself:** *name*
*Explanation:* A file includes itself, directly or through others. *User Action:* Break the
loop.

**INCTWICE, file included a second time:** *name*
*Explanation:* A file is included twice in one tree. *User Action:* Include it once; if two
groups need the same settings, give them a list or copy the settings.

**INCLUDE, cannot include** *name*
*Explanation:* An included file could not be read; the OPENIN message before it says why.
*User Action:* Check the name - it is relative to the directory of the including file.

**MIXARRAY, array element** *value* **is not of the type of the first**
*Explanation:* An array holds scalars of different types. *User Action:* Make them one type,
or use a list `( )`.

**NOMEM, memory allocation failure**
*Explanation:* The process ran out of memory. *User Action:* Check the limits of the process.

**NORMAL, normal successful completion**

**NOTFOUND, no setting on the path**
*Explanation:* A warning: nothing on the path, or no more children. *User Action:* None, if
the setting is optional.

**OPENIN, cannot open** *file*, **errno=** *n*
*Explanation:* The file could not be opened. *User Action:* Check the name and the
protection; `errno` tells the cause (2 - no such file, 13 - no access).

**READERR, error reading** *file*, **errno=** *n*
*Explanation:* The file could not be read, is not a regular file (errno 22), or is bigger than
CFG$K_MAXFILE (errno 27). *User Action:* As the errno says.

**SYNTAX, syntax error:** *expected X, found Y*
*Explanation:* The text does not follow the syntax of chapter 2. *User Action:* Look at the
line and column given. A common cause is a missing `;` or `}` on the line **before**.
Another is a `,` before a closing `]` or `)` - not allowed, as in libconfig.

**TOOBIG,** *what* **is too big**
*Explanation:* A name longer than 255 characters, a file name too long, values nested deeper
than CFG$K_MAXDEPTH. *User Action:* Shorten it.

**TYPE, the setting is of another type**
*Explanation:* A get or a set of a type the node does not have. *User Action:* Use the routine
of the right type, or remove the node and add it anew.

**UNTERM, unterminated** *what*
*Explanation:* A string, a comment or a bracket is not closed before the end of the line
(strings) or of the file. The position is where it began. *User Action:* Close it.

**WRITERR, error writing** *file*, **errno=** *n*
*Explanation:* The file could not be written: no space (28), no access to the directory (13).
The old file is left as it was. *User Action:* As the errno says.

The messages of a validation (CFG$VALIDATE) begin with the path of the setting:

**BADKWD, not an allowed value:** *path = value, allowed: words*
*Explanation:* A keyword setting holds a word its schema does not list. *User Action:* Use one
of the words given.

**BADADDR, not a valid address:** *path = value is not ...*
*Explanation:* The string is not an IPv4, IPv6 or MAC address, as the schema wants.
*User Action:* Correct the address.

**BADSCHEMA, the schema is wrong**
*Explanation:* A programmer's mistake in the schema table (see CFG$VALIDATE). *User Action:*
Correct the table.

**CHECK,** *path: text*
*Explanation:* A check routine of the program refused the value; the text is the program's.
*User Action:* As the text says.

**COUNT, wrong number of elements:** *path has n, allowed min..max*
*User Action:* Add or remove elements.

**DEPRECATED, setting no longer in use:** *path*
*Explanation:* A warning: the setting is still taken, but will go. *User Action:* See the
documentation of the program for what replaces it.

**DUPKEY, duplicate key:** *path, the same as line n [of file]*
*Explanation:* Two elements of a list have the same key; the line is that of the first of them
(in another file, when it came from an included one), or "an element added by the program". *User Action:* Give them different
keys or remove one.

**INVALID, the configuration does not match its schema**
*Explanation:* The condition value of CFG$VALIDATE when it found errors; the diagnostics
themselves are in the tree.

**LENGTH, wrong length:** *path = value is n long, allowed min..max*
*User Action:* Shorten or lengthen the string.

**MOREERRS,** *n* **more diagnostics not kept**
*Explanation:* There were more problems than CFG$K_MAXERRS. *User Action:* Correct the ones
shown and validate again.

**RANGE, out of range:** *path = value, allowed min..max*
*User Action:* Give a value within the range.

**REQUIRED, missing setting:** *path*
*Explanation:* A required setting (or the key of a list element) is not there; the position is
that of the group it should be in. *User Action:* Add it.

**UNKNOWN, unknown setting:** *path*
*Explanation:* The schema does not know the setting - most often a typo. *User Action:* Check
the spelling.

**VTYPE, wrong type:** *path: X expected, found Y*
*Explanation:* The value is of another type than the schema wants; what is inside it is not
checked. *User Action:* Correct the value: a string in quotes, a boolean as `true` or
`false`, a group in braces.

The messages of CFGTOOL:

**CANTSET, cannot change** *path* **in** *file*
*Explanation:* SET or DELETE was refused; the next message says why (TYPE, BADNAME, NOTFOUND).
*User Action:* As the next message says. An element of a list is never made by SET.

**CORRECT,** *file* **is correct:** *n* **settings, UUID** *uuid*
*Explanation:* CHECK found the file correct.

**DELETED,** *path* **deleted from** *file*
*Explanation:* DELETE /LOG did it.

**DIFFERENT,** *n* **differences between** *old* **and** *new*
*Explanation:* DIFF found differences; the exit status is 1.

**IDENTICAL, no differences between** *old* **and** *new*
*Explanation:* DIFF found none.

**MODIFIED,** *path* **set in** *file*
*Explanation:* SET /LOG did it.

**NOSETTING, no setting** *path* **in** *file*
*Explanation:* GET or DELETE found nothing on the path. *User Action:* Check the path: GET the
group above it.

**ARGLONG, argument** *n* **is longer than 255 octets**
*Explanation:* An argument of the command line is too long to be taken whole. *User Action:*
Shorten it - a long value is better set by a program through CFG$SET_*.

**INCMODIFIED, a change in an included file cannot go into another file**
*Explanation:* CFG$WRITE_FILE (CFGTOOL /OUTPUT) was asked to write the main file elsewhere, but
a changed setting lives in an included file. *User Action:* Write in place (CFG$WRITE, CFGTOOL
without /OUTPUT).

**OPENOUT, cannot create** *file*, **errno=** *n*
*Explanation:* The file of /OUTPUT cannot be created. *User Action:* As the errno says.

---

# Appendix B. Limits

| Constant | Value | What |
|---|---|---|
| CFG$K_MAXNAME | 255 | The longest setting name. |
| CFG$K_MAXFILE | 64 MB | The biggest file read. |
| CFG$K_MAXSRC | 64 | Source files in one tree (the main one and the included). |
| CFG$K_MAXINCDEPTH | 10 | Nesting of `@include`. |
| CFG$K_MAXDEPTH | 256 | Nesting of values. |
| CFG$K_MAXERRS | 32 | Diagnostics kept in a tree. |

---

# Appendix C. Differences from libconfig

| | libconfig | yaLIBCFG |
|---|---|---|
| Comments and layout on write | lost | kept; a file without changes is written byte for byte |
| Decimal integer beyond 32 bits without `L` | 1.5: cut to 32 bits silently; 1.7: 64-bit | 64-bit |
| `@include` | pastes the text in, anywhere | whole settings only, where a setting may stand |
| The same file included twice | allowed | refused (CFG$_INCTWICE) - a change could not be written back to both places |
| A change in an included file | written into the main file on save | written into the included file |
| Errors | one, text and line | condition values with file, line and column |
| Writing | in place | a temporary file and a rename - never half a file |
| Comparison of two configurations | no | CFG$DIFF, a listing, a fingerprint |
| Library | LGPL-2.1 | Apache-2.0 |

---

# Appendix D. The Fingerprint

The fingerprint is a contract: the same settings give the same UUID on every machine and in
every version of yaLIBCFG. It is defined here so that it can be computed by other programs too.

1. Start SHA-1 with the 16 octets of the yaLIBCFG namespace
   `6b2b0e1e-3f6a-5c4e-9d1a-0c5f9a7e2b14`.
2. Feed the root group by these rules (all numbers big-endian):

   | Node | Octets |
   |---|---|
   | integer (32 or 64 bits) | `'I'`, the value as 8 octets |
   | float | `'F'`, the 8 octets of the IEEE 754 double |
   | boolean | `'B'`, 8 octets: 1 or 0 |
   | string | `'S'`, the length as 8 octets, the octets of the string (escapes resolved) |
   | group | `'G'`, the count as 8 octets, then for every setting in the order of the names (bytewise; for a tree parsed with CFG$M_NOCASE - of the names in lower case): one octet of the name length, the name, the setting |
   | list | `'L'`, the count as 8 octets, the elements in their order - or, when the list has a key (every element a group holding a scalar `id`, else `name`, else `key`, unique in the list), in the order of the keys: by type (integer < float < boolean < string, as CFG$K_TYPE_*), then integers, floats and booleans by their 8 octets as unsigned, strings bytewise, a shorter one first |
   | array | `'A'`, the count as 8 octets, the elements in their order |

3. Take the first 16 octets of the digest; set the high four bits of octet 6 to 5 (version) and
   the high two bits of octet 8 to binary 10 (variant).

Two values to check an implementation by:

| File | Fingerprint |
|---|---|
| empty | `7b6d66a7-5b7b-5f45-8072-b9bfd2711fa0` |
| `a = 1;` | `1acc6d4e-04d6-5cd7-9e3f-8588fda68b52` |

In Python:

```python
import hashlib, uuid
ns = uuid.UUID("6b2b0e1e-3f6a-5c4e-9d1a-0c5f9a7e2b14")
data = b"G" + (1).to_bytes(8, "big") + bytes([1]) + b"a" + b"I" + (1).to_bytes(8, "big")
b = bytearray(hashlib.sha1(ns.bytes + data).digest()[:16])
b[6] = (b[6] & 0x0F) | 0x50
b[8] = (b[8] & 0x3F) | 0x80
print(uuid.UUID(bytes=bytes(b)))	# 1acc6d4e-04d6-5cd7-9e3f-8588fda68b52
```
