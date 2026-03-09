#!/usr/bin/env bash
# Input to this file: a board configuration like `sim:nsh`
#
# Output: the full list of sources that the target uses
#
# Pre-requisites:
# - Clean build system
# - jq command for JSON parsing

config="$1"

# TODO: Quiet CMake

mkdir -p build/.cmake/api/v1/query/codemodel-v2 # Create API query
cmake -S . -B build -DBOARD_CONFIG=${config} -GNinja # Generate configuration

# Output list of source files
sources="$(cat build/.cmake/api/v1/reply/target-*.json | jq '.sources[].path')"

cmake --build build -t clean # Clean configuration

echo "=== SOURCE FILES FOR CONFIG ${config} ==="
echo "${sources}"
