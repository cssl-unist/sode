#include "workload.h"

const char* operation_type_name[] = {
	"UPDATE", "INSERT", "READ", "SCAN", "READ_MODIFY_WRITE"
};

Workload::Workload(long key_size, long value_size)
: key_size(key_size), value_size(value_size) {
	;
}

long Workload::generate_random_long(unsigned int *seedp) {
	return (((long)rand_r(seedp)) << (sizeof(int) * 8)) | rand_r(seedp);
}

double Workload::generate_random_double(unsigned int *seedp) {
	return ((double)rand_r(seedp)) / RAND_MAX;
}

UniformWorkload::UniformWorkload(long key_size, long value_size, long scan_length, long nr_entry,
                                 long nr_op, struct OpProportion op_prop, unsigned int seed)
: Workload(key_size, value_size), scan_length(scan_length), nr_entry(nr_entry), nr_op(nr_op), op_prop(op_prop), seed(seed), cur_nr_op(0) {
	sprintf(this->key_format, "%%0%ldld", key_size - 1);
}

bool UniformWorkload::has_next_op() {
	return this->cur_nr_op < this->nr_op;
}

void UniformWorkload::next_op(Operation *op) {
	if (!this->has_next_op())
		throw std::invalid_argument("does not have next op");
	double op_random = this->generate_random_double(&this->seed);
	double running_sum = 0;
	if (running_sum += this->op_prop.op[UPDATE], op_random <= running_sum) {
		op->type = UPDATE;
		this->generate_value_string(op->value_buffer);
	} else if (running_sum += this->op_prop.op[INSERT], op_random <= running_sum) {
		op->type = INSERT;
		this->generate_value_string(op->value_buffer);
	} else if (running_sum += this->op_prop.op[READ], op_random <= running_sum) {
		op->type = READ;
	} else if (running_sum += this->op_prop.op[SCAN], op_random <= running_sum) {
		op->type = SCAN;
		op->scan_length = this->scan_length;
	} else if (running_sum += this->op_prop.op[READ_MODIFY_WRITE], op_random <= running_sum) {
		op->type = READ_MODIFY_WRITE;
		this->generate_value_string(op->value_buffer);
	} else {
		throw std::invalid_argument("failed to generate an operation");
	}
	long key = this->generate_random_long(&this->seed) % this->nr_entry;
	this->generate_key_string(op->key_buffer, key);
	++this->cur_nr_op;
	op->is_last_op = !this->has_next_op();
}

void UniformWorkload::generate_key_string(char *key_buffer, long key) {
	sprintf(key_buffer, this->key_format, key);
}

void UniformWorkload::generate_value_string(char *value_buffer) {
	for (int i = 0; i < this->value_size - 1; ++i) {
		value_buffer[i] = 'a' + (rand_r(&this->seed) % ('z' - 'a' + 1));
	}
	value_buffer[this->value_size - 1] = '\0';
}

ZipfianWorkload::ZipfianWorkload(long key_size, long value_size, long scan_length, long nr_entry, long nr_op,
                                 struct OpProportion op_prop, double zipfian_constant, unsigned int seed)
: Workload(key_size, value_size), scan_length(scan_length), nr_entry(nr_entry), nr_op(nr_op), op_prop(op_prop),
  zipfian_constant(zipfian_constant), seed(seed), cur_nr_op(0) {
	sprintf(this->key_format, "%%0%ldlu", key_size - 1);

	/* zipfian-related initialization */
	this->zetan = 0;
	for (long i = 1; i < this->nr_entry + 1; ++i) {
		this->zetan += 1.0 / (pow((double) i, this->zipfian_constant));
	}
	this->theta = this->zipfian_constant;
	this->zeta2theta = 0;
	for (long i = 1; i < 3; ++i) {
		this->zeta2theta += 1.0 / (pow((double) i, this->zipfian_constant));
	}
	this->alpha = 1.0 / (1.0 - this->theta);
	this->eta = (1 - pow(2.0 / (double) this->nr_entry, 1 - this->theta))
	            / (1 - (this->zeta2theta / this->zetan));
	this->generate_zipfian_random_ulong(true);
}

