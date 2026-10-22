#!/usr/bin/env bash
######################################################################
#
#	REGRESS.SH
#
# Regression tests for dgrep. Runs in Linux/POSIX shells and in
# Git Bash on Windows.
#
# Usage: bash regress.sh [dgrep [try [dgrep-smallbuf]]]
#
#	dgrep		dgrep binary to test (default ./dgrep)
#	try		regex test driver; if given, runs the tests,
#			tests.s and tests.i tables like tryall.sh
#	dgrep-smallbuf	dgrep built with a tiny I/O buffer (-DMAXBUF=256);
#			if given, its output is compared to dgrep's to
#			test the buffer boundary handling
#
# Exit status is the number of failed tests.
#
######################################################################

# use the standard tools, not other versions earlier in PATH
PATH=/usr/bin:/bin:$PATH

abspath() {
	case "$1" in
		"")	echo "" ;;
		/*)	echo "$1" ;;
		*)	echo "$PWD/$1" ;;
	esac
}

D=$(abspath "${1:-./dgrep}")
TRY=$(abspath "${2:-}")
SMALL=$(abspath "${3:-}")
SRCDIR=$(cd "$(dirname "$0")" && pwd)

case "$OSTYPE" in
	msys*|cygwin*|win32*)	WINDOWS=1 ;;
	*)			WINDOWS= ;;
esac

if [ ! -x "$D" ]; then
	echo "regress.sh: can't execute $D" >&2
	exit 99
fi

W=$(mktemp -d)
trap 'cd / && rm -rf "$W" 2>/dev/null' EXIT
cd "$W" || exit 99

npass=0
nfail=0

pass() {
	npass=$((npass + 1))
}

fail() {
	nfail=$((nfail + 1))
	echo "FAIL: $1"
	shift
	for line in "$@"; do
		echo "      $line"
	done
}

# t name expected command
#	Runs command with eval in the work directory and compares its
#	output (stdout and stderr, CRs removed) to expected.
t() {
	local actual
	actual=$(eval "$3" 2>&1 < /dev/null | tr -d '\r')
	if [ "$actual" == "$2" ]; then
		pass
	else
		fail "$1" "command:  $3" "expected: $(echo "$2" | head -5 | tr '\n' '|')" \
			"actual:   $(echo "$actual" | head -5 | tr '\n' '|')"
	fi
}

# s name expected-status command
#	Runs command with eval and compares its exit status.
s() {
	local status
	eval "$3" >/dev/null 2>&1 < /dev/null
	status=$?
	if [ "$status" == "$2" ]; then
		pass
	else
		fail "$1" "command:  $3" "expected exit $2, got $status"
	fi
}

######################################################################
# Test data

printf 'apple\nAPPLE\nbanana\n' > a.txt
printf 'one match\n' > '100%sure.txt'
printf 'cherry\n' > c.txt
printf 'bzz\nabz\nxbz\ncat\n' > neg.txt
printf 'm1\nm2\nfoo\nbar\n' > ctx1.txt
printf 'a\nm1\nb\nm2\nc\n' > ctx2.txt
printf 'x\nm1\ny\nz\nw\nm2\nq\n' > ctx3.txt
printf 'Abc\nxabc\nABC\n' > bol.txt
printf 'last line without newline' > nonl.txt
printf '\xc3\x84\n' > pat_AE.txt
printf '\xc3\x8e\n' > I_circ.txt
printf 'first|second\n' > pat_or.txt
printf 'a\nm1\nb\n' > mf1.txt
printf 'x\ny\n' > mf2.txt
printf 'm2\nc\nd\ne\nm3\n' > mf3.txt

A64=$(printf 'a%.0s' $(seq 1 64))
A31=$(printf 'a%.0s' $(seq 1 31))
CLS_RE="$(printf '.%.0s' $(seq 1 64))[q]$(printf '.%.0s' $(seq 1 31))[x]"
printf '%sq%sx\n%sq%sq\n' "$A64" "$A31" "$A64" "$A31" > cls.txt

######################################################################
# Basic behaviour

t "literal"		$'apple\nbanana'	'"$D" "an|ppl" < a.txt'
t "literal 2"		'banana'		'"$D" nan < a.txt'
t "ignore case"		$'apple\nAPPLE'		'"$D" -i apple < a.txt'
t "invert"		'banana'		'"$D" -v -i apple < a.txt'
t "count"		'2'			'"$D" -c -i apple < a.txt'
t "line numbers"	$'2:APPLE\n3:banana'	'"$D" -n "A|b" < a.txt'
t "regex"		$'apple\nbanana'	'"$D" "^(ap|ba).*[el]?$" < a.txt'
t "exact"		''			'"$D" -x "a.p" < a.txt'
t "exact special"	'a.p(x)*'		'printf "a.p(x)*\nap\n" | "$D" -x "a.p(x)*"'
t "file names"		$'a.txt:apple\nc.txt:cherry'	'"$D" "apple|cherry" a.txt c.txt'
t "names only"		$'a.txt\nc.txt'		'"$D" -l "an|err" a.txt c.txt neg.txt'
t "block mode"		$'*** File a.txt:\napple\n*** File c.txt:\ncherry' \
						'"$D" -b "apple|cherry" a.txt c.txt'
t "count per file"	$'a.txt:1\nc.txt:0'	'"$D" -c apple a.txt c.txt'
t "expression file"	$'first\nsecond'	'printf "first\nthird\nsecond\n" | "$D" -f pat_or.txt'
t "no final newline"	'last line without newline'	'"$D" without < nonl.txt'
t "dfa only"		$'apple\nbanana'	'"$D" -d "an|ppl" < a.txt'

# output of several files, in order, with context separators between
# and inside files
MF="mf1.txt mf2.txt mf3.txt"
t "files context"	$'mf1.txt:a\nmf1.txt:m1\nmf1.txt:b\n----------\nmf3.txt:m2\nmf3.txt:c\n----------\nmf3.txt:e\nmf3.txt:m3' \
						'"$D" -1 m $MF'
t "files block context"	$'*** File mf1.txt:\na\nm1\nb\n*** File mf3.txt:\nm2\nc\n----------\ne\nm3' \
						'"$D" -b -1 m $MF'
t "files verbose"	$'*** File mf1.txt:\na\nm1\nb\n*** File mf2.txt:\n*** File mf3.txt:\nm2\nc\n----------\ne\nm3' \
						'"$D" -z -1 m $MF'
t "files count"		$'mf1.txt:1\nmf2.txt:0\nmf3.txt:2'	'"$D" -c m $MF'
t "files names"		$'mf1.txt\nmf3.txt'	'"$D" -l m $MF'

s "status match"	0	'"$D" apple a.txt'
s "status no match"	1	'"$D" kiwi a.txt'
s "status syntax error"	2	'"$D" "a(b" a.txt'
s "status missing file"	2	'"$D" apple a.txt no-such-file.txt'

######################################################################
# Fixed bugs

# -i with a one character pattern (bm_d012.c memchr shortcut)
t "ignore case 1 char"	$'apple\nAPPLE\nbanana'	'"$D" -i a < a.txt'
t "ignore case 1 char -x" $'apple\nAPPLE\nbanana' '"$D" -i -x a < a.txt'

# file name was used as printf format
t "percent in file name" '100%sure.txt:one match' '"$D" match "100%sure.txt" a.txt'
t "percent in count"	$'100%sure.txt:1\na.txt:0' '"$D" -c match "100%sure.txt" a.txt'

# DGREP environment variable dropped the last argument
t "DGREP env"		$'a.txt:apple\na.txt:APPLE'	'DGREP=-i "$D" apple a.txt'
t "DGREP env 2 options"	$'a.txt:1:apple\na.txt:2:APPLE'	'DGREP="-i -n" "$D" apple a.txt'

# -p prepended current directory also to absolute paths
t "-p absolute path"	"$("$D" apple "$W/a.txt" 2>&1 | tr -d '\r')" \
						'"$D" -p apple "$W/a.txt"'
if [ -n "$WINDOWS" ]; then
	PSEP='\'
else
	PSEP='/'
fi
out=$("$D" -p apple a.txt 2>&1 | tr -d '\r')
case "$out" in
	*"${PSEP}a.txt:apple")	pass ;;
	*)			fail "-p relative path" "got: $out" ;;
esac

# negated class matched beginning of line
t "negated class and ^"	$'abz\ncat'		'"$D" "[^x]b|^c" < neg.txt'

# context lines were printed twice and separator between adjacent groups
t "context -A overlap"	$'m1\nm2\nfoo'		'"$D" -A1 m < ctx1.txt'
t "context -B adjacent"	$'a\nm1\nb\nm2'		'"$D" -B1 m < ctx2.txt'
t "context gap"		$'x\nm1\ny\n----------\nw\nm2\nq'	'"$D" -1 m < ctx3.txt'
t "context numbers"	$'1:a\n2:m1\n3:b\n4:m2\n5:c'	'"$D" -n -1 m < ctx2.txt'
t "context -v"		$'foo\nbar'	'"$D" -v -A1 m < ctx1.txt'

# -x with empty pattern looped forever
t "exact empty"		$'apple\nAPPLE\nbanana'	'timeout 10 "$D" -x "" < a.txt'
t "empty"		$'apple\nAPPLE\nbanana'	'timeout 10 "$D" "" < a.txt'

# with -i, character class index 97..122 was uppercased
t "many classes -i"	"1:${A64}q${A31}x"	'"$D" -n -i "$CLS_RE" < cls.txt'
t "many classes"	"1:${A64}q${A31}x"	'"$D" -n "$CLS_RE" < cls.txt'

# beginning of line with -i
t "^ with -i"		$'Abc\nABC'		'"$D" -i "^a" < bol.txt'
t "^ with -i 2"		$'Abc\nxabc\nABC'	'"$D" -i "^a|x" < bol.txt'

# IBM PC (CP437) case folding matched UTF-8 bytes (C3 84 vs C3 8E)
t "8-bit case folding"	''			'"$D" -i -f pat_AE.txt < I_circ.txt'

######################################################################
# Compare to GNU grep, if available. Output formats differ only in
# context lines: GNU grep uses "--" as a separator and "n-" before
# context line numbers.

for i in $(seq 1 300); do
	case $((i % 7)) in
		0)	echo "line $i match here" ;;
		3)	echo "line $i MATCH again" ;;
		*)	echo "line $i other text" ;;
	esac
done > big.txt
printf 'last match without newline' >> big.txt

if grep --version 2>/dev/null | grep -q GNU; then
	for opts in "" "-n" "-c" "-i" "-v" "-n -1" "-n -2" "-n -B3" "-n -A2" \
		    "-A5" "-n -v -1" "-n -i -2" "-n -B1 -A4"; do
		# "e.t" and "[st][a-z]*t" have a common one character
		# must-match string, dgrep switches to plain dfa for them
		for pat in "match" "m.tch|other" "^line 1[0-9]" "newline$" "x" \
			   "(ma|o)[t]+.*e" "e.t" "[st][a-z]*t" "[0-9]2 m"; do
			grep -E $opts "$pat" < big.txt 2>&1 | \
				sed 's/^--$/----------/; s/^\([0-9][0-9]*\)[-:]/\1:/' > exp.out
			"$D" $opts "$pat" < big.txt 2>&1 | tr -d '\r' > act.out
			if cmp -s exp.out act.out; then
				pass
			else
				fail "GNU grep: dgrep $opts '$pat'" \
					"$(diff exp.out act.out | head -6 | tr '\n' '|')"
			fi
		done
	done
fi

######################################################################
# Buffer boundaries: dgrep built with a tiny buffer must give the
# same output as the normal build.

if [ -n "$SMALL" ]; then
	for opts in "" "-n" "-i" "-v" "-c" "-l" "-n -i" "-1" "-A2" "-B3" \
		    "-n -2" "-n -B2" "-n -v -A1" "-i -B1" "-d -n" "-x"; do
		for pat in "match" "m.tch|other" "^line 1[0-9]" "newline$"; do
			"$D" $opts "$pat" big.txt 2>&1 | tr -d '\r' > exp.out
			"$SMALL" $opts "$pat" big.txt 2>&1 | tr -d '\r' > act.out
			if cmp -s exp.out act.out; then
				pass
			else
				fail "small buffer: dgrep $opts '$pat' big.txt" \
					"$(diff exp.out act.out | head -6 | tr '\n' '|')"
			fi
		done
	done
fi

######################################################################
# Regular expression engine tests (see tryall.sh)

if [ -n "$TRY" ]; then
	for args in "-ts tests.s" "-tsf tests.s" "-tsi tests.s" "-tsfi tests.s" \
		    "-t tests" "-tf tests" "-tfi tests" "-ti tests" \
		    "-tfi tests.i" "-ti tests.i"; do
		set -- $args
		if "$TRY" $1 < "$SRCDIR/$2" > try.out 2>&1; then
			pass
		else
			fail "try $1 < $2" "$(grep -iv '^[0-9]*:' try.out | head -3 | tr '\n' '|')"
		fi
	done
fi

######################################################################

echo "$npass passed, $nfail failed"
exit $nfail
