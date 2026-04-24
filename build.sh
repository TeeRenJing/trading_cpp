#!/bin/bash

set -euo pipefail

CMAKE="$(command -v cmake || true)"
NINJA="$(command -v ninja || true)"
MAKE="$(command -v make || true)"

if [[ -z "$CMAKE" ]]; then
  echo "Error: cmake is not installed or not on PATH." >&2
  exit 1
fi

if [[ -n "$NINJA" ]]; then
  GENERATOR="Ninja"
  GENERATOR_ARGS=(-DCMAKE_MAKE_PROGRAM="$NINJA" -G Ninja)
elif [[ -n "$MAKE" ]]; then
  GENERATOR="Unix Makefiles"
  GENERATOR_ARGS=(-G "Unix Makefiles")
else
  echo "Error: neither ninja nor make is installed or on PATH." >&2
  exit 1
fi

mkdir -p cmake-build-release
$CMAKE -DCMAKE_BUILD_TYPE=Release "${GENERATOR_ARGS[@]}" -S . -B cmake-build-release

echo "Using generator: $GENERATOR"
$CMAKE --build cmake-build-release --target clean -j 4
$CMAKE --build cmake-build-release --target all -j 4
