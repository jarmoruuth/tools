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
#	dgrep-smallbuf	dgrep built with a tiny I/O buffer (-DINITBUF=256);
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

# path separator in the file names dgrep prints
if [ -n "$WINDOWS" ]; then
	PSEP='\'
else
	PSEP='/'
fi

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
t "line numbers crlf"	$'3:match\n5:match x'	'printf "a\r\nb\r\nmatch\r\n\r\nmatch x\r\n" | "$D" -n match'
t "line numbers dfa"	$'3:c1\n5:c2'		'printf "a\nb\nc1\nd\nc2\n" | "$D" -n "c[0-9]"'
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

# -r: sorted, recursive, empty directories, current directory
S=$PSEP
mkdir -p tree/b/d tree/empty
printf 'match a\n' > tree/a.txt
printf 'match c\nother\n' > tree/b/c.txt
printf 'no\n' > tree/b/d/e.txt
printf 'match f\n' > tree/b/d/f.txt
printf 'match z\n' > tree/z.txt
TREE="tree${S}a.txt:match a
tree${S}b${S}c.txt:match c
tree${S}b${S}d${S}f.txt:match f
tree${S}z.txt:match z"
t "-r"			"$TREE"			'"$D" -r match tree'
t "-r current dir"	"a.txt:match a
b${S}c.txt:match c
b${S}d${S}f.txt:match f
z.txt:match z"						'(cd tree && "$D" -r match)'
t "-r -l"		"tree${S}a.txt
tree${S}b${S}c.txt
tree${S}b${S}d${S}f.txt
tree${S}z.txt"						'"$D" -r -l match tree'
t "-r -c"		"tree${S}a.txt:1
tree${S}b${S}c.txt:1
tree${S}b${S}d${S}e.txt:0
tree${S}b${S}d${S}f.txt:1
tree${S}z.txt:1"					'"$D" -r -c match tree'
t "-r file and dir"	"a.txt:apple
tree${S}a.txt:match a"					'"$D" -r "apple|match a" a.txt tree'
t "-r -j1"		"$TREE"			'"$D" -r -j1 match tree'
s "-r no match"		1	'"$D" -r -s nomatch tree'
s "-r missing dir"	2	'"$D" -r -s match tree no-such-dir'
s "-r empty dir"	1	'"$D" -r -s match tree/empty'
if [ -z "$WINDOWS" ]; then
	# symbolic links in the tree are skipped, on the command line
	# followed
	ln -s b tree/link
	ln -s a.txt tree/flink
	mkfifo tree/fifo
	t "-r skips links"	"$TREE"		'timeout 10 "$D" -r match tree'
	t "-r follows arg link"	"tree/link/c.txt:match c
tree/link/d/f.txt:match f"				'"$D" -r match tree/link'
	rm -f tree/link tree/flink tree/fifo
fi

# binary files (a NUL byte): only a message by default, -a greps as
# text, -I skips them
printf 'abc\0def\nmatch 1\nmatch 2\n' > bin1.bin
printf 'xx\0yy\0zz match\0ww\n' > bin2.bin
t "binary"		'Binary file bin1.bin matches'	'"$D" match bin1.bin'
t "binary stdin"	'Binary file (standard input) matches' '"$D" match < bin1.bin'
t "binary -a"		$'bin1.bin:match 1\nbin1.bin:match 2' '"$D" -a match bin1.bin'
t "binary -I"		'a.txt:apple'		'"$D" -I "apple|match" a.txt bin1.bin'
t "binary -c"		'bin1.bin:2'		'"$D" -c match bin1.bin'
t "binary -l"		'bin1.bin'		'"$D" -l match bin1.bin'
t "binary between NULs"	'Binary file bin2.bin matches'	'"$D" "zz m" bin2.bin'
t "binary no match"	''			'"$D" nomatch bin1.bin'
s "binary status"	0	'"$D" -s match bin1.bin'
s "binary -I status"	1	'"$D" -s -I match bin1.bin'
# NUL after the first buffer: earlier matches are printed as text
{
	echo "match early"
	seq 1 40000 | sed "s/^/filler line /"
	printf 'late\0binary\nmatch late\n'
} > binlate.bin
t "binary late"		$'binlate.bin:match early\nBinary file binlate.bin matches' \
						'"$D" match binlate.bin'
