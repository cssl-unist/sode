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
	const char *latency_file = nullptr;
	if (!config.measurement.latency_file.empty())
		latency_file = config.measurement.latency_file.c_str();

	MemcachedFactory factory(config.memcached.addr.c_str(), port);

	OpProportion op_prop;
	op_prop.op[READ] = config.workload.operation_proportion.read;
	op_prop.op[UPDATE] = config.workload.operation_proportion.update;
	op_prop.op[INSERT] = config.workload.operation_proportion.insert;
	op_prop.op[SCAN] = config.workload.operation_proportion.scan;
	op_prop.op[READ_MODIFY_WRITE] = config.workload.operation_proportion.read_modify_write;
	for (int i = 0; i < 2; ++i) {
		long nr_op;
		if (i == 0) {
			if (config.workload.nr_warmup_op == 0)
				continue;
			nr_op = config.workload.nr_warmup_op;
		} else {
			nr_op = config.workload.nr_op;
		}
		if (config.workload.request_distribution == "uniform") {
			run_uniform_workload_with_op_measurement(i == 0 ? "Uniform (Warm-Up)" : "Uniform",
			                                         &factory,
			                                         config.database.nr_entry,
			                                         config.database.key_size,
			                                         config.database.value_size,
			                                         config.workload.scan_length,
			                                         config.workload.nr_thread,
			                                         op_prop,
			                                         nr_op,
			                                         config.workload.next_op_interval_ns,
			                                         latency_file);
		} else if (config.workload.request_distribution == "zipfian") {
			run_zipfian_workload_with_op_measurement(i == 0 ? "Zipfian (Warm-Up)" : "Zipfian",
			                                         &factory,
			                                         config.database.nr_entry,
			                                         config.database.key_size,
			                                         config.database.value_size,
			                                         config.workload.scan_length,
			                                         config.workload.nr_thread,
			                                         op_prop,
			                                         config.workload.zipfian_constant,
			                                         nr_op,
			                                         config.workload.next_op_interval_ns,
			                                         latency_file);
		} else if (config.workload.request_distribution == "latest") {
			run_latest_workload_with_op_measurement(i == 0 ? "Latest (Warm-Up)" : "Latest",
			                                        &factory,
			                                        config.database.nr_entry,
			                                        config.database.key_size,
			                                        config.database.value_size,
			                                        config.workload.nr_thread,
			                                        op_prop.op[READ],
			                                        config.workload.zipfian_constant,
			                                        nr_op,
			                                        config.workload.next_op_interval_ns,
			                                        latency_file);
		}
	}
}

