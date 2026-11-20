#!/usr/bin/env bash
# MixBloom model and real adapter against fake OBS; no OBS or Qt installation required.
# Copyright (C) 2026 Emmanuel Suah. GPL-2.0-or-later.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build_tests
CXX="${CXX:-g++}"
flags=(-std=c++17 -Wall -Wextra -Werror -pedantic -pthread -Isrc -Itests -Itests/fake-obs)
"$CXX" "${flags[@]}" src/mix-core.cpp tests/core-tests.cpp -o build_tests/model-tests
"$CXX" "${flags[@]}" src/mix-core.cpp src/obs-mixer.cpp tests/fake-obs.cpp tests/obs-adapter-tests.cpp -o build_tests/adapter-tests
./build_tests/model-tests
./build_tests/adapter-tests
python3 tests/guide-tests.py
