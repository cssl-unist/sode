#!/bin/bash

# Install build dependencies
printf "Installing dependencies...\n"
sudo apt-get update
sudo apt-get install -y build-essential unzip python3 clang-10 llvm-10 python3.6

# Install YAML CPP
wget -O /tmp/yaml-cpp-0.6.3.zip https://github.com/jbeder/yaml-cpp/archive/yaml-cpp-0.6.3.zip
pushd /tmp
unzip yaml-cpp-0.6.3.zip
cd yaml-cpp-yaml-cpp-0.6.3
mkdir build
cd build
cmake ..
make -j8
sudo make install
popd
