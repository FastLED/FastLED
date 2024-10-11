#!/bin/bash
set -e
cp -r /mapped/* /js/
rm -rf /js/.pio
cp -r /wasm/* /js/
cd /js
for ino_file in *.ino; do
    if [ -f "$ino_file" ]; then
        sed -i "1i#include \"platforms/stub/wasm/led_sysdefs_wasm.hpp\"" "$ino_file"
    fi
done
pio run
rm -rf /mapped/fastled_js/*
mkdir -p /mapped/fastled_js
cp ./.pio/build/*/fastled.js /mapped/fastled_js/fastled.js
cp ./.pio/build/*/fastled.wasm /mapped/fastled_js/fastled.wasm
cp ./index.html /mapped/fastled_js/index.html
