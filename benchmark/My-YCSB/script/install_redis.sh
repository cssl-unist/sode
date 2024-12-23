WORK_DIR=$(pwd)

# install java & maven
cd /opt
sudo wget --no-cookies --no-check-certificate --header "Cookie: oraclelicense=accept-securebackup-cookie" https://javadl.oracle.com/webapps/download/GetFile/1.8.0_281-b09/89d678f2be164786b292527658ca1605/linux-i586/jdk-8u281-linux-x64.tar.gz
sudo tar -xzvf jdk-8u281-linux-x64.tar.gz
sudo mv jdk1.8.0_281 jdk1.8

sudo wget https://archive.apache.org/dist/maven/maven-3/3.6.0/binaries/apache-maven-3.6.0-bin.tar.gz
sudo tar -xvzf apache-maven-3.6.0-bin.tar.gz
sudo mv apache-maven-3.6.0 maven
sudo touch /etc/profile.d/mavenenv.sh

sudo bash -c "echo \"export M2_HOME=/opt/maven\" >> /etc/profile.d/mavenenv.sh"
sudo bash -c "echo export PATH=\\\${M2_HOME}/bin:\\\${PATH} >> /etc/profile.d/mavenenv.sh"
sudo bash -c "echo \"export JAVA_HOME=/opt/jdk1.8\" >> /etc/profile.d/mavenenv.sh"
sudo bash -c "echo export PATH=\\\${JAVA_HOME}/bin:\\\${PATH} >> /etc/profile.d/mavenenv.sh"
sudo chmod +x /etc/profile.d/mavenenv.sh
source /etc/profile.d/mavenenv.sh

# install redis
cd $WORK_DIR
wget https://github.com/antirez/redis/archive/6.0.3.zip
unzip 6.0.3.zip
mv redis-6.0.3 redis
cd redis 
make distclean
make -j8

# config redis
sudo bash -c "echo 'vm.overcommit_memory=1' >> /etc/sysctl.conf"
sudo sysctl vm.overcommit_memory=1
sudo bash -c "echo never > /sys/kernel/mm/transparent_hugepage/enabled"
sudo bash -c "echo never > /sys/kernel/mm/transparent_hugepage/defrag"
sed -i 's/save 900 1//g' redis.conf
sed -i 's/save 300 10//g' redis.conf
sed -i 's/save 60 10000//g' redis.conf
sed -i -e '$a save ""' redis.conf

# install hiredis
cd $WORK_DIR
wget https://github.com/redis/hiredis/archive/refs/tags/v1.0.0.zip
unzip v1.0.0.zip
mv hiredis-1.0.0 hiredis
cd hiredis
mkdir build
cd build
cmake ..
make -j8
sudo make install
ldconfig
