#ifndef YCSB_WORKER_H
#define YCSB_WORKER_H

#include <iostream>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <list>
#include <string>
#include "measurement.h"
#include "client.h"
#include "workload.h"

void worker_thread_fn(Client *client, Workload *workload, OpMeasurement *measurement, long next_op_interval_ns);
void monitor_thread_fn(const char *task, OpMeasurement *measurement);

void run_workload_with_op_measurement(const char *task, ClientFactory *factory, Workload **workload_arr,
                                      int nr_thread, long nr_op, long max_progress, long next_op_interval_ns,
                                      const char *latency_file);
void run_init_workload_with_op_measurement(const char *task, ClientFactory *factory, long nr_entry, long key_size, long value_size,
                                           int nr_thread);
void run_uniform_workload_with_op_measurement(const char *task, ClientFactory *factory, long nr_entry, long key_size, long value_size,
                                              long scan_length, int nr_thread, struct OpProportion op_prop, long nr_op, long next_op_interval_ns,
                                              const char *latency_file);
void run_zipfian_workload_with_op_measurement(const char *task, ClientFactory *factory, long nr_entry, long key_size, long value_size,
                                              long scan_length, int nr_thread, struct OpProportion op_prop, double zipfian_constant, long nr_op, long next_op_interval_ns,
                                              const char *latency_file);
void run_latest_workload_with_op_measurement(const char *task, ClientFactory *factory, long nr_entry, long key_size, long value_size,
                                             int nr_thread, double read_ratio, double zipfian_constant, long nr_op, long next_op_interval_ns,
                                             const char *latency_file);
void run_trace_workload_with_op_measurement(const char *task, ClientFactory *factory, long key_size, long value_size,
                                            int nr_thread, std::list<std::string> trace_file_list,long nr_op, long next_op_interval_ns,
                                            const char *latency_file);
#endif //YCSB_WORKER_H
