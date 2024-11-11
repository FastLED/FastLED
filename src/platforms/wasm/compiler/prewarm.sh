#!/bin/bash

mkdir -p /mapped && cp -r /js/fastled/examples/wasm /mapped/wasm
python /js/run.py compile && rm -rf /mapped && rm -rf /js/.pio/build/wasm/src/wasm.ino.o || echo "failed to delete wasm.ino.o"
