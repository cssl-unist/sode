#include "worker.h"

void worker_thread_fn(Client *client, Workload *workload, OpMeasurement *measurement, long next_op_interval_ns) {
	Operation op;
	op.key_buffer = new char[workload->key_size];
	op.value_buffer = new char[workload->value_size];
	std::chrono::steady_clock::time_point start_time, finish_time;
	std::chrono::steady_clock::time_point next_op_time = std::chrono::steady_clock::now();

	measurement->start_measure();
	while (workload->has_next_op()) {
		workload->next_op(&op);
		while (std::chrono::steady_clock::now() < next_op_time) {
			/* busy waiting */
		}
		start_time = std::chrono::steady_clock::now();
		client->do_operation(&op);
		finish_time = std::chrono::steady_clock::now();
		long latency = std::chrono::duration_cast<std::chrono::nanoseconds>(finish_time - start_time).count();
		measurement->record_op(op.type, (double) latency, client->id);
		measurement->record_progress(1);
		next_op_time += std::chrono::nanoseconds(next_op_interval_ns);
	}
	measurement->finish_measure();
	client->reset();
	delete[] op.key_buffer;
	delete[] op.value_buffer;
}

void monitor_thread_fn(const char *task, OpMeasurement *measurement) {
	double rt_throughput[NR_OP_TYPE];
	double progress;
	long epoch = 0;
	for (;!measurement->finished
	     ;std::this_thread::sleep_for(std::chrono::seconds(1)), ++epoch) {
		measurement->get_rt_throughput(rt_throughput);
		progress = measurement->get_progress_percent();
		printf("%s (epoch %ld, progress %.2f%%): ", task, epoch, 100 * progress);
		double total_throughput = 0;
		for (int i = 0; i < NR_OP_TYPE; ++i) {
			printf("%s throughput %.2lf ops/sec, ", operation_type_name[i], rt_throughput[i]);
			total_throughput += rt_throughput[i];
		}
		printf("total throughput %.2lf ops/sec\n", total_throughput);
		std::cout << std::flush;
	}
	printf("%s: calculating overall performance metrics... (might take a while)\n", task);
	std::cout << std::flush;
	measurement->final_result_lock.lock();

	/* print throughput */
	printf("%s overall: ", task);
	double total_throughput = 0;
	for (int i = 0; i < NR_OP_TYPE; ++i) {
		printf("%s throughput %.2lf ops/sec, ", operation_type_name[i], measurement->get_throughput((OperationType) i));
		total_throughput += measurement->get_throughput((OperationType) i);
	}
	printf("total throughput %.2lf ops/sec\n", total_throughput);

	/* print latency */
	printf("%s overall: ", task);
	for (int i = 0; i < NR_OP_TYPE; ++i) {
		printf("%s average latency %.2lf ns, ", operation_type_name[i], measurement->get_latency_average((OperationType) i));
		printf("%s p99 latency %.2lf ns", operation_type_name[i], measurement->get_latency_percentile((OperationType) i, 0.99));
		if (i != NR_OP_TYPE - 1)
			printf(", ");
	}
	printf("\n");
	measurement->final_result_lock.unlock();
	std::cout << std::flush;
}

void run_workload_with_op_measurement(const char *task, ClientFactory *factory, Workload **workload_arr, int nr_thread, long nr_op, long max_progress,
                                      long next_op_interval_ns, const char *latency_file) {
	/* allocate resources */
	Client **client_arr = new Client *[nr_thread];
	std::thread **thread_arr = new std::thread *[nr_thread];
	OpMeasurement measurement;
	for (int thread_index = 0; thread_index < nr_thread; ++thread_index) {
		client_arr[thread_index] = factory->create_client();
		measurement.enable_client(client_arr[thread_index]->id);
	}

	/* start running workload */
	measurement.set_max_progress(max_progress);
	for (int thread_index = 0; thread_index < nr_thread; ++thread_index) {
		thread_arr[thread_index] = new std::thread(worker_thread_fn, client_arr[thread_index], workload_arr[thread_index], &measurement, next_op_interval_ns);
	}
	std::thread stat_thread(monitor_thread_fn, task, &measurement);
	for (int thread_index = 0; thread_index < nr_thread; ++thread_index) {
		thread_arr[thread_index]->join();
	}
	measurement.finalize_measure();
	if (latency_file != nullptr)
		measurement.save_latency(latency_file);
	stat_thread.join();

	/* cleanup */
	for (int thread_index = 0; thread_index < nr_thread; ++thread_index) {
		factory->destroy_client(client_arr[thread_index]);
		delete thread_arr[thread_index];
	}
	delete[] client_arr;
	delete[] thread_arr;
}

