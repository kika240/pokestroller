#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
task_test_dir="$(mktemp -d "${TMPDIR:-/tmp}/pokestroller-tests.XXXXXX")"
trap 'rm -rf "$task_test_dir"' EXIT
task_flags=(-std=gnu11 -g -O1 -fno-strict-aliasing -fno-omit-frame-pointer
            -fsanitize=address,undefined -fno-sanitize-recover=all -Isrc)
"${CC:-clang}" "${task_flags[@]}" src/walker.c src/queue.c tests/core_test.c -o "$task_test_dir/core-test"
"$task_test_dir/core-test" "$task_test_dir"
if [[ "$#" -gt 0 ]]; then
    "${CC:-clang}" "${task_flags[@]}" src/walker.c src/queue.c tests/smoke.c -o "$task_test_dir/smoke"
    "$task_test_dir/smoke" "$@"
fi
