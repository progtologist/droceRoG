#!/bin/sh
cmake -DCMAKE_BUILD_TYPE=Release -DTARGET_TYPE=ARM
make
if [ -e drocerog ]; then
    mv drocerog drocerog.app
fi
