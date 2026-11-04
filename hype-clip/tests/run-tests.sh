#!/usr/bin/env bash
#
# Builds and runs the Hype Clip detector tests.
#
# Deciding when a moment is worth clipping has no libobs and no Qt
# dependencies, so this needs nothing but a C++17 compiler: the tests feed
# synthetic microphones and a fake clock.
#
#   ./tests/run-tests.sh
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ROOT}/build_tests"
BIN="${BUILD}/hype-core-tests"

CXX="${CXX:-g++}"

mkdir -p "${BUILD}"

echo "==> compiling"
# shellcheck disable=SC2086
"${CXX}" -std=c++17 -O1 -g -Wall -Wextra \
	-I"${ROOT}/src" \
	"${ROOT}/src/hype-core.cpp" \
	"${ROOT}/tests/hype-core-test.cpp" \
	-o "${BIN}"

echo "==> running"
"${BIN}"
