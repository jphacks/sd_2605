#!/usr/bin/env sh
set -eu
repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
out=$(mktemp)
trap 'rm -f "$out"' EXIT
cc -std=c11 -Wall -Wextra -Werror \
  -I"$repo_root/firmware/psoc/edge-e84-eval/include" \
  "$repo_root/firmware/psoc/edge-e84-eval/src/protocol_v1.c" \
  "$repo_root/firmware/psoc/edge-e84-eval/src/vision_events.c" \
  "$repo_root/firmware/psoc/edge-e84-eval/src/direction_detector.c" \
  "$repo_root/tests/edge-e84/edge_e84_logic_test.c" -o "$out"
"$out"
