#include "wt_client.h"

const char *WiredTigerClient::session_default_config = "isolation=read-uncommitted";
const char *WiredTigerClient::cursor_default_config = nullptr;
const char *WiredTigerClient::cursor_bulk_config = "bulk";

WiredTigerClient::WiredTigerClient(WiredTigerFactory *factory, int id, const char *session_config, const char *cursor_config)
	: Client(id, factory) {
	if (session_config == nullptr)
		session_config = WiredTigerClient::session_default_config;
	if (cursor_config == nullptr)
		cursor_config = WiredTigerClient::cursor_default_config;
	this->session_config = session_config;
	this->cursor_config = cursor_config;

	int ret;
	ret = factory->conn->open_session(factory->conn, nullptr, this->session_config, &this->session);
	if (ret != 0) {
		fprintf(stderr, "WiredTigerClient: failed to open session, ret: %s\n", wiredtiger_strerror(ret));
		throw std::invalid_argument("failed to open session");
	}
	ret = this->session->open_cursor(this->session, factory->table_name, nullptr, this->cursor_config, &this->cursor);
	if (ret != 0) {
		fprintf(stderr, "WiredTigerClient: failed to open cursor, ret: %s\n", wiredtiger_strerror(ret));
		this->session->close(this->session, nullptr);
		throw std::invalid_argument("failed to open cursor");
	}
}

WiredTigerClient::~WiredTigerClient() {
	if (this->session != nullptr) {
		int ret = this->session->close(session, nullptr);
		if (ret != 0) {
			fprintf(stderr, "WiredTigerClient: session close failed\n");
		}
	}
}

int WiredTigerClient::do_operation(Operation *op) {
	switch (op->type) {
	case UPDATE:
		return this->do_update(op->key_buffer, op->value_buffer);
	case INSERT:
		return this->do_insert(op->key_buffer, op->value_buffer);
	case READ:
		return this->do_read(op->key_buffer, &op->reply_value_buffer);
	case SCAN:
		return this->do_scan(op->key_buffer, op->scan_length);
	case READ_MODIFY_WRITE:
		return this->do_read_modify_write(op->key_buffer, op->value_buffer);
	default:
		throw std::invalid_argument("invalid op type");
	}
}

int WiredTigerClient::do_read(char *key_buffer, char **value) {
	int ret;
	this->cursor->reset(cursor);
	this->cursor->set_key(cursor, key_buffer);
	ret = this->cursor->search(cursor);
	if (ret != 0) {
		fprintf(stderr, "WiredTigerClient: search failed, ret: %s\n", wiredtiger_strerror(ret));
		throw std::invalid_argument("search failed");
	}
	this->cursor->get_value(cursor, value);
	return ret;
}

int WiredTigerClient::do_update(char *key_buffer, char *value_buffer) {
	int ret;
	char *value;
	this->cursor->reset(cursor);
	this->cursor->set_key(cursor, key_buffer);
	this->cursor->set_value(cursor, value_buffer);
	ret = this->cursor->insert(cursor);
	if (ret != 0) {
		fprintf(stderr, "WiredTigerClient: insert failed, ret: %s\n", wiredtiger_strerror(ret));
		throw std::invalid_argument("insert failed");
	}
	return ret;
}

int WiredTigerClient::do_insert(char *key_buffer, char *value_buffer) {
	return this->do_update(key_buffer, value_buffer);
}

int WiredTigerClient::do_read_modify_write(char *key_buffer, char *value_buffer) {
	int ret;
	char *value;
	this->cursor->reset(cursor);
	/* read */
	this->cursor->set_key(cursor, key_buffer);
	ret = this->cursor->search(cursor);
	if (ret != 0) {
		fprintf(stderr, "WiredTigerClient: search failed, ret: %s\n", wiredtiger_strerror(ret));
		throw std::invalid_argument("search failed");
	}
	this->cursor->get_value(cursor, &value);
	/* write */
	this->cursor->set_value(cursor, value_buffer);
	ret = this->cursor->insert(cursor);
	if (ret != 0) {
		fprintf(stderr, "WiredTigerClient: insert failed, ret: %s\n", wiredtiger_strerror(ret));
		throw std::invalid_argument("insert failed");
	}
	return ret;
}

int WiredTigerClient::do_scan(char *key_buffer, long scan_length) {
	int ret;
	char *value;
	this->cursor->reset(cursor);
	this->cursor->set_key(cursor, key_buffer);
	ret = this->cursor->search(cursor);
	if (ret != 0) {
		fprintf(stderr, "WiredTigerClient: search failed, ret: %s\n", wiredtiger_strerror(ret));
		throw std::invalid_argument("search failed");
	}
	this->cursor->get_value(cursor, &value);
	for (long i = 0; i < scan_length - 1; ++i) {
		ret = this->cursor->next(cursor);
		if (ret != 0) {
			fprintf(stderr, "WiredTigerClient: next failed, ret: %s\n", wiredtiger_strerror(ret));
			throw std::invalid_argument("next failed");
		}
		this->cursor->get_value(cursor, &value);
	}
	return ret;
}

int WiredTigerClient::reset() {
	int ret = this->cursor->reset(cursor);
	if (ret) {
		fprintf(stderr, "WiredTigerClient: reset failed, ret: %s\n", wiredtiger_strerror(ret));
		throw std::invalid_argument("reset failed");
	}
	return ret;
}

