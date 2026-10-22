#!/bin/bash
######################################################################
#
#	Regression tests for lj and rj. Runs in Linux and in Windows
#	Git Bash. Creates a temporary directory, including a 5 GB file
#	(not written, only its length is set) and a path longer than
#	MAX_PATH. Needs python or python3 for the long path.
#
#	Usage: bash test.sh path/to/lj path/to/rj
#
######################################################################

LJ=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
RJ=$(cd "$(dirname "$2")" && pwd)/$(basename "$2")
# avoid old custom tools earlier in PATH
PATH=/usr/bin:/bin:$PATH
export COLUMNS=80

case "$(uname -s)" in
	MINGW*|MSYS*|CYGWIN*|Windows_NT) WIN=1; S='\';;
	*) WIN=0; S='/';;
esac
if command -v python3 >/dev/null && python3 -c 1 2>/dev/null; then
	PY=python3
else
	PY=python
fi

T=$(mktemp -d)
trap 'cd /; rm -rf "$T"' EXIT
cd "$T" || exit 2

failed=0
passed=0

check() {
	if [ "$2" = "$3" ]; then
		passed=$((passed + 1))
	else
		failed=$((failed + 1))
		echo "FAIL: $1"
		echo "  expected: $(printf '%s' "$2" | od -c | head -5)"
		echo "  got:      $(printf '%s' "$3" | od -c | head -5)"
	fi
}

######################################################################
# lj

mkdir -p ls/sub/deep
cd ls
: > a.c; : > b.c; : > B.txt; : > .hidden
: > sub/x.c; : > sub/deep/y.c

check "lj -1" "a.c
b.c
B.txt
sub$S" "$("$LJ" -1)"
check "lj -1a" ".hidden
a.c
b.c
B.txt
sub$S" "$("$LJ" -1a)"
check "lj -1d" "sub$S" "$("$LJ" -1d)"
check "lj -1f" "a.c
b.c
B.txt" "$("$LJ" -1f)"
check "lj -1r" "sub$S
B.txt
b.c
a.c" "$("$LJ" -1r)"
check "lj pattern" "a.c
b.c" "$("$LJ" -1 '*.c')"
check "lj file args" "a.c
B.txt" "$("$LJ" -1 B.txt a.c)"
check "lj dir arg" "deep$S
x.c" "$("$LJ" -1 sub)"
check "lj -R pattern" ".:
a.c
b.c

sub:
x.c

sub${S}deep:
y.c" "$("$LJ" -1R '*.c')"
"$LJ" -1 '*.none' >/dev/null
check "lj no match rc" 1 $?
"$LJ" -1 nothere >/dev/null 2>&1
check "lj not found rc" 2 $?
if [ $WIN = 1 ]; then
	check "lj case insensitive" "a.c
b.c" "$("$LJ" -1 '*.C')"
fi

head -c 12345 /dev/zero > a.c
check "lj -1S" "a.c
b.c
B.txt
sub$S" "$("$LJ" -1S)"
touch -d '2001-02-03 04:05:06' b.c
check "lj -1t" "b.c" "$("$LJ" -1t | tail -1)"
check "lj -l" "0 3.02.2001 4:05 b.c" \
	"$("$LJ" -l b.c | head -1 | awk '{print $2, $3, $4, $5}')"
check "lj summary" "12,345 bytes in 4 files" \
	"$("$LJ" | tail -1 | sed 's/, .* bytes free$//')"
cd ..

# columns are sorted down, no trailing spaces
mkdir cols
for i in 01 02 03 04 05 06 07 08 09 10 11 12; do : > cols/f$i; done
check "lj columns" "f01 f03 f05 f07 f09 f11
f02 f04 f06 f08 f10 f12" "$(COLUMNS=30 "$LJ" cols | head -2)"
check "lj -s columns" " 0 f01  0 f04  0 f07  0 f10
 0 f02  0 f05  0 f08  0 f11" "$(COLUMNS=40 "$LJ" -s cols | head -2)"

# 64-bit sizes and disk free
truncate -s 5G big.bin
check "lj 5 GB file" "5,368,709,120" \
	"$("$LJ" -l big.bin | head -1 | awk '{print $2}')"
rm -f big.bin
free=$("$LJ" | tail -1 | sed 's/.*, \(.*\) bytes free/\1/; s/,//g')
dffree=$(df -B1 . | awk 'NR==2 {print $4}')
check "lj disk free" 1 "$(awk -v a="$free" -v b="$dffree" \
	'BEGIN { d = a - b; if (d < 0) d = -d; print (d < b / 100) ? 1 : 0 }')"

