sudo apt-get install build-essential unzip
wget https://github.com/jbeder/yaml-cpp/archive/yaml-cpp-0.6.3.zip
unzip yaml-cpp-0.6.3.zip
cd yaml-cpp-yaml-cpp-0.6.3
mkdir build
cd build
cmake ..
make -j8
sudo make install
cd ..
