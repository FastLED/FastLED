#!/bin/bash

source /emsdk/emsdk_env.sh
export PATH=$PATH:/emsdk/upstream/bin
python init_runtime.py
./final_prewarm.sh
exec "$@"