bool ZipfianWorkload::has_next_op() {
	return this->cur_nr_op < this->nr_op;
}

void ZipfianWorkload::next_op(Operation *op) {
	if (!this->has_next_op())
		throw std::invalid_argument("does not have next op");
	double op_random = this->generate_random_double(&this->seed);
	double running_sum = 0;
	if (running_sum += this->op_prop.op[UPDATE], op_random <= running_sum) {
		op->type = UPDATE;
		this->generate_value_string(op->value_buffer);
	} else if (running_sum += this->op_prop.op[INSERT], op_random <= running_sum) {
		op->type = INSERT;
		this->generate_value_string(op->value_buffer);
	} else if (running_sum += this->op_prop.op[READ], op_random <= running_sum) {
		op->type = READ;
	} else if (running_sum += this->op_prop.op[SCAN], op_random <= running_sum) {
		op->type = SCAN;
		op->scan_length = this->scan_length;
	} else if (running_sum += this->op_prop.op[READ_MODIFY_WRITE], op_random <= running_sum) {
		op->type = READ_MODIFY_WRITE;
		this->generate_value_string(op->value_buffer);
	} else {
		throw std::invalid_argument("failed to generate an operation");
	}
	unsigned long key = this->generate_zipfian_random_ulong(true) % ((unsigned long) this->nr_entry);
	this->generate_key_string(op->key_buffer, key);
	++this->cur_nr_op;
	op->is_last_op = !this->has_next_op();
}

ZipfianWorkload * ZipfianWorkload::clone(unsigned int new_seed) {
	/* create a new ZipfianWorkload with a cheap nr_entry */
	ZipfianWorkload *copy = new ZipfianWorkload(this->key_size, this->value_size, this->scan_length, 3, this->nr_op,
	                                            this->op_prop, this->zipfian_constant, new_seed);
	copy->zetan = this->zetan;
	copy->theta = this->theta;
	copy->zeta2theta = this->zeta2theta;
	copy->alpha = this->alpha;
	copy->eta = this->eta;
	copy->nr_entry = this->nr_entry;
	return copy;
}

unsigned long ZipfianWorkload::fnv1_64_hash(unsigned long value) {
	uint64_t hash = 14695981039346656037ul;
	uint8_t *p = (uint8_t *) &value;
	for (int i = 0; i < sizeof(unsigned long); ++i, ++p) {
		hash *= 1099511628211ul;
		hash ^= *p;
	}
	return (unsigned long) hash;
}

unsigned long ZipfianWorkload::generate_zipfian_random_ulong(bool hash) {
	double u = this->generate_random_double(&this->seed);
	double uz = u * this->zetan;
	if (uz < 1)
		return 0;
	if (uz < 1 + pow(0.5, this->theta))
		return 1;
	unsigned long ret = (unsigned long) ((double)this->nr_entry * pow(this->eta * u - this->eta + 1, this->alpha));
	if (hash)
		return ZipfianWorkload::fnv1_64_hash(ret);
	else
		return ret;
}

void ZipfianWorkload::generate_key_string(char *key_buffer, unsigned long key) {
	sprintf(key_buffer, this->key_format, key);
}

void ZipfianWorkload::generate_value_string(char *value_buffer) {
	for (int i = 0; i < this->value_size - 1; ++i) {
		value_buffer[i] = 'a' + (rand_r(&this->seed) % ('z' - 'a' + 1));
	}
	value_buffer[this->value_size - 1] = '\0';
}

InitWorkload::InitWorkload(long nr_entry, long start_key, long key_size, long value_size, unsigned int seed)
: Workload(key_size, value_size), nr_entry(nr_entry), start_key(start_key), cur_nr_entry(0), seed(seed) {
	sprintf(this->key_format, "%%0%ldld", key_size - 1);
}

