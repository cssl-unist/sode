#ifndef YCSB_CLIENT_H
#define YCSB_CLIENT_H

#include <atomic>
#include <stdexcept>
#include "workload.h"

struct ClientFactory;

struct Client {
	int id;
	ClientFactory *factory;

	Client(int id, ClientFactory *factory);
	virtual int do_operation(Operation *op) {
		throw std::invalid_argument("not implemented");
	}
	virtual int reset() = 0;
	virtual void close() = 0;
};

struct ClientFactory {
	virtual Client *create_client() = 0;
	virtual void destroy_client(Client *client) = 0;
};

#endif //YCSB_CLIENT_H
