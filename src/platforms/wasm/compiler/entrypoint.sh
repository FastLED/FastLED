#!/bin/bash

source /emsdk/emsdk_env.sh
python init_runtime.py
./final_prewarm.sh
exec "$@"