bool InitWorkload::has_next_op() {
	return this->cur_nr_entry < this->nr_entry;
}

void InitWorkload::next_op(Operation *op) {
	if (!this->has_next_op())
		throw std::invalid_argument("does not have next op");
	op->type = INSERT;
	this->generate_key_string(op->key_buffer, this->start_key + this->cur_nr_entry++);
	this->generate_value_string(op->value_buffer);
	op->is_last_op = !this->has_next_op();
}

void InitWorkload::generate_key_string(char *key_buffer, long key) {
	sprintf(key_buffer, this->key_format, key);
}

void InitWorkload::generate_value_string(char *value_buffer) {
	for (int i = 0; i < this->value_size - 1; ++i) {
		value_buffer[i] = 'a' + (rand_r(&this->seed) % ('z' - 'a' + 1));
	}
	value_buffer[this->value_size - 1] = '\0';
}

LatestWorkload::LatestWorkload(long key_size, long value_size, long nr_entry, long nr_op, double read_ratio,
                               double zipfian_constant, unsigned int seed)
	: Workload(key_size, value_size), nr_entry(nr_entry), nr_op(nr_op), read_ratio(read_ratio),
	  zipfian_constant(zipfian_constant), seed(seed), cur_nr_op(0), cur_ack_key(0) {
	sprintf(this->key_format, "%%0%ldlu", key_size - 1);

	/* zipfian-related initialization */
	this->zetan = 0;
	for (long i = 1; i < this->nr_entry + 1; ++i) {
		this->zetan += 1.0 / (pow((double) i, this->zipfian_constant));
	}
	this->theta = this->zipfian_constant;
	this->zeta2theta = 0;
	for (long i = 1; i < 3; ++i) {
		this->zeta2theta += 1.0 / (pow((double) i, this->zipfian_constant));
	}
	this->alpha = 1.0 / (1.0 - this->theta);
	this->eta = (1 - pow(2.0 / (double) this->nr_entry, 1 - this->theta))
		    / (1 - (this->zeta2theta / this->zetan));
	this->generate_zipfian_random_ulong(true);
}

bool LatestWorkload::has_next_op() {
	return this->cur_nr_op < this->nr_op;
}

void LatestWorkload::next_op(Operation *op) {
	if (!this->has_next_op())
		throw std::invalid_argument("does not have next op");
	bool read = this->generate_random_double(&this->seed) <= this->read_ratio;
	if (this->cur_ack_key == 0) {
		read = false;
	}
	unsigned long key;
	if (read) {
		op->type = READ;
		key = this->generate_zipfian_random_ulong(false) % (this->cur_ack_key);
		key = this->cur_ack_key - key - 1;
	} else {
		if (this->cur_ack_key >= this->nr_entry) {
			op->type = UPDATE;
			key = this->generate_zipfian_random_ulong(false) % (this->cur_ack_key);
			key = this->cur_ack_key - key - 1;
		} else {
			op->type = INSERT;
			key = this->cur_ack_key++;
		}
	}
	key = LatestWorkload::fnv1_64_hash(key) % ((unsigned long) this->nr_entry);
	this->generate_key_string(op->key_buffer, key);
	if (!read)
		this->generate_value_string(op->value_buffer);
	++this->cur_nr_op;
	op->is_last_op = !this->has_next_op();
}

LatestWorkload * LatestWorkload::clone(unsigned int new_seed) {
	/* create a new ZipfianWorkload with a cheap nr_entry */
	LatestWorkload *copy = new LatestWorkload(this->key_size, this->value_size, 3, this->nr_op,
						  this->read_ratio, this->zipfian_constant, new_seed);
	copy->zetan = this->zetan;
	copy->theta = this->theta;
	copy->zeta2theta = this->zeta2theta;
	copy->alpha = this->alpha;
	copy->eta = this->eta;
	copy->nr_entry = this->nr_entry;
	return copy;
}

