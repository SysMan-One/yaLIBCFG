# CFGTOOL -- check, format, compare, read and change configuration files

Works with configuration files in the libconfig syntax, by the
yaLIBCFG library: checks a file, writes it in the canonical form,
compares two of them, reads a setting, changes or deletes one in
place - the comments and the layout of the file are kept.

Format:

```
CFGTOOL command parameters [/qualifiers]
```

The commands are CHECK, FORMAT, DIFF, GET, SET, DELETE and HELP. A
command and a qualifier may be abbreviated as long as the
abbreviation is unambiguous, and a qualifier may be spelled in the
Unix long form -- --NOCASE for /NOCASE.

Messages go to the standard error, data -- a value, a listing, a
text -- to the standard output. The exit status for the shell: 0 --
success (for DIFF: no differences), 1 -- DIFF found differences, 2 --
an error.

## Paths -- how a setting is named

A path leads from the top of the file to a setting:

```
system.hostname         the setting HOSTNAME of the group SYSTEM
vlans.[1]               element 1 (from 0) of a list or an array
ports.[id=9].mtu        in the list PORTS, the element whose ID is 9
ports.[name="up 1"]     a string key, in double quotes
```

A slash may stand for the dot: system/hostname. Quote a path that
holds brackets or blanks against the shell.

An argument is at most 255 octets. A value or a path that begins
with / or -- would be taken for a qualifier: put it in double quotes
inside the quotes of the shell:

```
$ cfgtool set router.cfg system.logdir '"/var/log"'
```

## /SKIPERRORS -- go on past bad settings

Every command takes it. A bad setting is skipped whole and
reported, the rest of the file is taken. CHECK /SKIPERRORS reports
all the errors of a file in one run (and fails); GET, SET, DELETE,
DIFF and FORMAT work with the rest of the file. SET and DELETE write
the file back with the bad settings as they stand.

```
$ cfgtool check router.cfg /skiperrors
```

## CHECK -- check a file

```
CFGTOOL CHECK file [/FINGERPRINT /NOCASE /NOINCLUDE]
```

Parses the file and the files it includes. A correct file is
announced with the number of its settings and its fingerprint; every
error is reported with the file, the line and the column.

```
$ cfgtool check router.cfg
%CFG-S-CORRECT, router.cfg is correct: 16 settings, UUID a0f0b155-8348-5765-97a7-4dfb5164faba
```

### /FINGERPRINT

Writes the fingerprint alone to the standard output, for a script.
The fingerprint is a UUID made of the settings: two files with the
same settings have the same one, whatever their comments and layout.

### /NOCASE

Names of settings are compared regardless of case.

### /NOINCLUDE

An @include directive is an error.

## FORMAT -- write the canonical form

```
CFGTOOL FORMAT file [/OUTPUT=file /NOCASE /NOINCLUDE]
```

Writes the settings of the file in the canonical layout: one setting
on a line, groups over lines with a tab of indentation, the settings
of included files in place of the @include lines.

The comments are NOT kept: this is the text of the settings alone.
Use it to compare two files as texts, or to give a file a clean
look.

### /OUTPUT=file

Writes into the file instead of the standard output.

## DIFF -- compare two configurations

```
CFGTOOL DIFF old-file new-file [/OUTPUT=file /POSITIONAL /NOTIME /NOCASE]
```

Compares the settings of two files -- not their text: comments,
layout and the order of the settings in a group do not count -- and
writes a listing in the manner of the DIFFERENCES utility, every
difference with its line in each file. Elements of a list are
matched by their key (id, name or key) when they have one.

The exit status is 1 when there are differences.

### /POSITIONAL

Elements of lists are matched by position only.

### /NOTIME

No times in the listing: the same comparison gives the same text on
every run.

### /OUTPUT=file

Writes the listing into the file.

## GET -- read a setting

```
CFGTOOL GET file path [/FLAT /NOCASE]
```

Writes the value of the setting to the standard output: a string as
it is (no quotes), any other value as in the file, a group or a list
over lines.

```
$ cfgtool get router.cfg ports.[id=9].mtu
9216
```

### /FLAT

A group or a list on one line.

## SET -- change a setting

```
CFGTOOL SET file path value [/TYPE=keyword /OUTPUT=file /LOG /NOCASE]
```

Sets the value and writes the file back in place: only the value
changes, the comments and the layout stay. A setting that is not
there is made, with the groups on its path.

```
$ cfgtool set router.cfg ports.[id=9].mtu 1500
$ cfgtool set router.cfg system.location "Lab, rack 4"
```

### /TYPE=keyword

AUTO (the default) takes the type from the text, as in a file: 1500
is an integer, 0x1F or %X1F a hexadecimal one (written back in its
form), 12L a 64-bit one, 1.5 a
float, true a boolean, "text" a string. Whatever is not exactly one
such value -- 10.0.0.5, 12abc, 12 # note -- is a string as it
stands. A setting that is there already keeps its type: a string
takes the text as it is, a float takes an integer. STRING sets the
text as a string always.

### /OUTPUT=file

Writes the changed main file into another file; the original stays.
Refused when the change falls into an included file: it would be
lost there.

### /LOG

Reports the change.

## DELETE -- delete a setting

```
CFGTOOL DELETE file path [/OUTPUT=file /LOG /NOCASE]
```

Deletes the setting with the comment lines above it and writes the
file back in place.

## HELP -- the summary of the commands

```
CFGTOOL HELP
CFGTOOL command /HELP
```

## Messages -- what the utility says

### CORRECT

The file is correct; the number of settings and the fingerprint
follow.

### SYNTAX, UNTERM, BADNUM, BADESC, DUPNAME, MIXARRAY

An error in the text of the file, with its position. Look at the
line given -- and at the line before it: a missing ; or } shows
itself one line later.

### OPENIN, READERR

The file cannot be read; errno tells why (2 -- no such file, 13 --
no access).

### NOSETTING

GET or DELETE found nothing on the path. Check the path with
CFGTOOL GET file on its parent.

### CANTSET

SET or DELETE was refused; the next message says why: TYPE -- the
setting is of another type, BADNAME -- not a valid name, NOTFOUND --
an element on the path is missing (elements are never made).

### DIFFERENT, IDENTICAL

What DIFF found.

### SKIPPED

/SKIPERRORS skipped the bad settings reported before it.

### INCMODIFIED

SET or DELETE /OUTPUT changed a setting of an included file, which
cannot go into another file. Write in place, without /OUTPUT.

### ARGLONG

An argument is longer than 255 octets.

### WRITERR, OPENOUT

A file cannot be written: no space (errno 28), no access (13). The
original file is left as it was.