mkdir -p tree2/sub
cp bin1.bin tree2/sub/
printf 'match text\n' > tree2/text.txt
t "binary -r"		"Binary file tree2${S}sub${S}bin1.bin matches
tree2${S}text.txt:match text"				'"$D" -r match tree2'

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

	# Literal search (SSE2): matches at every position relative to
	# 16 byte blocks and line ends, and patterns with a candidate at
	# every position, which make dgrep fall back to Boyer-Moore.
	{
		for i in $(seq 0 40); do
			printf '%*sneedle%*s|\n' $i "" $(( (i * 7) % 19 )) ""
			printf '%*sNeEdLe\n' $i ""
		done
		for i in $(seq 1 30); do
			printf '%s\n' "$(printf 'a%.0s' $(seq 1 $((i * 9))))"
			printf '%sb\n' "$(printf 'a%.0s' $(seq 1 $((i * 7))))"
		done
		printf 'qQ q Q xq\n'
	} > sse.txt
	for opts in "" "-i" "-c" "-n -i" "-v -c"; do
		for pat in "needle" "dle " "ne" "NeEdLe" "e" "q" "aaaab" "aaaa" \
			   "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaab" "b" "ab" \
			   "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"; do
			grep -E $opts "$pat" < sse.txt 2>&1 > exp.out
			"$D" $opts "$pat" < sse.txt 2>&1 | tr -d '\r' > act.out
			if cmp -s exp.out act.out; then
				pass
			else
				fail "GNU grep sse.txt: dgrep $opts '$pat'" \
					"$(diff exp.out act.out | head -6 | tr '\n' '|')"
			fi
		done
	done

	# Random periodic patterns from a two letter alphabet, which
	# are hard cases for Boyer-Moore jump tables (the old Knuth
	# delta2 in bm_d012.c failed 12% of these)
	awk 'BEGIN { srand(7); for (i = 0; i < 3000; i++) {
		n = 1 + int(rand() * 60); s = ""
		for (j = 0; j < n; j++) s = s (rand() < 0.7 ? "a" : "b")
		print s } }' > ab.txt
	awk 'BEGIN { srand(11); for (i = 0; i < 200; i++) {
		n = 2 + int(rand() * 22); s = ""
		for (j = 0; j < n; j++) s = s (rand() < 0.75 ? "a" : "b")
		print s } }' > abpat.txt
	nbad=0
	while read -r pat; do
		for opts in "" "-i"; do
			if [ "$(grep -c -F $opts "$pat" < ab.txt)" != \
			     "$("$D" -c -x $opts "$pat" < ab.txt | tr -d '\r')" ]; then
				nbad=$((nbad + 1))
				[ $nbad -le 3 ] && echo "      dgrep -x $opts '$pat'"
			fi
		done
	done < abpat.txt
	if [ $nbad -eq 0 ]; then
		pass
	else
		fail "periodic patterns: $nbad of 400 counts differ from GNU grep"
	fi
fi

######################################################################
# Buffer boundaries: dgrep built with a tiny buffer must give the
# same output as the normal build. long.txt has lines longer than the
# tiny buffer, so its buffer must grow.

A3000=$(printf 'a%.0s' $(seq 1 3000))
{
	echo "line 1 short match"
	echo "line 2 ${A3000} match at the end"
	echo "line 3 short"
	echo "match ${A3000}${A3000}${A3000} line 4"
	echo "line 5 short match"
} > long.txt
t "long lines"		"5"	'"$D" -c "line" < long.txt'

if [ -n "$SMALL" ]; then
	for opts in "" "-n" "-i" "-v" "-c" "-l" "-n -i" "-1" "-A2" "-B3" \
		    "-n -2" "-n -B2" "-n -v -A1" "-i -B1" "-d -n" "-x"; do
		for pat in "match" "m.tch|other" "^line 1[0-9]" "newline$"; do
			for file in big.txt long.txt; do
				"$D" $opts "$pat" $file 2>&1 | tr -d '\r' > exp.out
				"$SMALL" $opts "$pat" $file 2>&1 | tr -d '\r' > act.out
				if cmp -s exp.out act.out; then
					pass
				else
					fail "small buffer: dgrep $opts '$pat' $file" \
						"$(diff exp.out act.out | head -6 | tr '\n' '|')"
				fi
			done
		done
	done
fi

######################################################################
# Parallel grepping: the output with many threads must be the same as
# with one thread, in the order of the files. The files have different
# sizes, so they finish in a different order than they start.

mkdir -p par
for i in $(seq 1 200); do
	n=$(( (i * 37) % 23 ))
	if [ $((i % 3)) -eq 0 ]; then
		head -c $((n * 2000)) big.txt
	else
		head -c $((n * 20)) big.txt
	fi > par/f$i.txt
done
PAR=$(for i in $(seq 1 200); do printf 'par/f%d.txt ' $i; done)
for opts in "" "-n -1" "-c" "-l" "-b -A2" "-z -B1" "-v -c" "-n -i"; do
	"$D" -j1 $opts "m.tch|line 1" $PAR 2>&1 | tr -d '\r' > exp.out
	for run in 1 2 3; do
		"$D" -j8 $opts "m.tch|line 1" $PAR 2>&1 | tr -d '\r' > act.out
		if cmp -s exp.out act.out; then
			pass
		else
			fail "parallel: dgrep -j8 $opts (run $run)" \
				"$(diff exp.out act.out | head -6 | tr '\n' '|')"
		fi
	done
done
s "parallel status"	0	'"$D" -j8 -s "line 77 " $PAR'
s "parallel no match"	1	'"$D" -j8 -s Ruuthian $PAR'
s "parallel missing"	2	'"$D" -j8 -s match no-such-file.txt $PAR'
s "bad -j"		2	'"$D" -j0 match a.txt'

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