# many files
mkdir many
(cd many && for i in $(seq 1 3000); do : > "file_number_$i.txt"; done)
check "lj many files" 3000 "$("$LJ" -1 many | wc -l | tr -d ' ')"
check "lj many summary" "0 bytes in 3000 files" \
	"$("$LJ" many | tail -1 | sed 's/, .* bytes free$//')"

# UTF-8 names
mkdir utf
: > "utf/äö_日本.txt"
: > "utf/abcdef.txt"
check "lj utf-8" "abcdef.txt
äö_日本.txt" "$("$LJ" -1 utf)"
check "lj utf-8 columns" "abcdef.txt äö_日本.txt" "$("$LJ" utf | head -1)"

# path longer than MAX_PATH
$PY - <<'EOF'
import os
p = os.path.abspath("long")
if os.name == "nt":
    p = "\\\\?\\" + p
for i in range(15):
    p = os.path.join(p, "directory_name_number_%02d" % i)
os.makedirs(p)
open(os.path.join(p, "deep.txt"), "w").close()
EOF
check "lj long path" "deep.txt" "$("$LJ" -1R long | tail -1)"
check "lj long path header length > 300" 1 \
	"$("$LJ" -1R long | tail -2 | head -1 | awk '{print (length($0) > 300)}')"

######################################################################
# rj

mkdir -p rm/sub/deep
cd rm
: > a.o; : > a.c; : > sub/b.o; : > sub/deep/c.o; : > sub/deep/c.c
"$RJ" -r '*.o' >/dev/null
check "rj -r pattern" "a.c
sub/deep/c.c" "$(find . -type f | sed 's|^\./||' | sort)"
"$RJ" -rd sub/ >/dev/null
check "rj -rd dir/" "a.c" "$(find . | sed 's|^\./||' | grep -v '^\.$' | sort)"

"$RJ" nothere 2>/dev/null
check "rj not found rc" 2 $?
"$RJ" -r '*.none' 2>/dev/null
check "rj no match rc" 2 $?
mkdir d
"$RJ" d 2>/dev/null
check "rj dir without -d rc" 2 $?
"$RJ" -d d >/dev/null
check "rj -d empty dir" "no" "$([ -e d ] && echo yes || echo no)"

check "rj output" "rm a.c" "$("$RJ" a.c)"
: > a.c
check "rj -s output" "" "$("$RJ" -s a.c)"

# hidden files need -f with wildcards
mkdir h
: > h/visible.o; : > h/hidden.o
if [ $WIN = 1 ]; then
	attrib +h 'h\hidden.o' >/dev/null
else
	mv h/hidden.o h/.hidden.o
fi
"$RJ" -s 'h/*.o'
check "rj skips hidden" 1 "$(ls -A h | wc -l | tr -d ' ')"
"$RJ" -sf 'h/*.o'
check "rj -f removes hidden" 0 "$(ls -A h | wc -l | tr -d ' ')"

# links and junctions are removed but not followed
mkdir -p keep tree/sub
: > keep/k.txt; : > tree/sub/t.txt
if [ $WIN = 1 ]; then
	MSYS_NO_PATHCONV=1 cmd /c 'mklink /J tree\lnk keep' >/dev/null
else
	ln -s ../keep tree/lnk
fi
"$RJ" -rdf tree/ >/dev/null
check "rj -rdf removes tree" "no" "$([ -e tree ] && echo yes || echo no)"
check "rj does not follow links" "yes" "$([ -e keep/k.txt ] && echo yes || echo no)"

# read-only files need -f in Windows
if [ $WIN = 1 ]; then
	: > ro.txt
	attrib +r ro.txt >/dev/null
	"$RJ" -s ro.txt 2>/dev/null
	check "rj read-only fails" "2 yes" "$? $([ -e ro.txt ] && echo yes || echo no)"
	"$RJ" -sf ro.txt
	check "rj -f read-only" "no" "$([ -e ro.txt ] && echo yes || echo no)"
fi
cd ..

"$RJ" -s 'many/*'
check "rj many files" 0 "$(ls -A many | wc -l | tr -d ' ')"
"$RJ" -rds utf/
check "rj utf-8" "no" "$([ -e utf ] && echo yes || echo no)"
"$RJ" -rds long/
check "rj long path" "no" "$([ -e long ] && echo yes || echo no)"

echo "$passed passed, $failed failed"
[ $failed = 0 ]
