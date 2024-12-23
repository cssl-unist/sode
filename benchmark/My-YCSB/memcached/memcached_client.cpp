#include "memcached_client.h"

MemcachedClient::MemcachedClient(MemcachedFactory *factory, int id)
: Client(id, factory), memcached_context(nullptr), last_reply(nullptr) {
	char config[MEMCACHED_MAX_CONFIG_LEN];
	int ret = sprintf(config, "--SERVER=%s:%d", factory->memcached_addr, factory->memcached_port);
	if (ret < 0) {
		fprintf(stderr, "MemcachedClient: failed to generate config string with addr %s and port %d\n",
		        factory->memcached_addr, factory->memcached_port);
		throw std::invalid_argument("failed to generate config string");
	}
	this->memcached_context = memcached(config, strlen(config));
	if (this->memcached_context == nullptr) {
		fprintf(stderr, "MemcachedClient: failed to create memcached context with addr %s and port %d\n",
		        factory->memcached_addr, factory->memcached_port);
		throw std::invalid_argument("failed to create memcached context");
	}
}

MemcachedClient::~MemcachedClient() {
	if (this->last_reply != nullptr)
		free(this->last_reply);
	if (this->memcached_context != nullptr)
		memcached_free(this->memcached_context);
}

int MemcachedClient::do_operation(Operation *op) {
	switch (op->type) {
	case READ:
		return this->do_read(op);
	case INSERT:
	case UPDATE:
		return this->do_update(op);
	default:
		throw std::invalid_argument("invalid op type");
	}
}

int MemcachedClient::do_read(Operation *op) {
	size_t value_length;
	uint32_t flags;
	memcached_return_t rc;
	char *value = memcached_get(this->memcached_context,
	                            op->key_buffer,
	                            strlen(op->key_buffer),
	                            &value_length,
	                            &flags,
	                            &rc);
	if (value == nullptr) {
		fprintf(stderr, "MemcachedClient: READ error: %s\n",
		        memcached_strerror(this->memcached_context, rc));
		throw std::invalid_argument("failed to READ");
	}
	op->reply_value_buffer = value;
	this->set_last_reply(value);
	return 0;
}

int MemcachedClient::do_update(Operation *op) {
	memcached_return_t rc;
	rc = memcached_set(this->memcached_context, op->key_buffer, strlen(op->key_buffer),
	                   op->value_buffer, strlen(op->value_buffer), 0, 0);
	if (rc != MEMCACHED_SUCCESS) {
		fprintf(stderr, "MemcachedClient: SET error: %s\n",
		        memcached_strerror(this->memcached_context, rc));
		throw std::invalid_argument("failed to SET");
	}
	return 0;
}

int MemcachedClient::reset() {
	return 0;
}

void MemcachedClient::close() {
	if (this->last_reply != nullptr)
		free(this->last_reply);
	this->last_reply = nullptr;
	if (this->memcached_context != nullptr)
		memcached_free(this->memcached_context);
	this->memcached_context = nullptr;
}

void MemcachedClient::set_last_reply(char *reply) {
	if (this->last_reply)
		free(this->last_reply);
	this->last_reply = reply;
}

MemcachedFactory::MemcachedFactory(const char *memcached_addr, int memcached_port)
: memcached_addr(memcached_addr), memcached_port(memcached_port), client_id(0) {
	;
}

MemcachedClient *MemcachedFactory::create_client() {
	MemcachedClient *client = new MemcachedClient(this, this->client_id++);
	return client;
}

void MemcachedFactory::destroy_client(Client *client) {
	MemcachedClient *memcached_client = (MemcachedClient *) client;
	memcached_client->close();
	delete memcached_client;
}
