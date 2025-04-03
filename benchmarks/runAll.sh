#!/usr/bin/env bash
set -e

LFC=lfc
LFCG=${REACTOR_UC_PATH}/lfuc/bin/lfuc-dev

$LFC src/PingPongC.lf
$LFCG src/PingPongUc.lf

$LFC src/ReactionLatencyC.lf
$LFCG src/ReactionLatencyUc.lf

echo "Running benchmarks..."

ping_pong_c_result=$(bin/PingPongC | grep -E "Time: *.")
ping_pong_uc_result=$(bin/PingPongUc | grep -E "Time: *.")
latency_c_result=$(bin/ReactionLatencyC | grep -E " latency: *.")
latency_uc_result=$(bin/ReactionLatencyUc | grep -E "latency: *.")


# Create or clear the output file
output_file="benchmark_results.md"
: > "$output_file"

# Print and dump the results into the file
echo "Benchmark results after merging this PR: " >> "$output_file"
echo "<details><summary>Benchmark results</summary>" >> "$output_file"
echo "" >> "$output_file"
echo "## Performance:" >> "$output_file"
echo "" >> "$output_file"

benchmarks=("PingPongUc" "PingPongC" "ReactionLatencyUc" "ReactionLatencyC")
results=("$ping_pong_uc_result" "$ping_pong_c_result" "$latency_uc_result" "$latency_c_result")
echo $latency_uc_result >> test.md

for i in "${!benchmarks[@]}"; do
  echo "${benchmarks[$i]}:" >> "$output_file"
  echo "${results[$i]}" >> "$output_file"
  echo "" >> "$output_file"
done

echo "## Memory usage:" >> "$output_file"
for benchmark in PingPongUc PingPongC ReactionLatencyUc ReactionLatencyC; 
do
  echo "$benchmark:" >> "$output_file"
  echo "$(size -d bin/$benchmark)" >> "$output_file"
  echo "" >> "$output_file"
done

cat "$output_file"
