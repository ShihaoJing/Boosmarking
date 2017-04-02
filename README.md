# Boostbench

# Build
# Prerequisite
```
sudo apt-get install libssl-dev
sudo apt-get install libboost-all-dev
```
## on Mac
```
mkdir build
cd build
cmake -DOPENSSL_ROOT_DIR=$(brew --prefix openssl) -DOPENSSL_INCLUDE_DIR=$(brew --prefix openssl)"/include" ..
```
