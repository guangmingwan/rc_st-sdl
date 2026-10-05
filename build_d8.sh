#!/bin/sh
# Build st for D8 (480x272, JZ4755) using the retrofw SDK (same as kbd4)
export PATH="/opt/retrofw/bin:$PATH"
make clean
exec make TARGET=d8 CC=mipsel-linux-gcc "$@"