void WiredTigerClient::close() {
	int ret = this->session->close(session, nullptr);
	if (ret != 0) {
		throw std::invalid_argument("close failed");
	}
	this->cursor = nullptr;
	this->session = nullptr;
}

const char *WiredTigerFactory::default_data_dir = "/home/yuhongyi/nvme0n1/tigerhome";
const char *WiredTigerFactory::default_table_name = "lsm:karaage";
const char *WiredTigerFactory::conn_default_config = "create,direct_io=[data,checkpoint],buffer_alignment=512B,mmap=false,"
	"cache_size=128M,eviction_trigger=95,eviction_target=80,"
	"eviction=(threads_max=6,threads_min=1)";
const char *WiredTigerFactory::create_table_default_config = "key_format=S,value_format=S,allocation_size=512B,"
	"internal_page_max=512B,leaf_page_max=512B";

WiredTigerFactory::WiredTigerFactory(const char *data_dir, const char *table_name, const char *conn_config,
				     const char *session_config, const char *cursor_config, bool new_table,
				     const char *create_table_config, bool print_stats)
	: client_id(0) {
	if (data_dir == nullptr)
		data_dir = WiredTigerFactory::default_data_dir;
	if (table_name == nullptr)
		table_name = WiredTigerFactory::default_table_name;
	if (conn_config == nullptr)
		conn_config = WiredTigerFactory::conn_default_config;
	if (new_table && create_table_config == nullptr)
		create_table_config = WiredTigerFactory::create_table_default_config;
	this->data_dir = data_dir;
	this->table_name = table_name;
	this->conn_config = conn_config;
	this->session_config = session_config;
	this->cursor_config = cursor_config;
	this->create_table_config = create_table_config;
	this->print_stats = print_stats;

	int ret;
	ret = wiredtiger_open(this->data_dir, nullptr, this->conn_config, &this->conn);
	if (ret != 0) {
		printf("WiredTigerFactory: wiredtiger_open failed, ret: %s\n", wiredtiger_strerror(ret));
		throw std::invalid_argument("wiredtiger_open failed");
	}
	if (new_table) {
		/* drop existing table and create a new one */
		WT_SESSION *session;
		ret = conn->open_session(conn, nullptr, nullptr, &session);
		if (ret != 0) {
			fprintf(stderr, "WiredTigerFactory: open_session failed, ret: %s\n", wiredtiger_strerror(ret));
			throw std::invalid_argument("open_session failed");
		}
		session->drop(session, this->table_name, nullptr);
		ret = session->create(session, this->table_name, this->create_table_config);
		if (ret != 0) {
			fprintf(stderr, "WiredTigerFactory: create table failed, ret: %s\n", wiredtiger_strerror(ret));
			throw std::invalid_argument("create table failed");
		}
		session->close(session, nullptr);
	}
}

int WiredTigerFactory::print_cursor(WT_CURSOR *cursor)
{
	const char *desc, *pvalue;
	uint64_t value;
	int ret;
	while ((ret = cursor->next(cursor)) == 0 &&
	       (ret = cursor->get_value(cursor, &desc, &pvalue, &value)) == 0)
		if (value != 0)
			printf("%s=%s\n", desc, pvalue);
	return (ret == WT_NOTFOUND ? 0 : ret);
}

WiredTigerFactory::~WiredTigerFactory() {
	WT_SESSION *session;
	WT_CURSOR *cursor;
	int ret;

	if (this->print_stats) {
		ret = this->conn->open_session(this->conn, nullptr, this->session_config, &session);
		if (ret != 0) {
			fprintf(stderr, "WiredTigerFactory: failed to open stat session, ret: %s\n", wiredtiger_strerror(ret));
			throw std::invalid_argument("failed to open stat session");
		}
		ret = session->open_cursor(session, "statistics:", NULL, "statistics=(fast)", &cursor);
		if (ret != 0) {
			fprintf(stderr, "WiredTigerFactory: failed to open stat cursor, ret: %s\n", wiredtiger_strerror(ret));
			throw std::invalid_argument("failed to open stat cursor");
		}
		print_cursor(cursor);
		ret = cursor->close(cursor);
		if (ret != 0) {
			fprintf(stderr, "WiredTigerFactory: failed to close stat cursor, ret: %s\n", wiredtiger_strerror(ret));
			throw std::invalid_argument("failed to close stat cursor");
		}
		ret = session->close(session, NULL);
		if (ret != 0) {
			fprintf(stderr, "WiredTigerFactory: failed to close stat session, ret: %s\n", wiredtiger_strerror(ret));
			throw std::invalid_argument("failed to close stat session");
		}
	}
	this->conn->close(this->conn, NULL);
}

void WiredTigerFactory::update_session_config(const char *new_session_config) {
	this->session_config = new_session_config;
}

void WiredTigerFactory::update_cursor_config(const char *new_cursor_config) {
	this->cursor_config = new_cursor_config;
}

WiredTigerClient * WiredTigerFactory::create_client() {
	return new WiredTigerClient(this, this->client_id++, this->session_config, this->cursor_config);
}

void WiredTigerFactory::destroy_client(Client *client) {
	WiredTigerClient *wt_client = (WiredTigerClient *)client;
	wt_client->close();
	delete wt_client;
}
