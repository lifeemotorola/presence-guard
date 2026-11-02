#!/usr/bin/env bash
#
# Builds and runs the Presence Guard state-machine harness.
#
# Requires: a C++17 compiler, Qt6 Core development headers (pkg-config name
# Qt6Core) and Python-free sanity. It links the real src/presence-guard.cpp
# against the fake libobs in tests/stubs, so no OBS installation is needed.
#
#   ./tests/run-tests.sh
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ROOT}/build_tests"
BIN="${BUILD}/presence-guard-tests"

CXX="${CXX:-g++}"
QT_CFLAGS="$(pkg-config --cflags Qt6Core)"
QT_LIBS="$(pkg-config --libs Qt6Core)"

mkdir -p "${BUILD}"

echo "==> compiling"
# shellcheck disable=SC2086
"${CXX}" -std=c++17 -O1 -g -Wall -Wextra \
	-I"${ROOT}/tests/stubs" \
	-I"${ROOT}/src" \
	${QT_CFLAGS} \
	"${ROOT}/src/presence-guard.cpp" \
	"${ROOT}/tests/fake-platform-idle.cpp" \
	"${ROOT}/tests/stub-support.cpp" \
	"${ROOT}/tests/state-machine-test.cpp" \
	-o "${BIN}" \
	${QT_LIBS}

echo "==> running"
# The harness drives the real 250 ms poll timer, so it takes a few seconds.
QT_QPA_PLATFORM=offscreen "${BIN}"
