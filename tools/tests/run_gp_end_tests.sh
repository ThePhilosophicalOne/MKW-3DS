#!/bin/sh
# Host tests for the Grand Prix rank, standings, award flow and demo stage (MSYS2 g++).
# Windows: C:\msys64\usr\bin\bash.exe -lc "sh tools/tests/run_gp_end_tests.sh"
# OUT (optional): folder for the test binaries, default build/host-tests.
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
OUT=${OUT:-$ROOT/build/host-tests}
mkdir -p "$OUT"
CXX=${CXX:-g++}
# The tree's root for the cooked-data checks (a Windows path the native g++ can open).
ROOTW=$(cygpath -m "$ROOT" 2>/dev/null || echo "$ROOT")
FLAGS="-std=c++20 -O1 -Wall -Wextra -Werror -ffp-contract=off -I$ROOT/include -DMKW_SOURCE_ROOT=\"$ROOTW\""
$CXX $FLAGS "$ROOT/tools/tests/gp_end.cpp" "$ROOT/src/award_flow.cpp" "$ROOT/src/demo_stage.cpp" -o "$OUT/gp_end"
"$OUT/gp_end"
