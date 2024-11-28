set -e

LFC=lfc
LFCG=${REACTOR_UC_PATH}/lfc/bin/lfc-dev

$LFC src/PingPongC.lf
$LFCG src/PingPongUc.lf

$LFC src/ReactionLatencyC.lf
$LFCG src/ReactionLatencyUc.lf

echo "Running benchmarks..."

ping_pong_c_result=$(bin/PingPongC | grep -E "Time: *.")
ping_pong_uc_result=$(bin/PingPongUc | grep -E "Time: *.")
latency_c_result=$(bin/ReactionLatencyC | grep -E " latency: *.")
latency_uc_result=$(bin/ReactionLatencyUc | grep -E " latency: *.")

echo "PingPongUc:\n $ping_pong_uc_result"
echo "PingPongC:\n $ping_pong_c_result"
echo "ReactionLatencyUc:\n $latency_uc_result"
echo "ReactionLatencyC:\n $latency_c_result"
