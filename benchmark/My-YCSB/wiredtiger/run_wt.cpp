#include <iostream>
#include "worker.h"
#include "wt_client.h"
#include "wt_config.h"

int main(int argc, char *argv[]) {
	if (argc != 2) {
		printf("Usage: %s <config file>\n", argv[0]);
		return -EINVAL;
	}
	YAML::Node file = YAML::LoadFile(argv[1]);
	WiredTigerConfig config = WiredTigerConfig::parse_yaml(file);

	WiredTigerFactory factory(config.wiredtiger.data_dir.c_str(),
	                          config.wiredtiger.table_name.c_str(),
	                          config.wiredtiger.conn_config.c_str(),
	                          config.wiredtiger.session_config.c_str(),
	                          config.wiredtiger.cursor_config.c_str(),
	                          false,
	                          config.wiredtiger.create_table_config.c_str(),
	                          config.wiredtiger.print_stats);
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
			                                         nullptr);
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
			                                         nullptr);
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
			                                        nullptr);
		} else if (config.workload.request_distribution == "trace") {
			run_trace_workload_with_op_measurement(i == 0 ? "Trace (Warm-Up)" : "Trace",
			                                       &factory,
			                                       config.database.key_size,
			                                       config.database.value_size,
			                                       config.workload.nr_thread,
			                                       config.workload.trace_file_list,
			                                       nr_op,
			                                       config.workload.next_op_interval_ns,
			                                       nullptr);
		} else {
			throw std::invalid_argument("unrecognized workload");
		}
	}
}
