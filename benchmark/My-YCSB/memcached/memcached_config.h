#ifndef YCSB_MEMCACHED_CONFIG_H
#define YCSB_MEMCACHED_CONFIG_H

#include <string>
#include "yaml-cpp/yaml.h"

using std::string;

struct MemcachedConfig {
	struct {
		long key_size;
		long value_size;
		long nr_entry;
	} database;
	struct {
		long nr_warmup_op;
		long nr_op;
		int nr_init_thread;
		int nr_thread;
		long next_op_interval_ns;
		struct {
			float read;
			float update;
			float insert;
			float scan;
			float read_modify_write;
		} operation_proportion;
		string request_distribution;
		double zipfian_constant;
		long scan_length;
	} workload;
	struct {
		string latency_file;
	} measurement;
	struct {
		string addr;
		int port;
	} memcached;

	static MemcachedConfig parse_yaml(YAML::Node &root);
};


MemcachedConfig MemcachedConfig::parse_yaml(YAML::Node &root) {
	MemcachedConfig config;
	YAML::Node database = root["database"];
	config.database.key_size = database["key_size"].as<long>();
	config.database.value_size = database["value_size"].as<long>();
	config.database.nr_entry = database["nr_entry"].as<long>();

	YAML::Node workload = root["workload"];
	config.workload.nr_warmup_op = workload["nr_warmup_op"].as<long>();
	config.workload.nr_op = workload["nr_op"].as<long>();
	config.workload.nr_init_thread = workload["nr_init_thread"].as<int>();
	config.workload.nr_thread = workload["nr_thread"].as<int>();
	config.workload.next_op_interval_ns = workload["next_op_interval_ns"].as<long>();
	YAML::Node operation_proportion = workload["operation_proportion"];
	config.workload.operation_proportion.read = operation_proportion["read"].as<float>();
	config.workload.operation_proportion.update = operation_proportion["update"].as<float>();
	config.workload.operation_proportion.insert = operation_proportion["insert"].as<float>();
	config.workload.operation_proportion.scan = operation_proportion["scan"].as<float>();
	config.workload.operation_proportion.read_modify_write = operation_proportion["read_modify_write"].as<float>();
	config.workload.request_distribution = workload["request_distribution"].as<string>();
	config.workload.zipfian_constant = workload["zipfian_constant"].as<double>();
	config.workload.scan_length = workload["scan_length"].as<long>();

	YAML::Node measurement = root["measurement"];
	config.measurement.latency_file = measurement["latency_file"].as<string>();

	YAML::Node memcached = root["memcached"];
	config.memcached.addr = memcached["addr"].as<string>();
	config.memcached.port = memcached["port"].as<int>();

	return config;
}

#endif //YCSB_MEMCACHED_CONFIG_H
