#!/usr/bin/env bash
######################################################################
#
#	BENCH.SH
#
# Performance benchmark for dgrep. Runs in Linux/POSIX shells and in
# Git Bash on Windows. Compares dgrep to GNU grep and ripgrep (if
# installed) and to cat, which shows the cost of just reading the
# data.
#
# Usage: bash bench.sh [dgrep]
#
# Environment:
#	BENCH_DIR	directory for the generated test data, default
#			$TMPDIR/dgrep-bench (keep it on a local disk; in WSL
#			not under /mnt/c)
#	BENCH_MB	size of the big file in MB, default 200
#	BENCH_RUNS	runs per measurement, the best is shown, default 3
#	BENCH_COLD	if set, drop the file cache before each run
#			(Linux only, needs sudo without password)
#	BENCH_ONLY	run only workloads whose name contains this text
#
# Times are wall clock milliseconds. MB/s is for dgrep.
#
######################################################################

# use the standard tools, not other versions earlier in PATH
PATH=/usr/bin:/bin:$PATH

abspath() {
	case "$1" in
		/*)	echo "$1" ;;
		*)	echo "$PWD/$1" ;;
	esac
}

D=$(abspath "${1:-./dgrep}")
DIR=${BENCH_DIR:-${TMPDIR:-/tmp}/dgrep-bench}
MB=${BENCH_MB:-200}
RUNS=${BENCH_RUNS:-3}

if [ ! -x "$D" ]; then
	echo "bench.sh: can't execute $D" >&2
	exit 1
fi

mkdir -p "$DIR" && cd "$DIR" || exit 1

######################################################################
# Test data
#
# base.txt is about 10 MB of English-like text with some special words
# at known rates:
#	alpha123x, beta45y	1% of words (alternation tests)
#	quick			0.5% of words
#	Ruuthian		1 line in 100000 (rare literal)
# big.txt is base.txt repeated to BENCH_MB MB and files/ contains 1000
# files of about 100 kB, split from ten copies of base.txt.

make_data() {
	echo "Generating test data in $DIR ..." >&2
	awk 'BEGIN {
		srand(1)
		nw = split("the of and to in is that for it as with was on be " \
			"by this are from at or an have not which but all were " \
			"when we there can more if will one about some out has " \
			"would them other into than its only time could new these " \
			"two may first then do any like my now over such our man " \
			"me even most made after also did many before must " \
			"through back years where much your way well down should " \
			"because each just those people how too little state good " \
			"very make world still own see men work long get here " \
			"between both life being under never day same another " \
			"know while last might us great old year off come since " \
			"against go came right used take three", W, " ")
		for (i = 0; i < 200000; i++) {
			n = 4 + int(rand() * 10)
			line = ""
			for (j = 0; j < n; j++) {
				r = rand()
				if (r < 0.01)
					w = (rand() < 0.5 ? "alpha" : "beta") \
					    int(rand() * 1000) (rand() < 0.5 ? "x" : "y")
				else if (r < 0.015)
					w = "quick"
				else
					w = W[1 + int(rand() * nw)]
				line = line (j ? " " : "") w
			}
			if (i % 100000 == 50000)
				line = line " Ruuthian"
			print line
		}
	}' > base.txt
	: > big.txt
	while [ $(wc -c < big.txt) -lt $((MB * 1024 * 1024)) ]; do
		cat base.txt >> big.txt
	done
	rm -rf files && mkdir files
	for i in 1 2 3 4 5 6 7 8 9 10; do cat base.txt; done > ten.txt
	split -l 2000 -a 4 -d ten.txt files/f
	rm -f ten.txt
	echo "$MB" > data.version
}

if [ "$(cat data.version 2>/dev/null)" != "$MB" ]; then
	make_data
fi

BIGMB=$(( $(wc -c < big.txt) / 1024 / 1024 ))
FILES=$(echo files/f*)
NFILES=$(echo $FILES | wc -w)
FILESMB=$(( $(cat files/f* | wc -c) / 1024 / 1024 ))

######################################################################
# Timing

drop_caches() {
	sync
	echo 3 | sudo -n tee /proc/sys/vm/drop_caches > /dev/null 2>&1
}

# best_ms command...
#	Runs command RUNS times and prints the best time in milliseconds.
best_ms() {
	local best= t0 t1 t i

	for ((i = 0; i < RUNS; i++)); do
		[ -n "$BENCH_COLD" ] && drop_caches
		# not to /dev/null: GNU grep then stops at the first match
		t0=$(date +%s%N)
		"$@" > "${OUT:-out.tmp}" 2>&1
		t1=$(date +%s%N)
		t=$(( (t1 - t0) / 1000000 ))
		if [ -z "$best" ] || [ "$t" -lt "$best" ]; then
			best=$t
		fi
	done
	echo "$best"
}

# nmatches command...
#	Prints the number of matching lines: the sum of the counts with
#	-c (with or without file name prefix), otherwise the line count.
nmatches() {
	case " $* " in
		*" -c "*)	"$@" 2>/dev/null | tr -d '\r' | \
					awk -F: '{ s += $NF } END { print s + 0 }' ;;
		*)		"$@" 2>/dev/null | wc -l ;;
	esac
}

GREP=
grep --version 2>/dev/null | grep -q GNU && GREP=grep
RG=$(command -v rg 2>/dev/null)

# warm the file cache (unless measuring cold)
cat big.txt $FILES > /dev/null

######################################################################
# Workloads: name ; dgrep/grep options ; pattern ; target [; dgrep only options]
#
#	literal-*	Boyer-Moore search
#	regmust-*	Boyer-Moore for the must-match string, then dfa
#	must1-*		one character must-match string (memchr), then dfa;
#			dgrep falls back to plain dfa if it is too common
#	dfa-*		dfa only, no must-match string
#	dfa-states	needs about 256 dfa states, more than the dfa
#			cache (NTRAN) holds
#	print-*		output of many lines (to a file)
#	files-*		1000 files, dgrep with the default threads;
#			files-*-j1 with one thread (rg -j1 is in its own
#			column)

WORKLOADS="
literal-rare;-c;Ruuthian;big
literal-common;-c;quick;big
literal-ignorecase;-c -i;ruuthian;big
regmust-simple;-c;qu[a-z]*k;big
regmust-alt;-c;(alpha|beta)[0-9]+x;big
regmust-common;-c;th.e;big
must1-rare;-c;[0-9][0-9]x;big
must1-common;-c;[a-z]k[a-z];big
must1-every-line;-c;[st][a-z]*t;big
dfa-class;-c;[0-9][0-9][xy];big
dfa-alt;-c;(alpha|beta|gamma)[0-9];big
dfa-states;-c;a.......b;big
line-numbers;-c -n;alpha[0-9]+x;big
print-lines;-n;alpha[0-9]+x;big
files-literal;-c;Ruuthian;files
files-literal-j1;-c;Ruuthian;files;-j1
files-regex;-c;(alpha|beta)[0-9]+x;files
files-regex-j1;-c;(alpha|beta)[0-9]+x;files;-j1
files-dfa;-c;(alpha|beta|gamma)[0-9];files
files-dfa-j1;-c;(alpha|beta|gamma)[0-9];files;-j1
"

printf '\ndgrep benchmark on %s, %s CPUs\n' "$(uname -s)" "$(nproc 2>/dev/null || echo ?)"
printf 'big.txt %d MB, files/ %d files %d MB, best of %d runs%s\n\n' \
	"$BIGMB" "$NFILES" "$FILESMB" "$RUNS" "${BENCH_COLD:+, cold cache}"

printf '%-20s %9s %8s %9s %9s %9s\n' "workload" "dgrep ms" "MB/s" "grep ms" "rg ms" "rg -j1 ms"
printf '%-20s %9s %8s %9s %9s %9s\n' "--------" "--------" "----" "-------" "-----" "---------"

# cat output can go to /dev/null, only reading is measured
t=$(OUT=/dev/null best_ms cat big.txt)
printf '%-20s %9s %8s\n' "cat big.txt" "$t" "$(( t > 0 ? BIGMB * 1000 / t : 0 ))"
t=$(OUT=/dev/null best_ms cat $FILES)
printf '%-20s %9s %8s\n' "cat files" "$t" "$(( t > 0 ? FILESMB * 1000 / t : 0 ))"

echo "$WORKLOADS" | while IFS=';' read -r name opts pat target dopts; do
	[ -z "$name" ] && continue
	case "$name" in *"$BENCH_ONLY"*) ;; *) continue ;; esac
	if [ "$target" = big ]; then
		args="big.txt"
		size=$BIGMB
	else
		args=$FILES
		size=$FILESMB
	fi
	td=$(best_ms "$D" $dopts $opts "$pat" $args)
	tg=-
	check=
	if [ -n "$GREP" ]; then
		tg=$(best_ms grep -E $opts "$pat" $args)
		# check that dgrep finds the same lines as grep
		if [ "$(nmatches "$D" $dopts $opts "$pat" $args)" = \
		     "$(nmatches grep -E $opts "$pat" $args)" ]; then
			check=ok
		else
			check=DIFFERENT
		fi
	fi
	tr=-
	tr1=-
	if [ -n "$RG" ]; then
		tr=$(best_ms "$RG" --no-config $opts "$pat" $args)
		tr1=$(best_ms "$RG" --no-config -j1 $opts "$pat" $args)
	fi
	printf '%-20s %9s %8s %9s %9s %9s  %s\n' "$name" "$td" \
		"$(( td > 0 ? size * 1000 / td : 0 ))" "$tg" "$tr" "$tr1" "$check"
done
