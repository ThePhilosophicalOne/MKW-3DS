#!/bin/sh
# Host tests for the save store. Run from Windows with MSYS2 (Git Bash does not build these):
#   C:\msys64\usr\bin\bash.exe -lc "tools/tests/run_save_tests.sh"
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${TMPDIR:-/tmp}/mkw-save-tests"
mkdir -p "$OUT"
g++ -std=gnu++23 -O1 -Wall -Wextra -Wno-psabi -I"$ROOT/include" \
    "$ROOT/tools/tests/save_store.cpp" "$ROOT/src/save_store.cpp" "$ROOT/src/license_mii.cpp" \
    -o "$OUT/save_store_test"
"$OUT/save_store_test"