void run_init_workload_with_op_measurement(const char *task, ClientFactory *factory, long nr_entry, long key_size, long value_size, int nr_thread) {
	InitWorkload **workload_arr = new InitWorkload *[nr_thread];
	long nr_entry_per_thread = (nr_entry + nr_thread - 1) / nr_thread;
	for (int thread_index = 0; thread_index < nr_thread; ++thread_index) {
		long start_key = nr_entry_per_thread * thread_index;
		long end_key = std::min(nr_entry_per_thread * (thread_index + 1), nr_entry);
		workload_arr[thread_index] = new InitWorkload(end_key - start_key, start_key, key_size, value_size, thread_index);
	}

	run_workload_with_op_measurement(task, factory, (Workload **)workload_arr, nr_thread, nr_entry, nr_entry, 0, nullptr);

	for (int thread_index = 0; thread_index < nr_thread; ++thread_index) {
		delete workload_arr[thread_index];
	}
	delete[] workload_arr;
}

void run_uniform_workload_with_op_measurement(const char *task, ClientFactory *factory, long nr_entry, long key_size, long value_size,
                                              long scan_length, int nr_thread, struct OpProportion op_prop, long nr_op, long next_op_interval_ns,
                                              const char *latency_file) {
	UniformWorkload **workload_arr = new UniformWorkload *[nr_thread];
	for (int thread_index = 0; thread_index < nr_thread; ++thread_index) {
		workload_arr[thread_index] = new UniformWorkload(key_size, value_size, scan_length, nr_entry, nr_op, op_prop, thread_index);
	}

	run_workload_with_op_measurement(task, factory, (Workload **)workload_arr, nr_thread, nr_op, nr_thread * nr_op, next_op_interval_ns, latency_file);

	for (int thread_index = 0; thread_index < nr_thread; ++thread_index) {
		delete workload_arr[thread_index];
	}
	delete[] workload_arr;
}

void run_zipfian_workload_with_op_measurement(const char *task, ClientFactory *factory, long nr_entry, long key_size, long value_size,
                                              long scan_length, int nr_thread, struct OpProportion op_prop, double zipfian_constant, long nr_op, long next_op_interval_ns,
                                              const char *latency_file) {
	ZipfianWorkload **workload_arr = new ZipfianWorkload *[nr_thread];
	printf("ZipfianWorkload: start initializing zipfian variables, might take a while\n");
	ZipfianWorkload base_workload(key_size, value_size, scan_length, nr_entry, nr_op, op_prop, zipfian_constant, 0);
	for (int thread_index = 0; thread_index < nr_thread; ++thread_index) {
		workload_arr[thread_index] = base_workload.clone(thread_index);
	}

	run_workload_with_op_measurement(task, factory, (Workload **)workload_arr, nr_thread, nr_op, nr_thread * nr_op, next_op_interval_ns, latency_file);

	for (int thread_index = 0; thread_index < nr_thread; ++thread_index) {
		delete workload_arr[thread_index];
	}
	delete[] workload_arr;
}

void run_latest_workload_with_op_measurement(const char *task, ClientFactory *factory, long nr_entry, long key_size, long value_size,
                                             int nr_thread, double read_ratio, double zipfian_constant, long nr_op, long next_op_interval_ns,
                                             const char *latency_file) {
	LatestWorkload **workload_arr = new LatestWorkload *[nr_thread];
	printf("LatestWorkload: start initializing zipfian variables, might take a while\n");
	LatestWorkload base_workload(key_size, value_size, nr_entry, nr_op, read_ratio, zipfian_constant, 0);
	for (int thread_index = 0; thread_index < nr_thread; ++thread_index) {
		workload_arr[thread_index] = base_workload.clone(thread_index);
	}

	run_workload_with_op_measurement(task, factory, (Workload **)workload_arr, nr_thread, nr_op, nr_thread * nr_op, next_op_interval_ns, latency_file);

	for (int thread_index = 0; thread_index < nr_thread; ++thread_index) {
		delete workload_arr[thread_index];
	}
	delete[] workload_arr;
}

void run_trace_workload_with_op_measurement(const char *task, ClientFactory *factory, long key_size, long value_size,
                                            int nr_thread, std::list<std::string> trace_file_list, long nr_op, long next_op_interval_ns,
                                            const char *latency_file) {
	if (trace_file_list.size() < nr_thread)
		throw std::invalid_argument("insufficient trace files");
	TraceWorkload **workload_arr = new TraceWorkload *[nr_thread];
	std::list<std::string>::iterator trace_file_iter = trace_file_list.begin();
	printf("TraceWorkload: start loading trace files, might take a while\n");
	for (int thread_index = 0; thread_index < nr_thread; ++thread_index, ++trace_file_iter) {
		workload_arr[thread_index] = new TraceWorkload(key_size, value_size, nr_op, *trace_file_iter, thread_index);
	}

	run_workload_with_op_measurement(task, factory, (Workload **)workload_arr, nr_thread, nr_op, nr_thread * nr_op, next_op_interval_ns, latency_file);

	for (int thread_index = 0; thread_index < nr_thread; ++thread_index) {
		delete workload_arr[thread_index];
	}
	delete[] workload_arr;
}
