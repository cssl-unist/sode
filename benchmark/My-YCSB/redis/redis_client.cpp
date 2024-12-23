#include "redis_client.h"

RedisClient::RedisClient(RedisFactory *factory, int id)
: Client(id, factory), redis_context(nullptr), last_reply(nullptr), batch_size(factory->batch_size), cur_batch(0) {
	this->redis_context = redisConnect(factory->redis_addr, factory->redis_port);
	if (this->redis_context == nullptr || this->redis_context->err) {
		if (this->redis_context) {
			fprintf(stderr, "RedisClient: redisConnect error %s\n", this->redis_context->errstr);
		} else {
			fprintf(stderr, "RedisClient: cannot allocate redis context\n");
		}
		throw std::invalid_argument("failed to open redis_context");
	}
}

RedisClient::~RedisClient() {
	if (this->last_reply != nullptr)
		freeReplyObject(this->last_reply);
	if (this->redis_context)
		redisFree(this->redis_context);
}

int RedisClient::do_operation(Operation *op) {
	switch (op->type) {
	case READ:
		return this->do_read(op->key_buffer, &op->reply_value_buffer, op->is_last_op);
	case INSERT:
		return this->do_insert(op->key_buffer, op->value_buffer, op->is_last_op);
	case UPDATE:
		return this->do_update(op->key_buffer, op->value_buffer, op->is_last_op);
	default:
		throw std::invalid_argument("invalid op type");
	}
}

int RedisClient::do_update(char *key_buffer, char *value_buffer, bool is_last_op) {
	if (this->batch_size == 1) {
		redisReply *reply = (redisReply *)redisCommand(this->redis_context, "SET %s %s", key_buffer, value_buffer);
		if (!reply) {
			fprintf(stderr, "RedisClient: SET error: %s\n", this->redis_context->errstr);
			throw std::invalid_argument("failed to SET");
		}
		this->set_last_reply(reply);
	} else {
		redisAppendCommand(this->redis_context, "SET %s %s", key_buffer, value_buffer);
		++this->cur_batch;
		if (this->cur_batch >= this->batch_size || is_last_op) {
			while (this->cur_batch > 0) {
				redisReply *reply;
				int ret = redisGetReply(this->redis_context, (void **) &reply);
				if (ret != REDIS_OK) {
					fprintf(stderr, "RedisClient: redisGetReply error: %s\n", this->redis_context->errstr);
					throw std::invalid_argument("redisGetReply failed");
				}
				this->set_last_reply(reply);
				--this->cur_batch;
			}
		}
	}
	return 0;
}

int RedisClient::do_insert(char *key_buffer, char *value_buffer, bool is_last_op) {
	return this->do_update(key_buffer, value_buffer, is_last_op);
}

int RedisClient::do_read(char *key_buffer, char **value, bool is_last_op) {
	if (this->batch_size == 1) {
		redisReply *reply = (redisReply *)redisCommand(this->redis_context, "GET %s", key_buffer);
		if (!reply) {
			fprintf(stderr, "RedisClient: GET error: %s\n", this->redis_context->errstr);
			throw std::invalid_argument("failed to GET");
		}
		*value = reply->str;
		this->set_last_reply(reply);
	} else {
		redisAppendCommand(this->redis_context, "GET %s", key_buffer);
		++this->cur_batch;
		if (this->cur_batch >= this->batch_size || is_last_op) {
			while (this->cur_batch > 0) {
				redisReply *reply;
				int ret = redisGetReply(this->redis_context, (void **) &reply);
				if (ret != REDIS_OK) {
					fprintf(stderr, "RedisClient: redisGetReply error: %s\n", this->redis_context->errstr);
					throw std::invalid_argument("redisGetReply failed");
				}
				*value = reply->str;
				this->set_last_reply(reply);
				--this->cur_batch;
			}
		}
	}
	return 0;
}

int RedisClient::reset() {
	return 0;
}

void RedisClient::close() {
	if (this->last_reply)
		freeReplyObject(this->last_reply);
	this->last_reply = nullptr;
	if (this->redis_context)
		redisFree(this->redis_context);
	this->redis_context = nullptr;
}

void RedisClient::set_last_reply(redisReply *reply) {
	if (this->last_reply)
		freeReplyObject(this->last_reply);
	this->last_reply = reply;
}

RedisFactory::RedisFactory(const char *redis_addr, int redis_port, int batch_size)
: redis_addr(redis_addr), redis_port(redis_port), client_id(0), batch_size(batch_size) {
	;
}

RedisClient *RedisFactory::create_client() {
	RedisClient *client = new RedisClient(this, this->client_id++);
	return client;
}

void RedisFactory::destroy_client(Client *client) {
	RedisClient *redis_client = (RedisClient *) client;
	redis_client->close();
	delete redis_client;
}
