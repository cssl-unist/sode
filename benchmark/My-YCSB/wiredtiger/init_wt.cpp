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
	                          true,
	                          config.wiredtiger.create_table_config.c_str(),
	                          false);
	factory.update_cursor_config(WiredTigerClient::cursor_bulk_config);
	run_init_workload_with_op_measurement("Initialization", &factory,
	                                      config.database.nr_entry,
	                                      config.database.key_size,
	                                      config.database.value_size,
	                                      1);
}
