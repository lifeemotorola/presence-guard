#!/usr/bin/env bash
#
# Builds and runs the Auto Scene Adjust geometry tests.
#
# Where a source belongs, what is wrong with it and how the canvas splits are
# all plain arithmetic, so this needs nothing but a C++17 compiler: no OBS, no
# Qt, no canvas.
#
#   ./tests/run-tests.sh
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ROOT}/build_tests"
BIN="${BUILD}/scene-core-tests"

CXX="${CXX:-g++}"

mkdir -p "${BUILD}"

echo "==> compiling"
# shellcheck disable=SC2086
"${CXX}" -std=c++17 -O1 -g -Wall -Wextra \
	-I"${ROOT}/src" \
	"${ROOT}/src/scene-core.cpp" \
	"${ROOT}/tests/scene-core-test.cpp" \
	-o "${BIN}"

echo "==> running"
"${BIN}"
