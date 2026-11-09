#!/usr/bin/env bash
# OnAirTwin core tests. No OBS, Qt, camera or network is required.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ROOT}/build_tests"
mkdir -p "${BUILD}"
"${CXX:-g++}" -std=c++17 -O1 -g -Wall -Wextra -Werror \
  -I"${ROOT}/src" "${ROOT}/src/twin-core.cpp" "${ROOT}/tests/twin-core-test.cpp" \
  -o "${BUILD}/twin-core-tests"
"${BUILD}/twin-core-tests"
"${CXX:-g++}" -std=c++17 -O1 -g -Wall -Wextra -Werror \
  -I"${ROOT}/tests/fake-obs" -I"${ROOT}/src" \
  "${ROOT}/src/twin-core.cpp" "${ROOT}/src/obs-snapshot.cpp" "${ROOT}/tests/obs-snapshot-test.cpp" \
  -o "${BUILD}/obs-snapshot-tests"
"${BUILD}/obs-snapshot-tests"
