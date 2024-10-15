#!/bin/bash

set -e
set -x

python compile.py --only-copy
cp src/wasm.ino src/wasm.ino.cpp
python process-ino.py src/wasm.ino.cpp
python compile.py --only-compile
