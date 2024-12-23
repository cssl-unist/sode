#ifndef YCSB_MEMCACHED_CLIENT_H
#define YCSB_MEMCACHED_CLIENT_H

#include <cstring>
#include <libmemcached/memcached.h>
#include "client.h"

#define MEMCACHED_MAX_CONFIG_LEN 256

struct MemcachedFactory;

struct MemcachedClient : public Client {
	memcached_st *memcached_context;
	char *last_reply;

	MemcachedClient(MemcachedFactory *factory, int id);
	~MemcachedClient();
	int do_operation(Operation *op) override;
	int reset() override;
	void close() override;

private:
	int do_update(Operation *op);
	int do_read(Operation *op);
	void set_last_reply(char *reply);
};

struct MemcachedFactory : public ClientFactory {
	const char *memcached_addr;
	const int memcached_port;
	std::atomic<int> client_id;

	MemcachedFactory(const char *memcached_addr, int memcached_port);
	MemcachedClient *create_client() override;
	void destroy_client(Client *client) override;
};

#endif //YCSB_MEMCACHED_CLIENT_H
