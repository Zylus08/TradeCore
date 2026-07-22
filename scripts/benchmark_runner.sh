#!/bin/bash
set -e

# Setup paths
ROOT_DIR=$(dirname "$0")/..
BUILD_DIR="${ROOT_DIR}/build-release"
RESULTS_DIR="${ROOT_DIR}/benchmarks/results"
BENCH_BIN="${BUILD_DIR}/benchmarks/tradecore_benchmarks"

mkdir -p "$RESULTS_DIR"
cd "$ROOT_DIR"

echo "=== Building TradeCore Release ==="
cmake --preset=release
cmake --build build-release -j$(nproc)

echo "=== Running Google Benchmarks (JSON) ==="
"$BENCH_BIN" \
    --benchmark_format=json \
    --benchmark_out="${RESULTS_DIR}/allocator_baseline.json"

cp "${RESULTS_DIR}/allocator_baseline.json" "${RESULTS_DIR}/allocator_pool_vs_malloc.json"

echo "=== Running Perf Stat ==="
if command -v perf >/dev/null 2>&1; then
    perf stat -e task-clock,cycles,instructions,L1-dcache-load-misses,L1-dcache-loads,LLC-load-misses,LLC-loads,branch-misses \
        "$BENCH_BIN" > "${RESULTS_DIR}/perf_report.txt" 2>&1
    echo "perf_report.txt generated."
else
    echo "Warning: 'perf' not found. Skipping perf stat." > "${RESULTS_DIR}/perf_report.txt"
fi

echo "=== Running Cachegrind ==="
if command -v valgrind >/dev/null 2>&1; then
    valgrind --tool=cachegrind --cachegrind-out-file="${RESULTS_DIR}/cachegrind.out" \
        "$BENCH_BIN" > /dev/null 2>&1
    echo "cachegrind.out generated."
else
    echo "Warning: 'valgrind' not found. Skipping cachegrind."
fi

echo "=== Generating FlameGraph ==="
if command -v perf >/dev/null 2>&1; then
    if [ ! -d "${ROOT_DIR}/third_party/FlameGraph" ]; then
        echo "Downloading FlameGraph..."
        mkdir -p "${ROOT_DIR}/third_party"
        git clone https://github.com/brendangregg/FlameGraph.git "${ROOT_DIR}/third_party/FlameGraph"
    fi
    
    perf record -F 99 -g -o "${RESULTS_DIR}/perf.data" -- "$BENCH_BIN"
    perf script -i "${RESULTS_DIR}/perf.data" | \
        "${ROOT_DIR}/third_party/FlameGraph/stackcollapse-perf.pl" | \
        "${ROOT_DIR}/third_party/FlameGraph/flamegraph.pl" > "${RESULTS_DIR}/flamegraph.svg"
    echo "flamegraph.svg generated."
else
    echo "Warning: 'perf' not found. Skipping FlameGraph."
fi

echo "=== Generating Cache Analysis Report ==="
cat <<EOF > "${RESULTS_DIR}/cache_analysis.md"
# Cache Analysis Report

## Performance Counters
\`\`\`text
$(cat "${RESULTS_DIR}/perf_report.txt")
\`\`\`

## Observations
- ObjectPool allocations should demonstrate significantly fewer L1 misses than system malloc.
- Wait-free queues (SPSC) will be analyzed in Phase 2 for false-sharing cacheline bouncing.

## Next Steps
- Review \`flamegraph.svg\` to verify hot-path operations (e.g. \`ArenaAllocator::allocate\`) are completely inlined and absent from the flamegraph.
EOF

echo "=== Infrastructure Validation Complete ==="
echo "Results available in ${RESULTS_DIR}/"
