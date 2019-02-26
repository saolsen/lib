#!/bin/bash

CODE_PATH="$(dirname "$0")"

mkdir -p "$CODE_PATH/build"
pushd "$CODE_PATH/build"

clang -O2 -g -std=c99 -Wall -fdiagnostics-absolute-paths \
    -o test_lib \
    ../test_lib.c

popd