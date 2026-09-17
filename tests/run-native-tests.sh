#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
task_dir="$(mktemp -d "${TMPDIR:-/tmp}/pokestroller-native.XXXXXX")"
trap 'rm -rf "$task_dir"' EXIT
task_flags=(-std=c++23 -O1 -g -fno-strict-aliasing -fno-omit-frame-pointer
    -fsanitize=address,undefined -fno-sanitize-recover=all -Ithird_party/pocketwalker -Isrc)
task_sources=()
while IFS= read -r source; do task_sources+=("$source"); done < <(find third_party/pocketwalker/core -name '*.cpp')
"${CXX:-clang++}" "${task_flags[@]}" src/emulator.cpp src/ir_link.cpp "${task_sources[@]}" tests/emulator_test.cpp -o "$task_dir/emulator-test"
"$task_dir/emulator-test"
clang -fobjc-arc -g -fsanitize=address,undefined -Isrc -framework Foundation src/session_store.m tests/session_test.m -o "$task_dir/session-test"
"$task_dir/session-test" "$task_dir/sessions"

if [[ "$#" -gt 0 ]]; then
    "${CXX:-clang++}" "${task_flags[@]}" src/emulator.cpp "${task_sources[@]}" tests/native_smoke.cpp -o "$task_dir/native-smoke"
    "$task_dir/native-smoke" "$@"
    "${CXX:-clang++}" "${task_flags[@]}" src/emulator.cpp src/ir_link.cpp "${task_sources[@]}" tests/ir_smoke.cpp -o "$task_dir/ir-smoke"
    "$task_dir/ir-smoke" "$@"
fi
