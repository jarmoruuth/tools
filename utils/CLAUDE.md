# CLAUDE.md

This file gives Claude Code guidance for working in this directory.

## What this is

Small command line tools written by Jarmo Ruuth (rj originally by Petri
Soini) in the MS-DOS days, when Unix tools were not available. The `j`
suffix avoids name clashes with the real Unix tools.

| File | Tool | Status |
|------|------|--------|
| `lj.c` | `ls` clone | **In use.** Rewritten 2026 for Windows and POSIX |
| `rj.c` | `rm` clone | **In use.** Rewritten 2026 for Windows and POSIX |
| `calcj.c` | interactive C expression calculator | **In use.** 64-bit ints (2025), portable (2026) |
| `pdir.c` / `pdir.h` | portable directory/file/console layer used by the three above | |
| `lcj.c`, `mj.c`, `teej.c`, `touchj.c` | line count, mv, tee, touch | Old, unmaintained, not built. They need the DOS-era `../lib` (filelist, getopt, conio) |

The coding style is the original one: tab indentation, banner comments
before each function, `mode` struct for options. Match it.

## Build and test

```
make                         # Linux/POSIX: lj, rj, calcj
make test                    # runs test.sh
nmake -f makefile.win        # Windows, from a VS Developer Command Prompt
nmake -f makefile.win test   # needs Git Bash on PATH
```

`test.sh lj rj` runs in Linux and in Windows Git Bash. It creates a temp
dir with a 5 GB file (length only), 3000 files, UTF-8 names, a path over
MAX_PATH (needs python), a junction/symlink, and hidden/read-only files.
Some checks are Windows-only. All must pass on both platforms.

Only `../lib/c.h` is still used from `../lib` (by calcj). Do not
reintroduce dependencies on `../lib`; put portable code in `pdir.c`.

## pdir layer

- All names are UTF-8. In Windows `pd_init()` gets argv via
  `CommandLineToArgvW`, sets the console output code page to UTF-8
  (restored at exit and on ^C) and sets stdout/stderr to binary (LF).
- Windows uses the wide API (`FindFirstFileExW` etc.). `wpath()` adds the
  `\\?\` prefix when the *absolute* path is near MAX_PATH, so any path
  length works.
- `pd_entry_t.is_link` is true only for symlinks and junctions; other
  reparse points (OneDrive files) are normal files. Recursion never
  follows links.
- `hidden` means Windows hidden/system attribute, or a dot name in POSIX.
- `pd_match`: `*` and `?` only, `*.*` matches everything (DOS style),
  case insensitive in Windows, `?` matches one UTF-8 character.
- Sizes and times are 64-bit.

## lj (ls)

Options `-adflrRsSt1`. Differences from `ls`, kept on purpose:
- `-d` lists only directories, `-f` only files (not as in ls).
- A summary line `N bytes in M files, X bytes free` unless `-1`.
- Columns even when output is piped (old behaviour); width from console,
  else `COLUMNS`, else 80. Directories get a trailing slash (native).
- Long format: Windows `dashrw` attributes, POSIX `drwxr-xr-x`; date is
  `dd.mm.yyyy`.
- Names starting with `.` are hidden in both platforms without `-a`.
- `-R` with a pattern matches the pattern in every subdirectory.
Exit: 0 found, 1 nothing found, 2 errors.

## rj (rm)

Options `-dfirs`. Differences from `rm`, kept on purpose:
- `-r PATTERN` searches the pattern in all subdirectories (like
  `find -name ... -delete`); directories are deleted only with `-d`.
- `dir/` (trailing slash) means the directory contents; `rj -rd dir/`
  removes the whole tree.
- Wildcards skip hidden files unless `-f`; `-f` also clears read-only.
- `-i` asks with a single key press, no Enter.
- Prints `rm name` / `rmdir name` for each removal unless `-s` or `-i`.
- All matches are collected first and removed at the end; directory
  contents come before the directory.
- In Unix shells (and Git Bash) patterns must be quoted, otherwise the
  shell expands them and `-r` searches only those literal names.
- The old `-m` menu option was removed (it needed DOS video code).
Exit: 0 ok, 1 usage, 2 errors (including no match).

## calcj

Reads expressions from stdin. `?` shows help with a `--More--` pager that
reads keys from the console (`pd_getkey`), so it blocks when there is no
console. Values are `double`; integers are shown as 64-bit, so integers
above 2^53 lose precision. Math errors are detected by checking the
result for NaN/inf (`check_math`); function table entries are cast to
`fun_t` and called as `fun1_t`/`fun2_t`.