unsigned long LatestWorkload::fnv1_64_hash(unsigned long value) {
	uint64_t hash = 14695981039346656037ul;
	uint8_t *p = (uint8_t *) &value;
	for (int i = 0; i < sizeof(unsigned long); ++i, ++p) {
		hash *= 1099511628211ul;
		hash ^= *p;
	}
	return (unsigned long) hash;
}

unsigned long LatestWorkload::generate_zipfian_random_ulong(bool hash) {
	double u = this->generate_random_double(&this->seed);
	double uz = u * this->zetan;
	if (uz < 1)
		return 0;
	if (uz < 1 + pow(0.5, this->theta))
		return 1;
	unsigned long ret = (unsigned long) ((double)this->nr_entry * pow(this->eta * u - this->eta + 1, this->alpha));
	if (hash)
		return LatestWorkload::fnv1_64_hash(ret);
	else
		return ret;
}

void LatestWorkload::generate_key_string(char *key_buffer, unsigned long key) {
	sprintf(key_buffer, this->key_format, key);
}

void LatestWorkload::generate_value_string(char *value_buffer) {
	for (int i = 0; i < this->value_size - 1; ++i) {
		value_buffer[i] = 'a' + (rand_r(&this->seed) % ('z' - 'a' + 1));
	}
	value_buffer[this->value_size - 1] = '\0';
}

TraceWorkload::TraceWorkload(long key_size, long value_size, long nr_op, std::string trace_path, unsigned int seed)
: Workload(key_size, value_size), nr_op(nr_op), trace_path(trace_path), cur_nr_op(0), seed(seed) {
	this->trace_file.open(this->trace_path);
	if (!this->trace_file.is_open())
		throw std::invalid_argument("unable to open trace file");
	for (long i = 0; i < nr_op; ++i) {
		std::string line;
		if (!std::getline(this->trace_file, line))
			throw std::invalid_argument("failed to get the next line from the trace file");
		this->line_list.push_back(line);
	}
	this->trace_file.close();
}

bool TraceWorkload::has_next_op() {
	return this->cur_nr_op < this->nr_op;
}

void TraceWorkload::next_op(Operation *op) {
	if (!this->has_next_op())
		throw std::invalid_argument("does not have next op");

	std::string line = this->line_list.front();
	this->line_list.pop_front();

	/* trace file format: 
	 * [UPDATE/READ/READ_MODIFY_WRITE],[key string]
	 * SCAN,[start key string],[scan length]
	 */
	std::stringstream line_stream(line);
	std::string op_type, key;
	if (!std::getline(line_stream, op_type, ','))
		throw std::invalid_argument("failed to get the op type");
	if (!std::getline(line_stream, key, ','))
		throw std::invalid_argument("failed to get the key");

	if (op_type == "UPDATE") {
		op->type = UPDATE;
		this->generate_value_string(op->value_buffer);
	} else if (op_type == "INSERT") {
		op->type = INSERT;
		this->generate_value_string(op->value_buffer);
	} else if (op_type == "READ") {
		op->type = READ;
	} else if (op_type == "SCAN") {
		std::string scan_length;
		if (!std::getline(line_stream, scan_length, ','))
			throw std::invalid_argument("failed to get the scan length");
		op->type = SCAN;
		op->scan_length = std::stol(scan_length, nullptr, 10);
	} else if (op_type == "READ_MODIFY_WRITE") {
		op->type = READ_MODIFY_WRITE;
		this->generate_value_string(op->value_buffer);
	} else {
		throw std::invalid_argument("invalid trace operation");
	}
	strcpy(op->key_buffer, key.c_str());
	++this->cur_nr_op;
	op->is_last_op = !this->has_next_op();
}

void TraceWorkload::generate_value_string(char *value_buffer) {
	for (int i = 0; i < this->value_size - 1; ++i) {
		value_buffer[i] = 'a' + (rand_r(&this->seed) % ('z' - 'a' + 1));
	}
	value_buffer[this->value_size - 1] = '\0';
}
