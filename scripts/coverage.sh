#!/usr/bin/env bash
# Generate gcov line coverage report for compiled firmware sources.
# Usage: ./scripts/coverage.sh [--no-build] [--csv]
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/build-coverage"
OUT="$ROOT/coverage"

NO_BUILD=0
CSV=0
for arg in "$@"; do
    case "$arg" in
        --no-build) NO_BUILD=1 ;;
        --csv)      CSV=1 ;;
    esac
done

if [ "$NO_BUILD" -eq 0 ]; then
    cmake -B "$BUILD" -G Ninja -DCOVERAGE=ON -S "$ROOT" > /dev/null
    ninja -C "$BUILD"
    # Clear old gcda files for a clean run
    find "$BUILD" -name '*.gcda' -delete 2>/dev/null || true
    "$BUILD/HarnessTests.exe" -c
fi

mkdir -p "$OUT"

if [ "$CSV" -eq 1 ]; then
    gcovr --root "$ROOT" --filter "firmware/src/deluge/" --csv --print-summary
else
    gcovr --root "$ROOT" --filter "firmware/src/deluge/" \
        --html-details "$OUT/index.html" --print-summary
    echo ""
    echo "HTML report: $OUT/index.html"
fi
