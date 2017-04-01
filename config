#!/bin/zsh

mkdir build
cd build
cmake -DOPENSSL_ROOT_DIR=$(brew --prefix openssl) -DOPENSSL_INCLUDE_DIR=$(brew --prefix openssl)"/include" ..
make