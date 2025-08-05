#!/bin/bash

set -e

./compile uno "$@" examples/Blink --use-pio-run
