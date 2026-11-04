#!/usr/bin/env bash
#
# Builds and runs the Auto Director policy tests.
#
# The directing policy (src/director-core.*) has no libobs and no Qt
# dependencies, so this needs nothing but a C++17 compiler - no OBS, no Qt, no
# stream. The tests drive it with synthetic microphones and a fake clock.
#
#   ./tests/run-tests.sh
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ROOT}/build_tests"
BIN="${BUILD}/director-core-tests"

CXX="${CXX:-g++}"

mkdir -p "${BUILD}"

echo "==> compiling"
# shellcheck disable=SC2086
"${CXX}" -std=c++17 -O1 -g -Wall -Wextra \
	-I"${ROOT}/src" \
	"${ROOT}/src/director-core.cpp" \
	"${ROOT}/tests/director-core-test.cpp" \
	-o "${BIN}"

echo "==> running"
"${BIN}"
