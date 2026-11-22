# yaLIBCFG

A C library for configuration files shaped as a tree, in a syntax compatible with libconfig,
for programs that keep their configuration declaratively: the file states the wanted state,
the program compares it with the running one and applies the difference.

What it adds over libconfig:

- **lossless write-back** - comments and layout survive; an unchanged file is written byte for byte;
- **schema validation** - types, ranges, keywords, required items, keys of lists, with
  OpenVMS-style condition values that name the file, line and column;
- **diff of two trees** - add / modify / delete with paths, lists of groups matched by key.

Status: stage 1 done - parse, tree, paths, lossless write-back, tests (round trip, edits,
diagnostics, comparison with libconfig).  Schema and diff are the next stages.  The design (in
Russian) is in `doc/yalibcfg_design_rus.md`.

License: Apache-2.0.  Copyright 2026 StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan).
