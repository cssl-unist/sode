#include <iostream>
#include "worker.h"
#include "memcached_client.h"
#include "memcached_config.h"
#include "yaml-cpp/yaml.h"

int main(int argc, char *argv[]) {
	if (argc != 2 && argc != 3) {
		printf("Usage: %s <config file> <redis port (optional)>\n", argv[0]);
		return -EINVAL;
	}
	YAML::Node file = YAML::LoadFile(argv[1]);
	MemcachedConfig config = MemcachedConfig::parse_yaml(file);
	int port = config.memcached.port;
	if (argc == 3)
		port = atoi(argv[2]);

	MemcachedFactory factory(config.memcached.addr.c_str(), port);

	run_init_workload_with_op_measurement("Initialization",
	                                      &factory,
	                                      config.database.nr_entry,
	                                      config.database.key_size,
	                                      config.database.value_size,
	                                      config.workload.nr_init_thread);
}
