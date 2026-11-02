#!/usr/bin/env bash
#
# Builds and runs the Presence Guard state-machine harness.
#
# Requires: a C++17 compiler and Qt6 Core development headers. It links the
# real src/presence-guard.cpp against the fake libobs in tests/stubs, so no OBS
# installation is needed.
#
# Qt is located in this order:
#   1. pkg-config (the normal case on Linux distributions and Homebrew)
#   2. $QTDIR, i.e. QTDIR=/path/to/qt6 ./tests/run-tests.sh
#   3. Qt6_DIR, i.e. a CMake package directory (Qt6_DIR=/qt/6.8.3/gcc_64/lib/cmake/Qt6)
#
#   ./tests/run-tests.sh
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ROOT}/build_tests"
BIN="${BUILD}/presence-guard-tests"

CXX="${CXX:-g++}"
QT_CFLAGS=""
QT_LIBS=""
QT_RPATH=""

if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists Qt6Core 2>/dev/null; then
	QT_CFLAGS="$(pkg-config --cflags Qt6Core)"
	QT_LIBS="$(pkg-config --libs Qt6Core)"
	QT_VERSION="$(pkg-config --modversion Qt6Core)"
elif [[ -n "${QTDIR:-}" ]]; then
	QT_CFLAGS="-I${QTDIR}/include -I${QTDIR}/include/QtCore"
	QT_LIBS="-L${QTDIR}/lib -lQt6Core"
	# Keep the run working when the Qt install is not in the loader's path.
	QT_RPATH="-Wl,-rpath,${QTDIR}/lib"
	QT_VERSION="(QTDIR=${QTDIR})"
elif [[ -n "${Qt6_DIR:-}" ]]; then
	QTDIR="$(cd "${Qt6_DIR}/../../.." && pwd)"
	QT_CFLAGS="-I${QTDIR}/include -I${QTDIR}/include/QtCore"
	QT_LIBS="-L${QTDIR}/lib -lQt6Core"
	QT_RPATH="-Wl,-rpath,${QTDIR}/lib"
	QT_VERSION="(Qt6_DIR=${Qt6_DIR})"
else
	echo "error: Qt6 Core development files not found." >&2
	echo "       Install them (e.g. 'qt6-base-dev'), or point QTDIR at a Qt6" >&2
	echo "       installation, e.g. QTDIR=\$HOME/Qt/6.8.3/gcc_64 ./tests/run-tests.sh" >&2
	exit 1
fi

echo "==> Qt: ${QT_VERSION}"

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
	${QT_LIBS} ${QT_RPATH}

echo "==> running"
# The harness drives the real 250 ms poll timer, so it takes a few seconds.
QT_QPA_PLATFORM=offscreen "${BIN}"
