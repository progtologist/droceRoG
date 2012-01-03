#!/usr/bin/env bash

PB_HEADERS_PATH='../../../PBSDK/include/'
ARM_HEADERS_PATH='../../../PBSDK/arm-linux/include/'
CTAGS_FLAGS='-R --sort=1 --c++-kinds=+p --fields=+iaS --extra=+q'
ctags $CTAGS_FLAGS $HEADERS_PATH $ARM_HEADERS_PATH .
