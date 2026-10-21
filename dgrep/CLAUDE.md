# CLAUDE.md

This file gives Claude Code guidance for working in this directory.

## What this is

**dgrep** is a fast Unix `egrep` clone, written in C by Jarmo Ruuth starting in
1988 (current version string `1.75` in `dgrep.c`). Petri ("Pete") Soini wrote
parts of it, notably the modified Boyer-Moore initialization in `bm.c` that
guarantees linear worst-case time, and the 2019 Linux port changes.

It was originally written for MS-DOS / Turbo C and later ported to Microsoft C,
OS/2, BSD 4.3 (gcc), QNX, Windows NT and Linux. It is historical code: the
style (K&R-era idioms, `REG1`/`REG2` register macros, `#if`-heavy portability
code, 16-bit/64k-segment concerns) is intentional and should be preserved.
Make minimal, targeted changes that match the surrounding code; don't
modernize or reformat it.

Documentation: `README` (English overview and benchmarks), `LUEMINUT`
(the same README in Finnish), `dgrep.doc` (detailed design notes, algorithm
references, exit codes, portability).

## Search strategy

`dgrep.c` picks the algorithm per expression:

- **Literal strings** -> Boyer-Moore (`bm.c`, derived from James A. Woods'
  fastgrep).
- **Regular expressions** -> a DFA built directly from the regex (Aho/Sethi/
  Ullman "Dragon book" followpos construction), with **lazy evaluation**:
  transitions are computed on demand and held in a cache.
- A "regmust" literal (a substring every match must contain) is extracted
  from the regex and searched first with Boyer-Moore to skip non-candidate
  lines quickly.

## Source layout

| File | Role |
|------|------|
| `dgrep.c` | `main`, option parsing, file I/O, buffering, output, search strategy |
| `bm.c` / `bm.h` | Boyer-Moore search (`gosper()` builds tables, `boyer_moore()` searches); Soini's O(m+n) version |
| `bm_d012.c` | Alternative Boyer-Moore with delta0/1/2 tables as in the original paper; used by the Linux/BSD makefile |
| `bm_word.c` | Experimental word-at-a-time Boyer-Moore (slower in practice, not built) |
| `om.c` | Sunday's Optimal Mismatch algorithm (experimental, not built) |
| `dfaregex.c` / `dfaregex.h` | Public regex API: `reg_comp`, `reg_comp_eol`, `reg_exec`; flags `RE_SEARCH`, `RE_IGNORECASE`, `RE_FULL`. Uses a single static `rbuf` (only one compiled regex at a time) |
| `dfatree.c` | Lexer + recursive-descent parser producing the syntax tree |
| `calcpos.c` | nullable / firstpos / lastpos / followpos |
| `dfa.c` / `dfa.h` | DFA construction with lazy transition cache |
| `regmust.c` | Extracts the must-match literal from the syntax tree |
| `set.c` / `set.h` | Bit-set routines used by the DFA construction |
| `clib.c` | Portability shims, buffer allocation (warned in-file as "a real mess") |
| `ectype.c` / `ectype.h` | Extended ctype tables (IBM PC / Scandinavian 7-bit charset), enabled with `-DECTYPE` |
| `getopt.c` / `getopt.h` | Turbo C getopt for DOS builds |
| `system.h` | All platform configuration and conditional-compilation switches |
| `try.c` | Regex test driver, adapted from Henry Spencer's regexp test program |
| `innerloo.*`, `stat`, `*.opt` | Old notes: assembly listings of the inner loops, timing statistics |

## Conditional compilation (see `system.h`)

`UNIX` (`\n` line ends; otherwise `\r\n`), `BSD`, `QNX`, `NT`, `MSC`,
`ECTYPE`, `TEST` (debug dumps, enables `-D`), `MANYREG`, `TOUCH`, and `FAST`
(faster DFA transition storage at the cost of memory). Builds should use
unsigned chars (`-funsigned-char` with gcc).

## Building

There are several makefiles, one per platform:

- **Linux**: `make -f makefile.l2x` (gcc, `-DUNIX -DFAST`, uses `bm_d012.c`)
- **BSD**: `make -f makefile.bsd`
- **Windows NT** (MSVC `cl`/nmake): `makefile` — links `..\lib\cwnt.lib` and
  a hardcoded VS 12.0 `setargv.obj` path for wildcard expansion; adjust as needed
- **DOS / OS/2**: `makefile.dos`, `makefile.os2` (historical)

Each makefile builds `dgrep` and the `try` test program. Object files and
binaries are not committed (see the repo `.gitignore`).

## Testing

Regression tests run `try` against tab-separated test tables
(regex, input, expected `y`/`n`/`c`, ...):

- `tests` — general cases
- `tests.s` — cases that must be run with `-s` (anchored, no substring search)
- `tests.i` — case-insensitive cases

Run all: `sh tryall.sh` (Unix) or `tryall.bat` (DOS/Windows). `try` options:
`-s` clears `RE_SEARCH`, `-i` sets `RE_IGNORECASE`, `-f` sets `RE_FULL`
(longest match), `-t` test mode, `-d` debug output.

## Exit codes

0 = match found, 1 = no match, 2 = syntax error or inaccessible file,
3 = internal error.
