#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build-host}"

cmake -S "$ROOT" -B "$BUILD_DIR" -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build "$BUILD_DIR" --parallel
ctest --test-dir "$BUILD_DIR" --output-on-failure
