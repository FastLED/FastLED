#!/bin/bash

# if os env has PREWARM == "true"
if [ "$PREWARM" == "true" ]; then
  echo "Prewarming..."
else
  exit 0
fi

mkdir -p /mapped && cp -r /js/fastled/examples/wasm /mapped/wasm
python /js/run.py compile --debug --mapped-dir /js/fastled/examples/wasm && rm -rf /js/.pio/build/wasm/src/wasm.ino.o || echo "failed to delete wasm.ino.o"

mkdir -p /mapped && cp -r /js/fastled/examples/wasm /mapped/wasm
python /js/run.py compile --quick --mapped-dir /js/fastled/examples/wasm && rm -rf /js/.pio/build/wasm/src/wasm.ino.o || echo "failed to delete wasm.ino.o"

mkdir -p /mapped && cp -r /js/fastled/examples/wasm /mapped/wasm
python /js/run.py compile --release --mapped-dir /js/fastled/examples/wasm && -rf /js/.pio/build/wasm/src/wasm.ino.o || echo "failed to delete wasm.ino.o"
