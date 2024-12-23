wget https://github.com/wiredtiger/wiredtiger/archive/refs/tags/mongodb-4.4.0.zip
unzip mongodb-4.4.0.zip
cd wiredtiger-mongodb-4.4.0
./autogen.sh
./configure
make -j8
sudo make install
