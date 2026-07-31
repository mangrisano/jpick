#!/usr/bin/env bash
# Micro-benchmark for jpick: times representative queries over a large JSON
# document and writes the results in the customSmallerIsBetter format consumed
# by benchmark-action/github-action-benchmark (lower is better, unit = ms).
#
# Requires GNU date (date +%s%N); intended to run on the Linux CI runner.
set -euo pipefail

JPICK="${JPICK:-build/jpick}"
DIR="$(cd "$(dirname "$0")" && pwd)"
DATA="$(mktemp -d)/big.json"
N="${N:-100000}"   # number of objects in the document
RUNS="${RUNS:-10}"  # timed iterations per query

python3 "$DIR/gen.py" "$N" "$DATA"

bench() {
  # Usage: bench <jpick-args...>; prints average milliseconds per run.
  "$JPICK" "$@" "$DATA" > /dev/null  # warm-up
  local start end
  start=$(date +%s%N)
  for _ in $(seq "$RUNS"); do
    "$JPICK" "$@" "$DATA" > /dev/null
  done
  end=$(date +%s%N)
  awk -v s="$start" -v e="$end" -v r="$RUNS" 'BEGIN { printf "%.3f", (e - s) / 1e6 / r }'
}

iter_pipe=$(bench '.[] | .value')
pretty=$(bench -p '.')
index=$(bench '.[50000].name')

cat > output.json <<EOF
[
  {"name": "iterate + pipe (${N} objects)", "unit": "ms", "value": ${iter_pipe}},
  {"name": "pretty-print (${N} objects)", "unit": "ms", "value": ${pretty}},
  {"name": "index + field (${N} objects)", "unit": "ms", "value": ${index}}
]
EOF

echo "Benchmark results:"
cat output.json
