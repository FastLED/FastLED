#!/bin/bash

set -e
set -x

python process-ino.py src/wasm.ino.cpp
rm -rf src/wasm.ino
python compile.py --only-compile