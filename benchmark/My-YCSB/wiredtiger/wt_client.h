#ifndef YCSB_WT_CLIENT_H
#define YCSB_WT_CLIENT_H

#include "client.h"
#include <wiredtiger.h>


struct WiredTigerFactory;

struct WiredTigerClient : public Client {
	WT_SESSION *session;
	WT_CURSOR *cursor;
	const char *session_config;
	const char *cursor_config;

	static const char *session_default_config;
	static const char *cursor_default_config;
	static const char *cursor_bulk_config;

	WiredTigerClient(WiredTigerFactory *factory, int id, const char *session_config, const char *cursor_config);
	~WiredTigerClient();
	int do_operation(Operation *op) override;
	int reset() override;
	void close() override;

private:
	int do_update(char *key_buffer, char *value_buffer);
	int do_insert(char *key_buffer, char *value_buffer);
	int do_read(char *key_buffer, char **value);
	int do_scan(char *key_buffer, long scan_length);
	int do_read_modify_write(char *key_buffer, char *value_buffer);
};

struct WiredTigerFactory : public ClientFactory {
	WT_CONNECTION *conn;
	const char *data_dir;
	const char *table_name;
	const char *conn_config;
	const char *session_config;
	const char *cursor_config;
	const char *create_table_config;
	std::atomic<int> client_id;
	bool print_stats;

	static const char *default_data_dir;
	static const char *default_table_name;
	static const char *conn_default_config;
	static const char *create_table_default_config;

	WiredTigerFactory(const char *data_dir, const char *table_name, const char *conn_config,
			  const char *session_config, const char *cursor_config, bool new_table,
			  const char *create_table_config, bool print_stats);
	~WiredTigerFactory();
	void update_session_config(const char *new_session_config);
	void update_cursor_config(const char *new_cursor_config);
	WiredTigerClient *create_client() override;
	void destroy_client(Client *client) override;
	static int print_cursor(WT_CURSOR *cursor);
};

#endif //YCSB_WT_CLIENT_H
