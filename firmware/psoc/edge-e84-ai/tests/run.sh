#!/usr/bin/env sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p .host-build
cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined \
  -Itests/fakes -Iproj_cm55/edge tests/test_edge.c \
  proj_cm55/edge/protocol_v1.c proj_cm55/edge/radar_events.c \
  proj_cm55/edge/i2c_target.c -lm -o .host-build/test_edge
.host-build/test_edge
