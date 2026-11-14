#!/bin/sh
# Host tests for the unlock rules and the credits flow (MSYS2 g++).
# Windows: C:\msys64\usr\bin\bash.exe -lc "sh tools/tests/run_unlock_tests.sh"
# OUT (optional): folder for the test binaries, default build/host-tests.
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
OUT=${OUT:-$ROOT/build/host-tests}
mkdir -p "$OUT"
CXX=${CXX:-g++}
FLAGS="-std=c++20 -O1 -Wall -Wextra -Werror -I$ROOT/include"
$CXX $FLAGS "$ROOT/tools/tests/unlock_rules.cpp" "$ROOT/src/unlocks.cpp" -o "$OUT/unlock_rules"
$CXX $FLAGS "$ROOT/tools/tests/credits_flow.cpp" "$ROOT/src/credits.cpp" "$ROOT/src/credits_run.cpp" \
    "$ROOT/src/demo_stage.cpp" "$ROOT/src/unlocks.cpp" -o "$OUT/credits_flow"
$CXX $FLAGS -Wno-psabi "$ROOT/tools/tests/unlock_save.cpp" "$ROOT/src/unlock_save.cpp" "$ROOT/src/unlocks.cpp" \
    "$ROOT/src/save_store.cpp" "$ROOT/src/license_mii.cpp" -o "$OUT/unlock_save"
g++ -std=gnu++23 -O1 -Wall -Wno-psabi -I"$ROOT/include" "$ROOT/tools/tests/menu_locks.cpp" "$ROOT/src/menu_state.cpp" "$ROOT/src/menu_focus.cpp" \
    "$ROOT/src/menu_transition.cpp" "$ROOT/src/menu_index.cpp" "$ROOT/src/unlocks.cpp" -o "$OUT/menu_locks"
"$OUT/unlock_rules"
"$OUT/credits_flow"
"$OUT/unlock_save"
"$OUT/menu_locks"
