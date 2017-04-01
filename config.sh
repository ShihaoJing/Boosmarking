#!/bin/bash

if [ -d build ]; then
  rm -rf build
fi
mkdir build
cd build
if [[ "$OSTYPE" == "linux-gnu" ]]; then
        cmake ..
elif [[ "$OSTYPE" == "darwin"* ]]; then
        # Mac OSX
        cmake -DOPENSSL_ROOT_DIR=$(brew --prefix openssl) -DOPENSSL_INCLUDE_DIR=$(brew --prefix openssl)"/include" ..
fi