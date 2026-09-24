#!/usr/bin/env bash
#
# Measure the scaling programs for ONE checkout and emit a machine-readable report.
#
# Every program is described by two deterministic numbers:
#
#   footprint  The sum of every ALLOC section, i.e. what the loaded image occupies.
#   ir         Instructions retired, as counted by callgrind. Wall clock is
#              deliberately not used: it varies and drifts on a shared CI runner,
#              whereas instruction counts are exact and reproducible across machines.
#
# The output is a TSV so that compare-scaling.py can diff two checkouts (base
# against head) without having to parse prose.
#
# Usage: report.sh <output.tsv> [sizes...]

set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEFAULT_SIZES=(2 32 256 1024)

# Written in place of a number when a program does not build or does not measure.
# compare-scaling.py reads it back and says so in the PR comment.
FAILED=failed

# Fall back to the enclosing checkout, so the script also works when run by hand.
: "${REACTOR_UC_PATH:="$(cd "$HERE/../.." && pwd)"}"
export REACTOR_UC_PATH

ULFC="$REACTOR_UC_PATH/ulf/bin/ulfc-dev"

usage() {
  echo "usage: report.sh <output.tsv> [sizes...]" >&2
  exit 1
}

# Compile Scaling<n>.ulf into $work/bin/Scaling<n>. On failure, print the tail of
# the build log: a silent failure here is indistinguishable from a bad measurement.
# The caller decides what to do about it -- the run continues either way.
build() {
  local n="$1"
  local log="$work/build_$n.log"

  if ! ( cd "$work" && "$ULFC" -o "$work" "Scaling$n.ulf" ) > "$log" 2>&1; then
    echo "BUILD FAILED for N=$n, last 40 lines of the log:" >&2
    tail -40 "$log" >&2
    return 1
  fi
}

# Instructions retired, taken from the summary callgrind prints when it exits:
#     ==12345== I   refs:      1,234,567
instructions_retired() {
  local n="$1"
  local bin="$2"

  valgrind --tool=callgrind --callgrind-out-file="$work/cg_$n" "$bin" 2>&1 >/dev/null \
    | awk '/refs:/ { gsub(",", "", $NF); print $NF }'  # 1,234,567 -> 1234567
}

# First argument is where the report goes. 
[[ $# -ge 1 ]] || usage
out="$1"
shift
# reaction counts to measure, defaulting to DEFAULT_SIZES when none are named.
sizes=("$@")
[[ ${#sizes[@]} -gt 0 ]] || sizes=("${DEFAULT_SIZES[@]}")

# One scratch directory for the whole run: the generated .lf sources, the build
# logs, the compiled binaries and the callgrind dumps all live here until exit.
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

python3 "$HERE/gen.py" "$work" "${sizes[@]}" >/dev/null

failures=0

echo -e "program\treactions\tfootprint\tir" > "$out"
for n in "${sizes[@]}"; do
  bin="$work/bin/Scaling$n"

  # A size that stops building is the most useful thing this report can say, and
  # it can only say it by finishing the run and writing a row. So a failure is
  # recorded and the loop moves on, rather than aborting and leaving the
  # comparison with nothing to compare.
  if build "$n"; then
    # The build can succeed and the measurement still fail -- a missing readelf,
    # a valgrind that will not run here. Both end up as $FAILED in the report, so
    # each one says on stderr which of the two it was.
    if ! footprint="$(python3 "$HERE/footprint.py" "$bin")"; then
      echo "MEASUREMENT FAILED for N=$n: footprint.py could not read $bin" >&2
      footprint="$FAILED"
    fi
    if ! ir="$(instructions_retired "$n" "$bin")" || [[ -z "$ir" ]]; then
      echo "MEASUREMENT FAILED for N=$n: callgrind printed no instruction count" >&2
      ir="$FAILED"
    fi
  else
    footprint="$FAILED"
    ir="$FAILED"
  fi

  if [[ "$footprint" == "$FAILED" && "$ir" == "$FAILED" ]]; then
    failures=$((failures + 1))
  fi

  echo -e "Scaling$n\t$n\t$footprint\t$ir" >> "$out"
done

if [[ $failures -gt 0 ]]; then
  echo "$failures of ${#sizes[@]} programs produced no numbers; recorded as '$FAILED'" >&2
fi

echo "wrote $out" >&2
cat "$out" >&2
