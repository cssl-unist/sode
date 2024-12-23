#include "measurement.h"

OpMeasurement::OpMeasurement() {
	for (int i = 0; i < NR_OP_TYPE; ++i) {
		this->op_count_arr[i] = 0;
		this->rt_op_count_arr[i] = 0;
	}
	this->cur_progress = 0;
	this->finished = false;
	this->final_result_lock.lock();
	this->nr_active_client = 0;
}

void OpMeasurement::enable_client(int client_id) {
	/* trigger unordered_map allocation for client_id */
	this->per_client_latency_vec[client_id][0].clear();
	this->per_client_timestamp_vec[client_id][0].clear();
}

void OpMeasurement::set_max_progress(long new_max_progress) {
	this->max_progress = new_max_progress;
}

void OpMeasurement::start_measure() {
	int total_nr_client = this->per_client_latency_vec.size();
	int prev_nr_active_client = this->nr_active_client.fetch_add(1);
	if (prev_nr_active_client == total_nr_client - 1) {
		this->start_time = std::chrono::steady_clock::now();
		this->rt_time = std::chrono::steady_clock::now();
	}
}

void OpMeasurement::finish_measure() {
	int total_nr_client = this->per_client_latency_vec.size();
	int prev_nr_active_client = this->nr_active_client.fetch_sub(1);
	if (prev_nr_active_client == total_nr_client) {
		this->finished.store(true);
		this->end_time = std::chrono::steady_clock::now();
	}
}

void OpMeasurement::finalize_measure() {
	for (int i = 0; i < NR_OP_TYPE; ++i) {
		for (const auto& client_vec : this->per_client_latency_vec) {
			this->final_latency_vec[i].insert(this->final_latency_vec[i].end(), client_vec.second[i].begin(), client_vec.second[i].end());
		}
		std::sort(this->final_latency_vec[i].begin(), this->final_latency_vec[i].end());
	}
	this->final_result_lock.unlock();
}

void OpMeasurement::record_op(OperationType type, double latency, int id) {
	if (this->per_client_latency_vec.size() != this->nr_active_client.load())
		return;
	long duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
		std::chrono::steady_clock::now() - this->start_time
	).count();
	++this->op_count_arr[type];
	++this->rt_op_count_arr[type];
	this->per_client_latency_vec[id][type].push_back(latency);
	this->per_client_timestamp_vec[id][type].push_back(duration);
}

void OpMeasurement::record_progress(long progress_delta) {
	this->cur_progress += progress_delta;
}

long OpMeasurement::get_op_count(OperationType type) {
	return this->op_count_arr[type];
}

double OpMeasurement::get_throughput(OperationType type) {
	long duration = std::chrono::duration_cast<std::chrono::microseconds>(
		this->end_time - this->start_time
	).count();
	return ((double) this->op_count_arr[type]) * 1000000 / duration;
}

void OpMeasurement::get_rt_throughput(double *throughput_arr) {
	if (this->per_client_latency_vec.size() != this->nr_active_client.load()) {
		/* not all the clients have started */
		for (int i = 0; i < NR_OP_TYPE; ++i) {
			throughput_arr[i] = 0;
		}
		return;
	}
	std::chrono::steady_clock::time_point cur_time = std::chrono::steady_clock::now();
	long duration = std::chrono::duration_cast<std::chrono::microseconds>(
		cur_time - this->rt_time
	).count();
	for (int i = 0; i < NR_OP_TYPE; ++i) {
		throughput_arr[i] = ((double) this->rt_op_count_arr[i].exchange(0)) * 1000000 / duration;
	}
	this->rt_time = cur_time;
}

double OpMeasurement::get_progress_percent() {
	return ((double) this->cur_progress) / ((double) this->max_progress);
}

double OpMeasurement::get_latency_average(OperationType type) {
	if (this->final_latency_vec[type].empty()) {
		return 0;
	}
	double latency_sum = std::accumulate(this->final_latency_vec[type].begin(), this->final_latency_vec[type].end(), 0.0);
	return latency_sum / (double) this->final_latency_vec[type].size();
}

double OpMeasurement::get_latency_percentile(OperationType type, float percentile) {
	if (this->final_latency_vec[type].empty()) {
		return 0;
	}
	double exact_index = (double)(this->final_latency_vec[type].size() - 1) * percentile;
	double left_index = floor(exact_index);
	double right_index = ceil(exact_index);

	double left_value = this->final_latency_vec[type][(long) left_index];
	double right_value = this->final_latency_vec[type][(long) right_index];

	double value = left_value + (exact_index - left_index) * (right_value - left_value);
	return value;
}

void OpMeasurement::save_latency(const char *path) {
	FILE *file = fopen(path, "w");
	if (file == nullptr) {
		fprintf(stderr, "OpMeasurement: failed to open latency file %s\n",
		        path);
		throw std::invalid_argument("failed to open latency file");
	}
	fprintf(file, "Timestamp (ns),Client ID,Operation,Latency (ns)\n");
	for (auto& client_it : this->per_client_latency_vec) {
		int id = client_it.first;
		for (int op = 0; op < NR_OP_TYPE; ++op) {
			for (size_t i = 0; i < this->per_client_latency_vec[id][op].size(); ++i) {
				fprintf(file, "%ld,%d,%s,%f\n",
				        this->per_client_timestamp_vec[id][op][i],
				        id,
				        operation_type_name[op],
				        this->per_client_latency_vec[id][op][i]);
			}
		}
	}
	fclose(file);
}
