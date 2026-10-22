# CLAUDE.md

This file gives Claude Code guidance for working in this directory.

## What this is

**dgrep** is a fast Unix `egrep` clone, written in C by Jarmo Ruuth starting in
1988 (current version string `1.75` in `dgrep.c`). Petri ("Pete") Soini wrote
parts of it, notably the modified Boyer-Moore initialization in `bm.c` that
guarantees linear worst-case time, and the 2019 Linux port changes.

It was originally written for MS-DOS / Turbo C and later ported to Microsoft C,
OS/2, BSD 4.3 (gcc), QNX, Windows NT and Linux. Today only Windows (MSVC)
and POSIX systems are supported; the code for the older platforms has been
removed. The coding style (`REG1`/`REG2` register macros, tab indentation,
banner comments before each function) is kept; match the surrounding code
and don't reformat it. The current plan is to make dgrep multithreaded.

Some files (e.g. `dfatree.c`) contain non-UTF-8 bytes in comments; edit
them with tools that keep the bytes as they are.

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
| `clib.c` | Buffer allocation (`MAXBUF` + 1 bytes), `d_free`, `error` |
| `getopt.c` / `getopt.h` | Portable `d_getopt` (prefixed so it never clashes with libc), used on all platforms |
| `system.h` | Platform detection and configuration, ASCII-only `IS_LOWER`/`TO_UPPER`/`CHCASE` macros |
| `try.c` | Regex test driver, adapted from Henry Spencer's regexp test program |
| `regress.sh` | Regression tests for the whole program (see Testing) |
| `innerloo.*`, `stat`, `*.opt` | Old notes: assembly listings of the inner loops, timing statistics |

State for multithreading: options and the search setup (`bm_t`, `regmust`,
`expr`) are globals that are only read after `main` sets them up. Everything
that changes while grepping a file is in a `grep_t` (`dgrep.c`): its own
`regbuf_t` (the lazy DFA writes to it while matching), input buffer, line
counters, context state and output buffer (`out_mem`/`out_flush`). Compiling
an expression (`dfatree.c`) is not reentrant; do it on one thread.

The code has no dependencies outside this directory (the old `..\lib` is not
used). Case conversion is ASCII-only on purpose, so bytes of UTF-8
characters are never case folded; don't reintroduce `<ctype.h>` calls on
values that may be the BOL/EOL/REM pseudo characters (>= 256).

## Configuration (see `system.h`)

The platform is detected automatically: `_WIN32` selects Windows (`\r\n` line
ends, `\` path separator), everything else is POSIX (`UNIX` is defined
internally). Optional defines:

- `TEST` — debug dumps, enables `-D`
- `NOFAST` — DFA transition tables hold state indexes instead of pointers
- `MAXBUF=n` — input buffer size (default 24 MB); the tests use 256 to
  exercise buffer boundaries

## Building

- **Linux / POSIX**: `make` (uses `cc`), `make test`
- **Windows** (Developer Command Prompt for Visual Studio):
  `nmake -f makefile.win`, `nmake -f makefile.win test`. Links the MSVC
  `setargv.obj` for wildcard expansion of file names.

Both build `dgrep` and `try`; `test` also builds `dgrep-smallbuf`
(`-DMAXBUF=256`) and runs `regress.sh`. `bm.c` is the Boyer-Moore used on
all platforms; `bm_d012.c` is an alternative with the same interface.

## Testing

`bash regress.sh dgrep try dgrep-smallbuf` (Git Bash on Windows). It runs:

- named cases for options, exit codes and previously fixed bugs
- a comparison with GNU `grep -E` when available (normalizing the `--`
  separator and `n-` context line prefix)
- a comparison of `dgrep-smallbuf` with `dgrep` over many option/pattern
  combinations, to test buffer boundaries
- the `try` regex tables: `tests` (general), `tests.s` (run with `-s`,
  anchored, no substring search), `tests.i` (case-insensitive)

`try` options: `-s` clears `RE_SEARCH`, `-i` sets `RE_IGNORECASE`, `-f` sets
`RE_FULL` (longest match), `-t` test mode, `-d` debug output.

Run the tests after every change; the exit status is the number of failures.

## Benchmark

`bash bench.sh dgrep` (`make bench`) generates test data (a 200 MB file and
1000 small files) in `$BENCH_DIR` and times dgrep, GNU grep, ripgrep and
`cat` (read cost only). The last column checks that dgrep and grep find
the same number of lines. Output goes to a file, not `/dev/null`, because
GNU grep stops at the first match when writing to `/dev/null`. In WSL keep
`BENCH_DIR` on the Linux filesystem, not under `/mnt/c`.

## Exit codes

0 = match found, 1 = no match, 2 = syntax error or inaccessible file,
3 = internal error.
