# -*- mode: ruby -*-
# vi: set ft=ruby :

# All Vagrant configuration is done below. The "2" in Vagrant.configure
# configures the configuration version (we support older styles for
# backwards compatibility). Please don't change it unless you know what
# you're doing.
Vagrant.configure("2") do |config|
  # The most common configuration options are documented and commented below.
  # For a complete reference, please see the online documentation at
  # https://docs.vagrantup.com.

  # Every Vagrant development environment requires a box. You can search for
  # boxes at https://vagrantcloud.com/search.
  # config.vm.box = "ubuntu/bionic64"
  # config.vm.box_version = "20200229.0.0"
  #
  #
  config.vm.box = "generic/ubuntu1804"
  config.vm.box_version  = "2.0.6"
  config.vm.define "sode_for_ae"

  # Disable automatic box update checking. If you disable this, then
  # boxes will only be checked for updates when the user runs
  # `vagrant box outdated`. This is not recommended.
  # config.vm.box_check_update = false

  # Create a forwarded port mapping which allows access to a specific port
  # within the machine from a port on the host machine. In the example below,
  # accessing "localhost:8080" will access port 80 on the guest machine.
  # NOTE: This will enable public access to the opened port
  # config.vm.network "forwarded_port", guest: 80, host: 8080
  config.vm.network "forwarded_port", guest: 80, host: 8080

  # Create a forwarded port mapping which allows access to a specific port
  # within the machine from a port on the host machine and only allow access
  # via 127.0.0.1 to disable public access
  # config.vm.network "forwarded_port", guest: 80, host: 8080, host_ip: "127.0.0.1"
  config.vm.network "forwarded_port", guest: 80, host: 8080, host_ip: "127.0.0.1"

  # Create a private network, which allows host-only access to the machine
  # using a specific IP.
  # config.vm.network "private_network", ip: "192.168.33.10"
  config.vm.network "private_network", ip: "192.168.33.11"

  # Create a public network, which generally matched to bridged network.
  # Bridged networks make the machine appear as another physical device on
  # your network.
  # config.vm.network "public_network"

  # Share an additional folder to the guest VM. The first argument is
  # the path on the host to the actual folder. The second argument is
  # the path on the guest to mount the folder. And the optional third
  # argument is a set of non-required options.
  # config.vm.synced_folder "../data", "/vagrant_data"
  # config.vm.synced_folder "utils/", "/home/vagrant/", type: "rsync",
  #  rsync__args: ["-az"]

  # Provider-specific configuration so you can fine-tune various
  # backing providers for Vagrant. These expose provider-specific options.
  # Example for VirtualBox:
  #
  # config.vm.provider "virtualbox" do |vb|
  #   # Display the VirtualBox GUI when booting the machine
  #   vb.gui = true
  #
  #   # Customize the amount of memory on the VM:
  #   vb.memory = "1024"
  # end
  #
  # View the documentation for the provider you are using for more
  # information on available options.

  config.vm.provider :libvirt do |libvirt|
    libvirt.cpus = 24
    libvirt.nested = 'true'
    libvirt.memory = 376832 # unit MB

    # unit MB
    libvirt.numa_nodes = [
        {:cpus => "0-11", :memory => "180224"},
        {:cpus => "12-23", :memory => "196608"}
    ]

    #libvirt.memorybacking :locked
    #libvirt.memorybacking :nosharepages
  end
  # config.ssh.username = 'root'
  # config.ssh.password = 'chan'
  config.ssh.insert_key = 'true'
  
  # Enable provisioning with a shell script. Additional provisioners such as
  # Ansible, Chef, Docker, Puppet and Salt are also available. Please see the
  # documentation for more information about their specific syntax and use.
  # config.vm.provision "shell", inline: <<-SHELL
  #   apt-get update
  #   apt-get install -y apache2
  # SHELL
  config.vm.provision :host_shell do |host_shell|
      host_shell.inline = "sudo virsh attach-disk sode_for_ae /dev/sdd vdb"
  end
  config.vm.provision "shell", inline: <<-SHELL
      apt-get update
      apt-get install build-essential libncurses-dev flex bison openssl libssl-dev dkms libelf-dev libudev-dev libpci-dev libiberty-dev autoconf -y
      apt-get install gcc gdb cmake libtool -y
      apt-get install libssl-dev bison flex libelf-dev ctags -y
      apt-get install libncurses-dev numactl -y
      systemctl enable serial-getty@ttyS0.service
      systemctl start serial-getty@ttyS0.service

      sudo mkdir -p /cached-db
      sudo mount /dev/vda /cached-db
  SHELL
end

