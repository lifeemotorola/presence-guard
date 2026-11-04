#!/usr/bin/env bash
#
# Builds and runs the OBS Beautifier look tests.
#
# The whole skin - colour maths, option clamping and the generated Qt
# stylesheet - has no Qt and no libobs dependencies, so this needs nothing but a
# C++17 compiler. The tests check what matters for a UI skin: readable contrast,
# well-formed rules, and that every option actually changes the sheet.
#
#   ./tests/run-tests.sh
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ROOT}/build_tests"
BIN="${BUILD}/beautifier-core-tests"

CXX="${CXX:-g++}"

mkdir -p "${BUILD}"

echo "==> compiling"
# shellcheck disable=SC2086
"${CXX}" -std=c++17 -O1 -g -Wall -Wextra \
	-I"${ROOT}/src" \
	"${ROOT}/src/beautifier-core.cpp" \
	"${ROOT}/tests/beautifier-core-test.cpp" \
	-o "${BIN}"

echo "==> running"
"${BIN}"
