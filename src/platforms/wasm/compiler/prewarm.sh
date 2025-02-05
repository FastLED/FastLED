#!/bin/bash

set -x
set -e

# --force flag to force prewarm
#test to see if --force flag is set
forced=0
if [ "$1" == "--force" ]; then
  forced=1
fi

echo "Forced: $forced"
echo "NO_PREWARM: $NO_PREWARM"

# if os env has PREWARM == "true"
if [ "$forced" == "0" ]; then
  if [ "$NO_PREWARM" != "1" ]; then
    echo "Prewarming..."
  else
    echo "Skipping prewarm..."
    exit 0
  fi
fi

rm -rf /prewarm
mkdir -p /prewarm && cp -r /js/fastled/examples/wasm /prewarm/wasm
python /js/compile.py --debug --mapped-dir /prewarm && rm -rf /js/.pio/build/wasm/src/wasm.ino.o || echo "failed to delete wasm.ino.o"
rm -rf /prewarm

mkdir -p /prewarm && cp -r /js/fastled/examples/wasm /prewarm/wasm
python /js/compile.py --quick --mapped-dir /prewarm && rm -rf /js/.pio/build/wasm/src/wasm.ino.o || echo "failed to delete wasm.ino.o"
rm -rf /prewarm

mkdir -p /prewarm && cp -r /js/fastled/examples/wasm /prewarm/wasm
python /js/compile.py --release --mapped-dir /prewarm && rm -rf /js/.pio/build/wasm/src/wasm.ino.o || echo "failed to delete wasm.ino.o"
rm -rf /prewarm

mkdir -p /prewarm && cp -r /js/fastled/examples/wasm /prewarm/wasm
cd /js
python compile.py --no-platformio --mapped-dir /prewarm  # 60 seconds -> 5 seconds
python compile.py --no-platformio --mapped-dir /prewarm  # 5 seconds -> 0.5 seconds
rm -rf /prewarm
