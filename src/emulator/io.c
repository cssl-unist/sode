// SPDX-License-Identifier: GPL-2.0-only

#include <linux/kthread.h>
#include <linux/ktime.h>
#include <linux/highmem.h>
#include <linux/sched/clock.h>

#include <linux/mutex.h>
#include <linux/filter.h>
#include <linux/bio.h>
extern const struct inode_operations ext4_file_inode_operations;

#include "nvmev.h"
#include "dma.h"

#if (SUPPORTED_SSD_TYPE(CONV) || SUPPORTED_SSD_TYPE(ZNS))
#include "ssd.h"
#else
struct buffer;
#endif

#undef PERF_DEBUG

#define sq_entry(entry_id) sq->sq[SQ_ENTRY_TO_PAGE_NUM(entry_id)][SQ_ENTRY_TO_PAGE_OFFSET(entry_id)]
#define cq_entry(entry_id) cq->cq[CQ_ENTRY_TO_PAGE_NUM(entry_id)][CQ_ENTRY_TO_PAGE_OFFSET(entry_id)]

extern bool io_using_dma;

static void __reclaim_completed_reqs_force(void);
static void __reclaim_completed_reqs(void);

int resubmit_logic(int sqid, int sq_entry, struct nvmev_io_work *w, int subtask);
int resubmit_logic_single(int sqid, int sq_entry, struct nvmev_io_work *w);

static inline unsigned int __get_io_worker(int sqid)
{
#ifdef CONFIG_NVMEV_IO_WORKER_BY_SQ
    return (sqid - 1) % nvmev_vdev->config.nr_io_workers;
#else
    return nvmev_vdev->io_worker_turn;
#endif
}

static inline unsigned long long __get_wallclock(void)
{
    return cpu_clock(nvmev_vdev->config.cpu_nr_dispatcher);
}

struct value {
    char found;
    uint64_t value;
};

struct xrp_scratch {
    uint64_t root;
    uint64_t value;
    unsigned int state_flags;
    int current_index;
    int n_keys;
    unsigned long keys[32];
    struct value values[32];
};

static inline u64 nvme_sect_to_lba(struct nvme_ns *ns, sector_t sector)
{
    return sector >> (ns->lba_shift - SECTOR_SHIFT);
}

static inline void log_empty(struct profile *profiler, unsigned int i) {
#ifdef PROFILE
        profiler[i].empty[profiler[i].id] = ktime_get_ns();
#endif
}

static inline void log_start(struct profile *profiler, unsigned int i) {
#ifdef PROFILE
        profiler[i].start[profiler[i].id] = ktime_get_ns();
#endif
}

static inline void log_io(struct profile *profiler, unsigned int i) {
#ifdef PROFILE
        profiler[i].io_time[profiler[i].id] = ktime_get_ns();
#endif
}

static inline void log_latemul(struct profile *profiler, unsigned int i) {
#ifdef PROFILE
    profiler[i].latency_emulation[profiler[i].id] = ktime_get_ns();
#endif
}

static inline void log_sec1(struct profile *profiler, unsigned int i) {
#ifdef PROFILE
    profiler[i].sec1[profiler[i].id] = ktime_get_ns();
    profiler[i].id = (profiler[i].id + 1) % 6;
#endif
}

static inline void log_sec2(struct profile *profiler, unsigned int i) {
#ifdef PROFILE
    profiler[i].sec2 = ktime_get_ns();
#endif
}

static inline void log_print(struct profile *profiler, unsigned int i) {
#ifdef PROFILE
    printk("LOG %lu, %lu, %lu, %lu, %lu, %lu, %lu, %lu, %lu, %lu, %lu, %lu, %lu, %lu, %lu, %lu, %lu, %lu, %lu, %lu, %lu, %lu, %lu, %lu, %lu, %lu, %lu, %lu, %lu, %lu", 
            profiler[i].empty[0], profiler[i].start[0], profiler[i].io_time[0], profiler[i].latency_emulation[0], profiler[i].sec1[0],
            profiler[i].empty[1], profiler[i].start[1], profiler[i].io_time[1], profiler[i].latency_emulation[1], profiler[i].sec1[1],
            profiler[i].empty[2], profiler[i].start[2], profiler[i].io_time[2], profiler[i].latency_emulation[2], profiler[i].sec1[2],
            profiler[i].empty[3], profiler[i].start[3], profiler[i].io_time[3], profiler[i].latency_emulation[3], profiler[i].sec1[3],
            profiler[i].empty[4], profiler[i].start[4], profiler[i].io_time[4], profiler[i].latency_emulation[4], profiler[i].sec1[4],
            profiler[i].empty[5], profiler[i].start[5], profiler[i].io_time[5], profiler[i].latency_emulation[5], profiler[i].sec1[5]
            );
#endif
}

static unsigned int __do_perform_io(int sqid, int sq_entry, struct nvmev_io_work *w)
{
	struct nvmev_submission_queue *sq = nvmev_vdev->sqes[sqid];
	struct nvme_rw_command *cmd = &sq_entry(sq_entry).rw;
	size_t offset;
	size_t length, remaining;
	int prp_offs = 0;
	int prp2_offs = 0;
	u64 paddr;
	u64 *paddr_list = NULL;
	size_t nsid = cmd->nsid - 1; // 0-based

    if (w->is_sode) {
        offset = w->slba << 9;
    }
    else {
        offset = cmd->slba << 9;
    }
    length = (cmd->length + 1) << 9;
	remaining = length;

	while (remaining) {
		size_t io_size;
		void *vaddr;
		size_t mem_offs = 0;

        if (!w->is_sode || w->paddr_cache == 0) {
            prp_offs++;
            if (prp_offs == 1) {
                paddr = cmd->prp1;
            } else if (prp_offs == 2) {
                paddr = cmd->prp2;
                if (remaining > PAGE_SIZE) {
                    paddr_list = kmap_atomic_pfn(PRP_PFN(paddr)) +
                             (paddr & PAGE_OFFSET_MASK);
                    paddr = paddr_list[prp2_offs++];
                }
            } else {
                paddr = paddr_list[prp2_offs++];
            }
            w->paddr_cache = paddr;
        }
        else {
            paddr = w->paddr_cache;
        }

		vaddr = kmap_atomic_pfn(PRP_PFN(paddr));

		io_size = min_t(size_t, remaining, PAGE_SIZE);

		if (paddr & PAGE_OFFSET_MASK) {
			mem_offs = paddr & PAGE_OFFSET_MASK;
			if (io_size + mem_offs > PAGE_SIZE)
				io_size = PAGE_SIZE - mem_offs;
		}

		if (cmd->opcode == nvme_cmd_write ||
		    cmd->opcode == nvme_cmd_zone_append) {
			memcpy(nvmev_vdev->ns[nsid].mapped + offset, vaddr + mem_offs, io_size);
		} else if (cmd->opcode == nvme_cmd_read) {
			memcpy(vaddr + mem_offs, nvmev_vdev->ns[nsid].mapped + offset, io_size);
		} else if (cmd->opcode == (nvme_cmd_read | nvme_cmd_sode)) {
            memcpy(vaddr + mem_offs, nvmev_vdev->ns[nsid].mapped + offset, io_size);
		} else if (cmd->opcode == (nvme_cmd_write | nvme_cmd_sode)) {
			memcpy(nvmev_vdev->ns[nsid].mapped + offset, vaddr + mem_offs, io_size);
		}

		kunmap_atomic(vaddr);

		remaining -= io_size;
		offset += io_size;
	}

	if (paddr_list != NULL)
		kunmap_atomic(paddr_list);

	return length;
}

static u64 paddr_list[513] = {
	0,
}; // Not using index 0 to make max index == num_prp
static unsigned int __do_perform_io_using_dma(int sqid, int sq_entry)
{
	struct nvmev_submission_queue *sq = nvmev_vdev->sqes[sqid];
	struct nvme_rw_command *cmd = &sq_entry(sq_entry).rw;
	size_t offset;
	size_t length, remaining;
	int prp_offs = 0;
	int prp2_offs = 0;
	int num_prps = 0;
	u64 paddr;
	u64 *tmp_paddr_list = NULL;
	size_t io_size;
	size_t mem_offs = 0;

	offset = cmd->slba << 9;
	length = (cmd->length + 1) << 9;
	remaining = length;

	memset(paddr_list, 0, sizeof(paddr_list));
	/* Loop to get the PRP list */
	while (remaining) {
		io_size = 0;

		prp_offs++;
		if (prp_offs == 1) {
			paddr_list[prp_offs] = cmd->prp1;
		} else if (prp_offs == 2) {
			paddr_list[prp_offs] = cmd->prp2;
			if (remaining > PAGE_SIZE) {
				tmp_paddr_list = kmap_atomic_pfn(PRP_PFN(paddr_list[prp_offs])) +
						 (paddr_list[prp_offs] & PAGE_OFFSET_MASK);
				paddr_list[prp_offs] = tmp_paddr_list[prp2_offs++];
			}
		} else {
			paddr_list[prp_offs] = tmp_paddr_list[prp2_offs++];
		}

		io_size = min_t(size_t, remaining, PAGE_SIZE);

		if (paddr_list[prp_offs] & PAGE_OFFSET_MASK) {
			mem_offs = paddr_list[prp_offs] & PAGE_OFFSET_MASK;
			if (io_size + mem_offs > PAGE_SIZE)
				io_size = PAGE_SIZE - mem_offs;
		}

		remaining -= io_size;
	}
	num_prps = prp_offs;

	if (tmp_paddr_list != NULL)
		kunmap_atomic(tmp_paddr_list);

	remaining = length;
	prp_offs = 1;

	/* Loop for data transfer */
	while (remaining) {
		size_t page_size;
		mem_offs = 0;
		io_size = 0;
		page_size = 0;

		paddr = paddr_list[prp_offs];
		page_size = min_t(size_t, remaining, PAGE_SIZE);

		/* For non-page aligned paddr, it will never be between continuous PRP list (Always first paddr)  */
		if (paddr & PAGE_OFFSET_MASK) {
			mem_offs = paddr & PAGE_OFFSET_MASK;
			if (page_size + mem_offs > PAGE_SIZE) {
				page_size = PAGE_SIZE - mem_offs;
			}
		}

		for (prp_offs++; prp_offs <= num_prps; prp_offs++) {
			if (paddr_list[prp_offs] == paddr_list[prp_offs - 1] + PAGE_SIZE)
				page_size += PAGE_SIZE;
			else
				break;
		}

		io_size = min_t(size_t, remaining, page_size);

		if (cmd->opcode == nvme_cmd_write ||
		    cmd->opcode == nvme_cmd_zone_append) {
			ioat_dma_submit(paddr, nvmev_vdev->config.storage_start + offset, io_size);
		} else if (cmd->opcode == nvme_cmd_read) {
			ioat_dma_submit(nvmev_vdev->config.storage_start + offset, paddr, io_size);
		} else if (cmd->opcode == (nvme_cmd_read | nvme_cmd_sode)) {
			ioat_dma_submit(nvmev_vdev->config.storage_start + offset, paddr, io_size);
		} else if (cmd->opcode == (nvme_cmd_write | nvme_cmd_sode)) {
			ioat_dma_submit(paddr, nvmev_vdev->config.storage_start + offset, io_size);
		}

		remaining -= io_size;
		offset += io_size;
	}

	return length;
}



static void __insert_req_sorted(unsigned int entry, struct nvmev_io_worker *worker,
				unsigned long nsecs_target)
{
	/**
	 * Requests are placed in @work_queue sorted by their target time.
	 * @work_queue is statically allocated and the ordered list is
	 * implemented by chaining the indexes of entries with @prev and @next.
	 * This implementation is nasty but we do this way over dynamically
	 * allocated linked list to minimize the influence of dynamic memory allocation.
	 * Also, this O(n) implementation can be improved to O(logn) scheme with
	 * e.g., red-black tree but....
	 */
	if (worker->io_seq == -1) {
		worker->io_seq = entry;
		worker->io_seq_end = entry;
	} else {
		unsigned int curr = worker->io_seq_end;

		while (curr != -1) {
			if (worker->work_queue[WQ(curr)][QE(curr)].nsecs_target <= worker->latest_nsecs)
				break;

			if (worker->work_queue[WQ(curr)][QE(curr)].nsecs_target <= nsecs_target)
				break;

			curr = worker->work_queue[WQ(curr)][QE(curr)].prev;
		}

		if (curr == -1) { /* Head inserted */
			worker->work_queue[WQ(worker->io_seq)][QE(worker->io_seq)].prev = entry;
			worker->work_queue[WQ(entry)][QE(entry)].next = worker->io_seq;
			worker->io_seq = entry;
		} else if (worker->work_queue[WQ(curr)][QE(curr)].next == -1) { /* Tail */
			worker->work_queue[WQ(entry)][QE(entry)].prev = curr;
			worker->io_seq_end = entry;
			worker->work_queue[WQ(curr)][QE(curr)].next = entry;
		} else { /* In between */
            unsigned int next;
			worker->work_queue[WQ(entry)][QE(entry)].prev = curr;
			worker->work_queue[WQ(entry)][QE(entry)].next = worker->work_queue[WQ(curr)][QE(curr)].next;

            next = worker->work_queue[WQ(entry)][QE(entry)].next;
            worker->work_queue[WQ(next)][QE(next)].prev = entry;

            worker->work_queue[WQ(curr)][QE(curr)].next = entry;
		}
	}
}

static struct nvmev_io_worker *__allocate_work_queue_entry(int sqid, unsigned int *entry)
{
	unsigned int io_worker_turn = __get_io_worker(sqid);
	struct nvmev_io_worker *worker = &nvmev_vdev->io_workers[io_worker_turn];
	unsigned int e = -1;
	struct nvmev_io_work *w = NULL;

	e = worker->free_seq;
    if (e == -1) {
        __reclaim_completed_reqs_force();
        cond_resched();
	    e = worker->free_seq;
    }

	w = &worker->work_queue[WQ(e)][QE(e)];
	if (w->next == worker->free_seq_end) {
        WARN_ON_ONCE("IO queue is almost full");
        
        __reclaim_completed_reqs_force();
        cond_resched();

        e = worker->free_seq;
	    w = &worker->work_queue[WQ(e)][QE(e)];
	}

	if (++io_worker_turn == nvmev_vdev->config.nr_io_workers)
		io_worker_turn = 0;
	nvmev_vdev->io_worker_turn = io_worker_turn;

	worker->free_seq = w->next;
	BUG_ON(worker->free_seq == -1);
	*entry = e;

	return worker;
}

static int __enqueue_io_req(int sqid, int cqid, int sq_entry, unsigned long long nsecs_start,
			     struct nvmev_result *ret)
{
	struct nvmev_submission_queue *sq = nvmev_vdev->sqes[sqid];
	struct nvmev_io_worker *worker;
	struct nvmev_io_work *w;
	unsigned int entry;

	worker = __allocate_work_queue_entry(sqid, &entry);
	if (!worker)
		return -1;

    w = &worker->work_queue[WQ(entry)][QE(entry)];

	NVMEV_DEBUG_VERBOSE("%s/%u[%d], sq %d cq %d, entry %d, %llu + %llu\n", worker->thread_name, entry,
		    sq_entry(sq_entry).rw.opcode, sqid, cqid, sq_entry, nsecs_start,
		    ret->nsecs_target - nsecs_start);

    if (sq_entry(sq_entry).rw.opcode & nvme_cmd_sode) {
        phys_addr_t phys = (phys_addr_t)sq_entry(sq_entry).rw.rsvd2;
        void *base = kmap_atomic_pfn(PHYS_PFN(phys));
        w->on_meta = (struct resubmit_data *)base;
        w->slba = w->on_meta->slba;
        w->is_sode = true;
    }
    else {
        w->on_meta = 0;
        w->slba = 0;
        w->is_sode = false;
    }

	/////////////////////////////////
	w->sqid = sqid;
	w->cqid = cqid;
	w->sq_entry = sq_entry;
    w->command_id = sq_entry(sq_entry).common.command_id;
	w->nsecs_start = nsecs_start;
	w->nsecs_enqueue = local_clock();
	w->nsecs_target = ret->nsecs_target;
	w->status = ret->status;
    w->is_copied = false;

	w->prev = -1;
	w->next = -1;
	w->is_internal = false;
    w->paddr_cache = 0;
	smp_mb(); /* IO worker shall see the updated w at once */

	w->is_completed = false;
	__insert_req_sorted(entry, worker, ret->nsecs_target);
    return 0;
}

void schedule_internal_operation(int sqid, unsigned long long nsecs_target,
				 struct buffer *write_buffer, size_t buffs_to_release)
{
	struct nvmev_io_worker *worker;
	struct nvmev_io_work *w;
	unsigned int entry;

	worker = __allocate_work_queue_entry(sqid, &entry);
	if (!worker)
		return;

    w = &worker->work_queue[WQ(entry)][QE(entry)];

	NVMEV_DEBUG_VERBOSE("%s/%u, internal sq %d, %llu + %llu\n", worker->thread_name, entry, sqid,
		    local_clock(), nsecs_target - local_clock());

	/////////////////////////////////
	w->sqid = sqid;
	w->nsecs_start = w->nsecs_enqueue = local_clock();
	w->nsecs_target = nsecs_target;
	w->is_completed = false;
	w->is_copied = true;
	w->prev = -1;
	w->next = -1;

	w->is_internal = true;
	w->write_buffer = write_buffer;
	w->buffs_to_release = buffs_to_release;
	mb(); /* IO worker shall see the updated w at once */

	__insert_req_sorted(entry, worker, nsecs_target);
}

static void __reclaim_completed_reqs_force(void)
{
    unsigned int turn;
    unsigned int i;

    for (turn = 0; turn < nvmev_vdev->config.nr_io_workers; turn++) {
        struct nvmev_io_worker *worker;
        struct nvmev_io_work *w;

        unsigned int first_free_seq = -1;
        unsigned int first_io_seq = -1;

        unsigned int prev_free_seq = -1;
        unsigned int prev_io_seq = -1;

        int reclaim_free = 0;
        int reclaim_io = 0;

        worker = &nvmev_vdev->io_workers[turn];

        if (worker->working_signal) {
            continue;
        }

        worker->working_signal = true;

        for (i = 0; i < NR_MAX_PARALLEL_IO * NUM_R_CPU; i++) {
            w = &worker->work_queue[WQ(i)][QE(i)];
            if (w->is_completed == true && w->is_copied == true &&
                    w->nsecs_target <= worker->latest_nsecs &&
                    w->is_resubmit == false) {

                if (first_free_seq == -1) {
                    first_free_seq = i;
                }

                /*
                if (w->prev != -1) {
                    worker->work_queue[w->prev].next = w->next;
                }

                if (w->next != -1) {
                    worker->work_queue[w->next].prev = w->prev;
                }
                */

                w->prev = prev_free_seq;
                if (prev_free_seq != -1) {
                    worker->work_queue[WQ(prev_free_seq)][QE(prev_free_seq)].next = i;
                }

                w->next = -1;

                prev_free_seq = i;

                reclaim_free++;
            }
            else {
                if (first_io_seq == -1) {
                    first_io_seq = i;
                }

                /*
                if (w->prev != -1) {
                    worker->work_queue[w->prev].next = w->next;
                }

                if (w->next != -1) {
                    worker->work_queue[w->next].prev = w->prev;
                }
                */

                w->prev = prev_io_seq;
                if (prev_io_seq != -1) {
                    worker->work_queue[WQ(prev_io_seq)][QE(prev_io_seq)].next = i;
                }

                w->next = -1;

                prev_io_seq = i;

                reclaim_io++;
            }
        }

        worker->free_seq = first_free_seq;
        worker->free_seq_end = prev_free_seq;

        worker->io_seq = first_io_seq;
        worker->io_seq_end = prev_io_seq;
        smp_mb();

        worker->working_signal = false;
        /*
        printk("WORKER %d: FREE %d (%d, %d), IO %d (%d, %d)\n", turn,
                reclaim_free, first_free_seq, prev_free_seq, 
                reclaim_io, first_io_seq, prev_io_seq
                );
                */
    }
}

void __reclaim_completed_reqs(void)
{
	unsigned int turn;

	for (turn = 0; turn < nvmev_vdev->config.nr_io_workers; turn++) {
		struct nvmev_io_worker *worker;
		struct nvmev_io_work *w;

		unsigned int first_entry = -1;
		unsigned int last_entry = -1;
		unsigned int curr;
		int nr_reclaimed = 0;

		worker = &nvmev_vdev->io_workers[turn];

        if (worker->working_signal) {
            continue;
        }

        worker->working_signal = true;

        first_entry = worker->io_seq;
		curr = first_entry;

		while (curr != -1) {
			w = &worker->work_queue[WQ(curr)][QE(curr)];
            if (w->is_completed == true && w->is_copied == true &&
			    w->nsecs_target <= worker->latest_nsecs &&
                w->is_resubmit == false) {
                last_entry = curr;
                curr = w->next;
				nr_reclaimed++;
			} else {
                break;
			}
		}

		if (last_entry != -1) {
			w = &worker->work_queue[WQ(last_entry)][QE(last_entry)];
			worker->io_seq = w->next;
			if (w->next != -1) {
			    worker->work_queue[WQ(w->next)][QE(w->next)].prev = -1;
			}
			w->next = -1;

			w = &worker->work_queue[WQ(first_entry)][QE(first_entry)];
			w->prev = worker->free_seq_end;

			w = &worker->work_queue[WQ(worker->free_seq_end)][QE(worker->free_seq_end)];
			w->next = first_entry;

			worker->free_seq_end = last_entry;
			NVMEV_DEBUG_VERBOSE("%s: %u -- %u, %d\n", __func__,
					first_entry, last_entry, nr_reclaimed);
		}

        worker->working_signal = false;
	}
}


static size_t __nvmev_proc_io(int sqid, int sq_entry, size_t *io_size)
{
	struct nvmev_submission_queue *sq = nvmev_vdev->sqes[sqid];
	unsigned long long nsecs_start = __get_wallclock();
	struct nvme_command *cmd = &sq_entry(sq_entry);
#if (BASE_SSD == KV_PROTOTYPE)
	uint32_t nsid = 0; // Some KVSSD programs give 0 as nsid for KV IO
#else
	uint32_t nsid = cmd->common.nsid - 1;
#endif
	struct nvmev_ns *ns = &nvmev_vdev->ns[nsid];

	struct nvmev_request req = {
		.cmd = cmd,
		.sq_id = sqid,
		.nsecs_start = nsecs_start,
	};
	struct nvmev_result ret = {
		.nsecs_target = nsecs_start,
		.status = NVME_SC_SUCCESS,
	};


#ifdef PERF_DEBUG
	unsigned long long prev_clock = local_clock();
	unsigned long long prev_clock2 = 0;
	unsigned long long prev_clock3 = 0;
	unsigned long long prev_clock4 = 0;
	static unsigned long long clock1 = 0;
	static unsigned long long clock2 = 0;
	static unsigned long long clock3 = 0;
	static unsigned long long counter = 0;
#endif

	if (!ns->proc_io_cmd(ns, &req, &ret)) {
        return false;
    }

	*io_size = (cmd->rw.length + 1) << 9;

#ifdef PERF_DEBUG
	prev_clock2 = local_clock();
#endif


    if (__enqueue_io_req(sqid, sq->cqid, sq_entry, nsecs_start, &ret) < 0) {
        return false;
    }

#ifdef PERF_DEBUG
	prev_clock3 = local_clock();
#endif

    __reclaim_completed_reqs();

#ifdef PERF_DEBUG
	prev_clock4 = local_clock();

	clock1 += (prev_clock2 - prev_clock);
	clock2 += (prev_clock3 - prev_clock2);
	clock3 += (prev_clock4 - prev_clock3);
	counter++;

	if (counter > 1000) {
		NVMEV_DEBUG("LAT: %llu, ENQ: %llu, CLN: %llu\n", clock1 / counter, clock2 / counter,
			    clock3 / counter);
		clock1 = 0;
		clock2 = 0;
		clock3 = 0;
		counter = 0;
	}
#endif
	return true;
}

int nvmev_proc_io_sq(int sqid, int new_db, int old_db)
{
	struct nvmev_submission_queue *sq = nvmev_vdev->sqes[sqid];
	int num_proc = new_db - old_db;
	int seq;
	int sq_entry = old_db;
	int latest_db;

	if (unlikely(!sq))
		return old_db;
	if (unlikely(num_proc < 0))
		num_proc += sq->queue_size;

	for (seq = 0; seq < num_proc; seq++) {
		size_t io_size;
		if (!__nvmev_proc_io(sqid, sq_entry, &io_size))
			break;

		if (++sq_entry == sq->queue_size) {
			sq_entry = 0;
		}
		sq->stat.nr_dispatched++;
		sq->stat.nr_in_flight++;
		sq->stat.total_io += io_size;
	}
	sq->stat.nr_dispatch++;
	sq->stat.max_nr_in_flight = max_t(int, sq->stat.max_nr_in_flight, sq->stat.nr_in_flight);

	latest_db = (old_db + seq) % sq->queue_size;
	return latest_db;
}

void nvmev_proc_io_cq(int cqid, int new_db, int old_db)
{
	struct nvmev_completion_queue *cq = nvmev_vdev->cqes[cqid];
	int i;
	for (i = old_db; i != new_db; i++) {
		int sqid = cq_entry(i).sq_id;
		if (i >= cq->queue_size) {
			i = -1;
			continue;
		}

		/* Should check the validity here since SPDK deletes SQ immediately
		 * before processing associated CQes */
		if (!nvmev_vdev->sqes[sqid]) continue;

		nvmev_vdev->sqes[sqid]->stat.nr_in_flight--;
	}

	cq->cq_tail = new_db - 1;
	if (new_db == -1)
		cq->cq_tail = cq->queue_size - 1;
}

static void __fill_cq_result(struct nvmev_io_work *w)
{
	int sqid = w->sqid;
	int cqid = w->cqid;
	int sq_entry = w->sq_entry;
	unsigned int command_id = w->command_id;
	unsigned int status = w->status;
	unsigned int result0 = w->result0;
	unsigned int result1 = w->result1;

	struct nvmev_completion_queue *cq = nvmev_vdev->cqes[cqid];
	int cq_head;
	struct nvme_completion *cqe;

	spin_lock(&cq->entry_lock);
    cq_head = cq->cq_head;
    cqe = &cq_entry(cq_head);

	cqe->command_id = command_id;
	cqe->sq_id = sqid;
	cqe->sq_head = sq_entry;
	cqe->status = cq->phase | (status << 1);
	cqe->result0 = result0;
	cqe->result1 = result1;

	if (++cq_head == cq->queue_size) {
		cq_head = 0;
		cq->phase = !cq->phase;
	}

	cq->cq_head = cq_head;
	cq->interrupt_ready = true;
	spin_unlock(&cq->entry_lock);
}

static void __fill_cq_result_complete(struct nvmev_io_work *w)
{
	int sqid = w->sqid;
	int cqid = w->cqid;
	int sq_entry = w->sq_entry;
	unsigned int command_id = w->command_id;
	unsigned int status = w->status;
	unsigned int result0 = w->result0;
	unsigned int result1 = w->result1;

	struct nvmev_completion_queue *cq = nvmev_vdev->cqes[cqid];
	int cq_head;
	struct nvme_completion *cqe;

	spin_lock(&cq->entry_lock);
    cq_head = cq->cq_head;
    cqe = &cq_entry(cq_head);

	cqe->command_id = command_id;
	cqe->sq_id = sqid;
	cqe->sq_head = sq_entry;
	cqe->status = cq->phase | (status << 1);
	cqe->result0 = result0;
	cqe->result1 = result1;

	if (++cq_head == cq->queue_size) {
		cq_head = 0;
		cq->phase = !cq->phase;
	}

	cq->cq_head = cq_head;
	//cq->interrupt_ready = true;
	spin_unlock(&cq->entry_lock);

    spin_lock(&cq->irq_lock);
    nvmev_signal_irq(cq->irq_vector);
    spin_unlock(&cq->irq_lock);
}

void driver_retrieve_mapping(struct xrp_extent *extents, int num_extents, loff_t offset, loff_t len, struct xrp_mapping *mapping)
{
	int i;
	__u64 i_lblk_start, i_lblk_end;

	i_lblk_start = offset >> XRP_BLOCK_SHIFT;
	i_lblk_end = (offset + len - 1) >> XRP_BLOCK_SHIFT;

	mapping->exist = false;
	for (i = 0; i < num_extents; i++) {
		if (i_lblk_start >= extents[i].lblk && i_lblk_start <= (extents[i].lblk + extents[i].len) - 1) {
			loff_t in_extent_offset = offset - (((u64)extents[i].lblk) << XRP_BLOCK_SHIFT);
			loff_t in_extent_len = min(len, (((u64)extents[i].len) << XRP_BLOCK_SHIFT) - in_extent_offset);

			mapping->exist = true;
			mapping->offset = offset;
			mapping->len = in_extent_len;
			mapping->address = (extents[i].pblk << XRP_BLOCK_SHIFT) + in_extent_offset;
			mapping->version = extents[i].version;
			return;
		}
	}

	return;
}

/*
 * Config definitions
 */
#define EBPF_BLOCK_SIZE 512
#define EBPF_MAX_DEPTH 6
#define EBPF_DEPTH_MASK 0x7
#define EBPF_KEY_MAX_LEN 18
#define EBPF_CONTEXT_MASK 0xfff

/*
 * Error numbers
 */
#define EBPF_EINVAL 22
#define EBPF_EAGAIN 11

/*
 * Page layout
 */
 struct ebpf_page_header {
     uint64_t recno; /* 00-07: column-store starting recno */
     uint64_t write_gen; /* 08-15: write generation */
     uint32_t mem_size; /* 16-19: in-memory page size */
     union {
         uint32_t entries; /* 20-23: number of cells on page */
         uint32_t datalen; /* 20-23: overflow data length */
     } u;
     uint8_t type; /* 24: page type */
#define EBPF_PAGE_COMPRESSED 0x01u   /* Page is compressed on disk */
#define EBPF_PAGE_EMPTY_V_ALL 0x02u  /* Page has all zero-length values */
#define EBPF_PAGE_EMPTY_V_NONE 0x04u /* Page has no zero-length values */
#define EBPF_PAGE_ENCRYPTED 0x08u    /* Page is encrypted on disk */
#define EBPF_PAGE_UNUSED 0x10u       /* Historic lookaside store page updates, no longer used */
     uint8_t flags; /* 25: flags */
     uint8_t unused; /* 26: unused padding */
#define EBPF_PAGE_VERSION_ORIG 0 /* Original version */
#define EBPF_PAGE_VERSION_TS 1   /* Timestamps added */
     uint8_t version; /* 27: version */
};
#define EBPF_PAGE_HEADER_SIZE 28

#define EBPF_PAGE_INVALID 0       /* Invalid page */
#define EBPF_PAGE_BLOCK_MANAGER 1 /* Block-manager page */
#define EBPF_PAGE_COL_FIX 2       /* Col-store fixed-len leaf */
#define EBPF_PAGE_COL_INT 3       /* Col-store internal page */
#define EBPF_PAGE_COL_VAR 4       /* Col-store var-length leaf page */
#define EBPF_PAGE_OVFL 5          /* Overflow page */
#define EBPF_PAGE_ROW_INT 6       /* Row-store internal page */
#define EBPF_PAGE_ROW_LEAF 7      /* Row-store leaf page */

struct ebpf_block_header {
    /* copy from https://github.com/wiredtiger/wiredtiger/blob/mongodb-4.4.0/src/include/block.h#L329 */

    uint32_t disk_size; /* 00-03: on-disk page size */
    uint32_t checksum; /* 04-07: checksum */
    uint8_t flags; /* 08: flags */
    uint8_t unused[3]; /* 09-11: unused padding */
};
#define EBPF_BLOCK_HEADER_SIZE (12)

/*
 * Cell types & macros
 * extract from https://github.com/wiredtiger/wiredtiger/blob/mongodb-4.4.0/src/include/cell.h#L10
 */
#define EBPF_CELL_KEY_SHORT 0x01     /* Short key */
#define EBPF_CELL_KEY_SHORT_PFX 0x02 /* Short key with prefix byte */
#define EBPF_CELL_VALUE_SHORT 0x03   /* Short data */
#define EBPF_CELL_SHORT_TYPE(v) ((v)&0x03U)

#define EBPF_CELL_SHORT_MAX 63  /* Maximum short key/value */
#define EBPF_CELL_SHORT_SHIFT 2 /* Shift for short key/value */

#define EBPF_CELL_64V 0x04         /* Associated value */
#define EBPF_CELL_SECOND_DESC 0x08 /* Second descriptor byte */

#define EBPF_CELL_ADDR_DEL (0)            /* Address: deleted */
#define EBPF_CELL_ADDR_INT (1 << 4)       /* Address: internal  */
#define EBPF_CELL_ADDR_LEAF (2 << 4)      /* Address: leaf */
#define EBPF_CELL_ADDR_LEAF_NO (3 << 4)   /* Address: leaf no overflow */
#define EBPF_CELL_DEL (4 << 4)            /* Deleted value */
#define EBPF_CELL_KEY (5 << 4)            /* Key */
#define EBPF_CELL_KEY_OVFL (6 << 4)       /* Overflow key */
#define EBPF_CELL_KEY_OVFL_RM (12 << 4)   /* Overflow key (removed) */
#define EBPF_CELL_KEY_PFX (7 << 4)        /* Key with prefix byte */
#define EBPF_CELL_VALUE (8 << 4)          /* Value */
#define EBPF_CELL_VALUE_COPY (9 << 4)     /* Value copy */
#define EBPF_CELL_VALUE_OVFL (10 << 4)    /* Overflow value */
#define EBPF_CELL_VALUE_OVFL_RM (11 << 4) /* Overflow value (removed) */

#define EBPF_CELL_TYPE_MASK (0x0fU << 4)
#define EBPF_CELL_TYPE(v) ((v)&EBPF_CELL_TYPE_MASK)

#define EBPF_CELL_SIZE_ADJUST (EBPF_CELL_SHORT_MAX + 1)

/*
 * Variable-sized unpacking for unsigned integers
 * extracted from https://github.com/wiredtiger/wiredtiger/blob/mongodb-4.4.0/src/include/intpack.i#L254
 */
#define EBPF_POS_1BYTE_MARKER (uint8_t)0x80
#define EBPF_POS_2BYTE_MARKER (uint8_t)0xc0
#define EBPF_POS_MULTI_MARKER (uint8_t)0xe0
#define EBPF_POS_1BYTE_MAX ((1 << 6) - 1)
#define EBPF_POS_2BYTE_MAX ((1 << 13) + EBPF_POS_1BYTE_MAX)

struct wt_ebpf_scratch {
    uint64_t key_size;
    char key[EBPF_KEY_MAX_LEN];

    int32_t level;
    int32_t iteration;
    uint64_t page_offset;
    uint64_t prev_cell_descent_offset, prev_cell_descent_size;

    int32_t nr_page;
    uint64_t descent_index_arr[EBPF_MAX_DEPTH];
};

/* Extract bits <start> to <end> from a value (counting from LSB == 0). */
#define GET_BITS(x, start, end) (((uint64_t)(x) & ((1U << (start)) - 1U)) >> (end))

int ebpf_lex_compare(struct bpf_xrp_kern *context, uint64_t key_offset_1, uint64_t key_len_1,
                                uint64_t key_offset_2, uint64_t key_len_2) {
    /* extracted from https://github.com/wiredtiger/wiredtiger/blob/mongodb-4.4.0/src/include/btree_cmp.i#L90 */
    uint8_t *p_base = (uint8_t *) context->data;
    uint8_t *k_base = (uint8_t *) context->scratch;
    uint8_t b1, b2;
    uint64_t len = (key_len_1 > key_len_2) ? key_len_2 : key_len_1;
    uint64_t max_len = EBPF_KEY_MAX_LEN;
    for (; len > 0 && max_len > 0; --len, --max_len, ++key_offset_1, ++key_offset_2) {
        b1 = *(k_base + (key_offset_1 & EBPF_CONTEXT_MASK));
        b2 = *(p_base + (key_offset_2 & EBPF_CONTEXT_MASK));
        if (b1 != b2)
            return (b1 < b2 ? -1 : 1);
    }
    return ((key_len_1 == key_len_2) ? 0 : (key_len_1 < key_len_2) ? -1 : 1);
}

int ebpf_unpack_posint(struct bpf_xrp_kern *context, uint64_t p_offset,
                                  uint64_t *retp, uint64_t *p_delta) {
    uint64_t x = 0;
    uint8_t max_len = 15;  /* max_len is set to pass the ebpf verifier */
    uint8_t *p_base = (uint8_t *) context->data;
    uint8_t b;
    uint8_t len;

    if (retp == NULL || p_delta == NULL)
        return -EBPF_EINVAL;

    /* There are four length bits in the first byte. */
    b = *(p_base + (p_offset & EBPF_CONTEXT_MASK));
    len = (b & 0xf);
    ++p_offset;
    ++(*p_delta);

    for (; len > 0 && max_len > 0; --len, --max_len) {
        b = *(p_base + (p_offset & EBPF_CONTEXT_MASK));
        x = (x << 8) | b;
        ++p_offset;
        ++(*p_delta);
    }

    *retp = x;
    return 0;
}

int ebpf_vunpack_uint(struct bpf_xrp_kern *context, uint64_t p_offset,
                                 uint64_t *xp, uint64_t *p_delta) {
    uint8_t *p_base = (uint8_t *) context->data;
    uint8_t b;
    int ret;

    if (xp == NULL || p_delta == NULL)
        return -EBPF_EINVAL;

    /* encoding scheme: https://github.com/wiredtiger/wiredtiger/blob/mongodb-4.4.0/src/include/intpack.i#L10 */
    b = *(p_base + (p_offset & EBPF_CONTEXT_MASK));
    switch (b & 0xf0) {
    case EBPF_POS_1BYTE_MARKER:
    case EBPF_POS_1BYTE_MARKER | 0x10:
    case EBPF_POS_1BYTE_MARKER | 0x20:
    case EBPF_POS_1BYTE_MARKER | 0x30:
        /* higher 2 bits of the first byte is 10 */
        *xp = GET_BITS(b, 6, 0);  /* extract integer from the remaining (8 - 2) = 6 bites */
        ++p_offset;
        ++(*p_delta);
        break;
    case EBPF_POS_2BYTE_MARKER:
    case EBPF_POS_2BYTE_MARKER | 0x10:
        /* higher 3 bits of the first byte is 110 */
        *xp = GET_BITS(b, 5, 0) << 8;
        ++p_offset;
        ++(*p_delta);
        b = *(p_base + (p_offset & EBPF_CONTEXT_MASK));
        *xp |= b;
        *xp += EBPF_POS_1BYTE_MAX + 1;
        ++(*p_delta);
        ++p_offset;
        break;
    case EBPF_POS_MULTI_MARKER:
        /* higher 4 bits of the first byte is 1110 */
        ret = ebpf_unpack_posint(context, p_offset, xp, p_delta);
        if (ret != 0) {
            return ret;
        }
        *xp += EBPF_POS_2BYTE_MAX + 1;
        return 0;
    default:
        return -EBPF_EINVAL;
    }

    return 0;
}

int ebpf_addr_to_offset(struct bpf_xrp_kern *context, uint64_t p_offset,
                                   uint64_t *offset, uint64_t *size) {
    int ret = 0;
    uint64_t p_delta = 0;
    uint64_t raw_offset = 0, raw_size = 0, raw_checksum = 0;

    if (offset == NULL || size == NULL)
        return -EBPF_EINVAL;

    ret = ebpf_vunpack_uint(context, p_offset, &raw_offset, &p_delta);
    if (ret < 0) {
        return ret;
    }
    p_offset += p_delta;
    p_delta = 0;

    ret = ebpf_vunpack_uint(context, p_offset, &raw_size, &p_delta);
    if (ret < 0) {
        return ret;
    }
    p_offset += p_delta;
    p_delta = 0;

    ret = ebpf_vunpack_uint(context, p_offset, &raw_checksum, &p_delta);  /* checksum is not used */
    if (ret < 0) {
        return ret;
    }

    if (raw_size == 0) {
        *offset = 0;
        *size = 0;
    } else {
        /* assumption: allocation size is EBPF_BLOCK_SIZE */
        *offset = EBPF_BLOCK_SIZE * (raw_offset + 1);
        *size = EBPF_BLOCK_SIZE * raw_size;
    }
    return 0;
}

static __inline int ebpf_get_cell_type(uint8_t cell_desc) {
    return EBPF_CELL_SHORT_TYPE(cell_desc) ? EBPF_CELL_SHORT_TYPE(cell_desc) : EBPF_CELL_TYPE(cell_desc);
}

int ebpf_parse_cell_addr(struct bpf_xrp_kern *context, uint64_t p_offset,
                                    uint64_t *offset, uint64_t *size, uint64_t *p_delta) {
    uint8_t *p_base = (uint8_t *) context->data;
    uint8_t b;
    uint64_t local_p_delta = 0;
    uint8_t cell_desc, flags;
    uint64_t addr_len = 0;
    int ret;

    if (offset == NULL || size == NULL || p_delta == NULL)
        return -EBPF_EINVAL;

    /* read the first cell descriptor byte (cell type, RLE count) */
    b = *(p_base + (p_offset & EBPF_CONTEXT_MASK));
    cell_desc = b;
    if ((ebpf_get_cell_type(cell_desc) != EBPF_CELL_ADDR_INT
         && ebpf_get_cell_type(cell_desc) != EBPF_CELL_ADDR_LEAF
         && ebpf_get_cell_type(cell_desc) != EBPF_CELL_ADDR_LEAF_NO)
        || ((cell_desc & EBPF_CELL_64V) != 0)) {
        return -EBPF_EINVAL;
    }
    ++p_offset;
    ++(*p_delta);

    /* read the second cell descriptor byte (if present) */
    if ((cell_desc & EBPF_CELL_SECOND_DESC) != 0) {
        b = *(p_base + (p_offset & EBPF_CONTEXT_MASK));
        flags = b;
        ++p_offset;
        ++(*p_delta);
        if (flags != 0) {
            return -EBPF_EINVAL;
        }
    }

    /* the cell is followed by data length and a chunk of data */
    ret = ebpf_vunpack_uint(context, p_offset, &addr_len, &local_p_delta);
    if (ret != 0) {
        return ret;
    }
    p_offset += local_p_delta;
    (*p_delta) += local_p_delta;
    local_p_delta = 0;

    /* convert addr to file offset */
    ret = ebpf_addr_to_offset(context, p_offset, offset, size);
    if (ret != 0) {
        return ret;
    }

    (*p_delta) += addr_len;
    return 0;
}

int ebpf_parse_cell_key(struct bpf_xrp_kern *context, uint64_t p_offset,
                                   uint64_t *key_offset, uint64_t *key_size, uint64_t *p_delta) {
    uint8_t *p_base = (uint8_t *) context->data;
    uint8_t b;
    uint64_t local_p_delta = 0;
    uint64_t data_len = 0;
    int ret;

    if (key_offset == NULL || key_size == NULL || p_delta == NULL)
        return -EBPF_EINVAL;
    (*key_offset) = 0;

    /* read the first cell descriptor byte (cell type, RLE count) */
    b = *(p_base + (p_offset & EBPF_CONTEXT_MASK));
    if ((ebpf_get_cell_type(b) != EBPF_CELL_KEY)
        || ((b & EBPF_CELL_64V) != 0)) {
        return -EBPF_EINVAL;
    }
    ++p_offset;
    ++(*p_delta);
    ++(*key_offset);

    /* key cell does not have the second descriptor byte */

    /* the cell is followed by data length and a chunk of data */
    ret = ebpf_vunpack_uint(context, p_offset, &data_len, &local_p_delta);
    if (ret != 0) {
        return ret;
    }
    data_len += EBPF_CELL_SIZE_ADJUST;
    p_offset += local_p_delta;
    (*p_delta) += local_p_delta;
    (*key_offset) += local_p_delta;
    local_p_delta = 0;

    *key_size = data_len;
    (*p_delta) += data_len;
    return 0;
}

int ebpf_parse_cell_short_key(struct bpf_xrp_kern *context, uint64_t p_offset,
                                         uint64_t *key_offset, uint64_t *key_size, uint64_t *p_delta) {
    uint8_t *p_base = (uint8_t *) context->data;
    uint8_t b;
    uint64_t data_len;

    if (key_offset == NULL || key_size == NULL || p_delta == NULL)
        return -EBPF_EINVAL;
    (*key_offset) = 0;

    /* read the first cell descriptor byte */
    b = *(p_base + (p_offset & EBPF_CONTEXT_MASK));
    if (ebpf_get_cell_type(b) != EBPF_CELL_KEY_SHORT) {
        return -EBPF_EINVAL;
    }
    data_len = (b) >> EBPF_CELL_SHORT_SHIFT;
    *key_size = data_len;

    ++p_offset;
    ++(*p_delta);
    ++(*key_offset);

    (*p_delta) += data_len;
    return 0;
}

/*
__wt_page_inmem: https://github.com/wiredtiger/wiredtiger/blob/mongodb-4.4.0/src/btree/bt_page.c#L128
__inmem_row_int: https://github.com/wiredtiger/wiredtiger/blob/mongodb-4.4.0/src/btree/bt_page.c#L375
WT_CELL_FOREACH_ADDR: https://github.com/wiredtiger/wiredtiger/blob/mongodb-4.4.0/src/include/cell.i#L1155
__wt_cell_unpack_safe: https://github.com/wiredtiger/wiredtiger/blob/mongodb-4.4.0/src/include/cell.i#L663
__wt_row_search: https://github.com/wiredtiger/wiredtiger/blob/mongodb-4.4.0/src/btree/row_srch.c#L331
*/
int ebpf_search_int_page(struct bpf_xrp_kern *context,
                                    uint64_t user_key_offset, uint64_t user_key_size,
                                    uint64_t *descent_offset, uint64_t *descent_index) {
    uint8_t *p_base = (uint8_t *) context->data;
    //struct wt_ebpf_scratch *scratch = (struct wt_ebpf_scratch *) context->scratch;
    uint8_t b;
    uint64_t p_offset = 0;
    uint64_t local_p_delta = 0;

    struct ebpf_block_header *bheader = (struct ebpf_block_header *)(context->data + EBPF_PAGE_HEADER_SIZE);
    struct ebpf_page_header *header = (struct ebpf_page_header *) context->data;
    uint32_t nr_kv = header->u.entries / 2, i = 0, ii = 0;
    uint64_t prev_cell_descent_offset = 0, prev_cell_descent_size = 0;
    int done = 0;
    int ret = 0;

    //asm volatile("r0 = 0" ::: "r0");

    if (descent_offset == NULL || descent_index == NULL) {
        printk("ebpf_search_int_page: invalid arguments");
        return -EBPF_EINVAL;
    }

    done = context->done;
    if (done > 0 && done < 7) {
        uint64_t val = 0;

        memcpy(&val, bheader->unused, 3);

        p_offset = 0;
        p_offset += 40;
        p_offset += (val >> ((2) * 8)) & 0xFF * (done == 4 || done == 3 || done == 2);
        p_offset += (val >> ((1) * 8)) & 0xFF * (done == 4 || done == 3);
        p_offset += (val >> ((0) * 8)) & 0xFF * (done == 4);

        nr_kv = 4;

        if ((header->u.entries / 2) < 4 * done) {
            if ((int)(header->u.entries / 2 - 4 * (done - 1)) > 0) {
                nr_kv = header->u.entries / 2 - 4 * (done - 1);
            }
            else {
                nr_kv = 0;
            }
        }

        context->done = 0;
    }
    else {
        done = 1;
        /* skip page header + block header */
        p_offset += (EBPF_PAGE_HEADER_SIZE + EBPF_BLOCK_HEADER_SIZE);
    }

    /* traverse all key value pairs */
    for (i = 0, ii = EBPF_BLOCK_SIZE; i < nr_kv && ii > 0; ++i, --ii) {
        uint64_t cell_key_offset = 0, cell_key_size = 0;
        uint64_t cell_descent_offset = 0, cell_descent_size = 0;
        int cmp = 0;

        /*
         * searching for the corresponding descent.
         * each cell (key, addr) corresponds to key range [key, next_key)
         * extracted from https://github.com/wiredtiger/wiredtiger/blob/mongodb-4.4.0/src/btree/row_srch.c#L331
         */

        /* parse key cell */
        b = *(p_base + (p_offset & EBPF_CONTEXT_MASK));
        switch (ebpf_get_cell_type(b)) {
        case EBPF_CELL_KEY:
            ret = ebpf_parse_cell_key(context, p_offset, &cell_key_offset, &cell_key_size, &local_p_delta);
            if (ret < 0) {
                printk("ebpf_search_int_page: ebpf_parse_cell_key failed, kv %d, ret %d", i, ret);
                return ret;
            }
            break;
        case EBPF_CELL_KEY_SHORT:
            ret = ebpf_parse_cell_short_key(context, p_offset, &cell_key_offset, &cell_key_size, &local_p_delta);
            if (ret < 0) {
                printk("ebpf_search_int_page: ebpf_parse_cell_short_key failed, kv %d ret %d", i, ret);
                return ret;
            }
            break;
        default:
            printk("%d: ebpf_search_int_page: invalid cell type %d, kv %d", done, ebpf_get_cell_type(b), i);
            return -EBPF_EINVAL;
        }
        cell_key_offset += p_offset;
        p_offset += local_p_delta;
        local_p_delta = 0;

        /* parse addr cell */
        ret = ebpf_parse_cell_addr(context, p_offset, &cell_descent_offset, &cell_descent_size, &local_p_delta);
        if (ret < 0) {
            printk("ebpf_search_int_page: ebpf_parse_cell_addr failed, kv %d, ret %d", i, ret);
            return ret;
        }
        if (cell_descent_size != EBPF_BLOCK_SIZE) {
            printk("ebpf_search_int_page: descent size mismatch, expected %d, got %lld", EBPF_BLOCK_SIZE, cell_descent_size);
            return -EBPF_EINVAL;
        }
        p_offset += local_p_delta;
        local_p_delta = 0;

        /*
         * compare with user key
         * extracted from https://github.com/wiredtiger/wiredtiger/blob/mongodb-4.4.0/src/btree/row_srch.c#L331
         */
        if (i == 0 && done == 1)
            cmp = 1;
        else
            cmp = ebpf_lex_compare(context, user_key_offset, user_key_size, cell_key_offset, cell_key_size);
        if (cmp == 0) {
            /* user key = cell key */
            *descent_offset = cell_descent_offset;
            *descent_index = i + (done - 1) * 4;
            //printk("%d] 1: %d", done, prev_cell_descent_offset);
            return 1;
        } else if (cmp < 0) {
            /* user key < cell key */
            *descent_offset = prev_cell_descent_offset;
            *descent_index = i - 1 + (done - 1) * 4;
            //printk("%d] 2: %d, %d", done, i - 1 + (done - 1) * 4, nr_kv);
            return 2;
        }
        prev_cell_descent_offset = cell_descent_offset;
        prev_cell_descent_size = cell_descent_size;
    }

    *descent_offset = prev_cell_descent_offset;
    *descent_index = i - 1 + (done - 1) * 4;
    //printk("%d] 3: %d", done, i - 1 + (done - 1) * 4);
    return 3;
}

__u32 wiredtiger_lookup(struct bpf_xrp_kern *context) {
    struct wt_ebpf_scratch *scratch = (struct wt_ebpf_scratch *) context->scratch;
    struct ebpf_page_header *header = (struct ebpf_page_header *) context->data;
    uint64_t descent_offset = 0, descent_index = 0;
    uint64_t *src_ptr = (uint64_t *) context->data;
    uint64_t *dst_ptr = (uint64_t *) context->scratch;
    uint64_t src_offset, dst_offset;
    int i;
    int ret;

    /*
    dst_offset = 1024;
    dst_offset += EBPF_BLOCK_SIZE * scratch->level;
    dst_offset /= sizeof(uint64_t);
    src_offset = 0;
    for (i = 0; i < EBPF_BLOCK_SIZE / sizeof(uint64_t); ++i, ++src_offset, ++dst_offset) {
        *(dst_ptr + (dst_offset & (EBPF_CONTEXT_MASK >> 3))) = *(src_ptr + (src_offset & (EBPF_CONTEXT_MASK >> 3)));
    }
    */

    ++scratch->nr_page;


    switch (header->type) {
    case EBPF_PAGE_ROW_INT:
        ret = ebpf_search_int_page(context, offsetof(struct wt_ebpf_scratch, key), scratch->key_size, &descent_offset, &descent_index);
        if (ret == 1 || ret == 2 || ret == 3) {
            scratch->descent_index_arr[scratch->level & EBPF_DEPTH_MASK] = descent_index;
            /* fill control fields in the context */
            if (scratch->level == EBPF_MAX_DEPTH - 1) {
                /* buffer is full, return to the application immediately */
                context->done = ret | (0x1 << 4);
            } else {
                context->done = ret;
                context->next_addr[0] = descent_offset;
                context->size[0] = EBPF_BLOCK_SIZE;
            }
            /* update scratch */
            ++scratch->level;

            ret = 0;
        } else {
            printk("wiredtiger_lookup: ebpf_search_int_page failed, ret %d", ret);
        }
        break;
    case EBPF_PAGE_ROW_LEAF:
        context->done = (0x1 << 4);
        /* reach leaf page, return to the application immediately */
        ret = 0;
        break;
    default:
        printk("wiredtiger_lookup: unknown page type %d", header->type);
        ret = -EBPF_EINVAL;
    }
    return -1 * ret;
}

int resubmit_logic_single(int sqid, int sq_entry, struct nvmev_io_work *w)
{
	struct nvmev_submission_queue *sq = nvmev_vdev->sqes[sqid];
    int qid;

    struct bpf_prog *ebpf_prog;
    struct bpf_xrp_kern ebpf_context;
    u32 ebpf_return;
    loff_t file_offset = 0, data_len;
    u64 disk_offset;

    struct xrp_mapping mapping;

    struct resubmit_data *on_meta = w->on_meta;
    struct nvme_rw_command *cmd = (struct nvme_rw_command *)on_meta->xrp_command;

    memset(&ebpf_context, 0, sizeof(struct bpf_xrp_kern));

    ebpf_context.data = on_meta->xrp_data_page;
    ebpf_context.scratch = on_meta->xrp_scratch_page;
    ebpf_prog = on_meta->xrp_bpf_prog;

    ebpf_return = BPF_PROG_RUN(ebpf_prog, &ebpf_context);

    if (ebpf_return != 0 || ebpf_return == EINVAL) {
        printk("nvme_handle_cqe: ebpf failed, dump context\n");
        return -1;
    }

    if (ebpf_context.done) {
        return 1;
    }

    file_offset = ebpf_context.next_addr[0];
    data_len = 512;

    // FIXME: support variable data_len and more than one next_addr
    if (on_meta->i_op == &ext4_file_inode_operations) {
        memset(&mapping, 0, sizeof(struct xrp_mapping));
        driver_retrieve_mapping(on_meta->extents, on_meta->num_extents, file_offset, data_len, &mapping);
        //xrp_retrieve_mapping(on_meta->inode, file_offset, data_len, &mapping);
		if (!mapping.exist || mapping.len < data_len || mapping.address & 0x1ff) {
			struct xrp_scratch *scratch = (struct xrp_scratch *)on_meta->xrp_scratch_page;
			struct value *val = &scratch->values[0];

            //ebpf_dump_page((uint8_t *) ebpf_context.scratch, 4096);
            printk("nvme_handle_cqe: mapping does not exist\n");
			return -1;
        } else {
            disk_offset = mapping.address;
        }
    } else {
        /* no address translation, use direct map */
        disk_offset = file_offset;
    }

    on_meta->slba = cpu_to_le64(nvme_sect_to_lba(on_meta->queuedata, (disk_offset >> 9) + on_meta->xrp_partition_start_sector));
    cmd->slba = on_meta->slba;
    w->slba = on_meta->slba;
    /*
    if (w->on_meta == sq_entry(sq_entry).rw.rsvd2) {
        sq_entry(sq_entry).rw.slba = cmd->slba;
    }
    */
    return 0;
}

int resubmit_logic(int sqid, int sq_entry, struct nvmev_io_work *w, int subtask)
{
	struct nvmev_submission_queue *sq = nvmev_vdev->sqes[sqid];
    int qid;

    struct bpf_prog *ebpf_prog;
    struct bpf_xrp_kern ebpf_context;
    u32 ebpf_return;
    loff_t file_offset = 0, data_len;
    u64 disk_offset;

    struct xrp_mapping mapping;
                        
    struct resubmit_data *on_meta = w->on_meta;
    struct nvme_rw_command *cmd;

    cmd = (struct nvme_rw_command *)on_meta->xrp_command;
    memset(&ebpf_context, 0, sizeof(struct bpf_xrp_kern));

    /*
    if (subtask == 0) {
        uint64_t next_offset = ebpf_context.next_addr[0];
        ebpf_context.data = on_meta->xrp_data_page;
        ebpf_prog = on_meta->xrp_bpf_prog;
        ebpf_context.scratch = (char *)on_meta->xrp_scratch_page;
        ebpf_context.done = 0;
        ebpf_return = BPF_PROG_RUN(ebpf_prog, &ebpf_context);
        printk("VERIFY %1d - result %d, %d\n", subtask, ebpf_context.done, ebpf_context.next_addr[0]);
        memset(&ebpf_context, 0, sizeof(struct bpf_xrp_kern));
    }
    */

    ebpf_context.data = on_meta->xrp_data_page;
    ebpf_context.scratch = (char *)on_meta->subtask_scratch[subtask];
    ebpf_context.done = subtask + 1;
    ebpf_prog = on_meta->xrp_bpf_prog;

    ebpf_return = BPF_PROG_RUN(ebpf_prog, &ebpf_context);
    //ebpf_return = wiredtiger_lookup(&ebpf_context);

    //printk("%1d - result %d, %d\n", 
    //        subtask, ebpf_context.done, ebpf_context.next_addr[0]);


    if (ebpf_return != 0 || ebpf_return == EINVAL) {
        w->subtask_status[subtask] = 2;
        //printk("nvme_handle_cqe: ebpf failed, dump context\n");
        //printk("\tret %d, fail to search\n", subtask);
        return -1;
    }

    //printk("%1d - result %d, %d\n", subtask, ebpf_context.done, ebpf_context.next_addr[0]);
    w->done_status[subtask] = ebpf_context.done & 0x7;

    ebpf_context.done >>= 4;

    if (ebpf_context.done) {
        w->subtask_status[subtask] = 1;
        //printk("ret %d, search done\n", subtask);
        return 1;
    }

    file_offset = ebpf_context.next_addr[0];
    data_len = 512;

    //printk("\t%d: %16lx", subtask, file_offset);
    w->subtask_status[subtask] = file_offset;
    return 0;

    /*
    // FIXME: support variable data_len and more than one next_addr
    if (on_meta->i_op == &ext4_file_inode_operations) {
        memset(&mapping, 0, sizeof(struct xrp_mapping));
        driver_retrieve_mapping(on_meta->extents, on_meta->num_extents, file_offset, data_len, &mapping);
        //xrp_retrieve_mapping(on_meta->inode, file_offset, data_len, &mapping);
		if (!mapping.exist || mapping.len < data_len || mapping.address & 0x1ff) {
			struct xrp_scratch *scratch = (struct xrp_scratch *)on_meta->xrp_scratch_page;
			struct value *val = &scratch->values[0];

            //ebpf_dump_page((uint8_t *) ebpf_context.scratch, 4096);
            printk("nvme_handle_cqe: mapping does not exist\n");
			return -1;
        } else {
            disk_offset = mapping.address;
        }
    } else {
        disk_offset = file_offset;
    }

    cmd->slba = cpu_to_le64(nvme_sect_to_lba(on_meta->queuedata, (disk_offset >> 9) + on_meta->xrp_partition_start_sector));
    atomic64_set(&on_meta->slba, cpu_to_le64(nvme_sect_to_lba(on_meta->queuedata, (disk_offset >> 9) + on_meta->xrp_partition_start_sector)));
    w->slba = atomic64_read(&on_meta->slba);
    */
    //return 0;
}

static void nvmev_init_latency_emulation(struct nvmev_io_work *w, unsigned long long artificial_delay)
{
	struct nvmev_submission_queue *sq = nvmev_vdev->sqes[w->sqid];
	unsigned long long nsecs_start = __get_wallclock();
    struct resubmit_data *on_meta = w->on_meta;
	struct nvme_command *cmd = on_meta->xrp_command;
#if (BASE_SSD == KV_PROTOTYPE)
	uint32_t nsid = 0; // Some KVSSD programs give 0 as nsid for KV IO
#else
	uint32_t nsid = cmd->common.nsid - 1;
#endif
    struct nvmev_ns *ns = &nvmev_vdev->ns[nsid];

	struct nvmev_request req = {
		.cmd = cmd,
		.sq_id = w->sqid,
		.nsecs_start = nsecs_start,
	};
	struct nvmev_result ret = {
		.nsecs_target = nsecs_start,
		.status = NVME_SC_SUCCESS,
    };

    if (!ns->proc_io_cmd(ns, &req, &ret)) {
        printk("ERROR");
    }
    w->nsecs_start = nsecs_start;
    w->nsecs_enqueue = local_clock();
    w->nsecs_target = ret.nsecs_target;
}

//static atomic_t xrp_waiting;

static volatile int resubmit_waiting[NUM_R_CPU];
static unsigned int target_rq[NUM_R_CPU];
static unsigned int curr_rq[NUM_R_CPU];

static int nvmev_resubmit_worker(void *data)
{
	struct nvmev_io_worker *my_worker = (struct nvmev_io_worker *)data;
	struct nvmev_io_worker *worker;
    size_t io_size = 0;
        
    int id;
        
    int existing_job_id = 0;

    int r_count_goal;

	NVMEV_INFO("resubmit started on cpu %d (node %d)\n", smp_processor_id(),
	    cpu_to_node(smp_processor_id()));

    id = smp_processor_id();
        
    existing_job_id = my_worker->id;

    while (!kthread_should_stop()) {
        //int tracking = 0;

        bool is_parallel = false;

        r_count_goal = 0;
        worker = NULL;

        /*
        if (atomic_read(&my_worker->resubmit_waiting) > 0) {
            preempt_disable();
            local_irq_disable();
            spin_lock(&my_worker->lock);
            curr = my_worker->r_seq;
            tail = my_worker->r_seq_end;
            r_count_goal = my_worker->r_count_goal;
            if (r_count_goal > 0) {
                my_worker->r_count_goal = 0;
                my_worker->r_seq = -1;
                my_worker->r_seq_end = -1;

                worker = my_worker;
            }
            spin_unlock(&my_worker->lock);
        }
        else {
            cond_resched();
        }
        */

        //if (r_count_goal == 0) {
        /*
        if (false) {
            struct nvmev_io_worker *other_worker;
            unsigned int other_worker_id = (my_worker->id + 1) % nvmev_vdev->config.nr_io_workers;
            other_worker = &nvmev_vdev->io_workers[other_worker_id];

            while (1) {
                if (other_worker->r_count_goal > 0) {
                    spin_lock(&other_worker->lock);
                    curr = other_worker->r_seq;
                    tail = other_worker->r_seq_end;
                    r_count_goal = other_worker->r_count_goal;
                    if (r_count_goal > 0) {
                        other_worker->r_count_goal = 0;
                        other_worker->r_seq = -1;
                        other_worker->r_seq_end = -1;

                        worker = other_worker;

                        atomic_sub(r_count_goal, &worker->resubmit_waiting);
                    }
                    spin_unlock(&other_worker->lock);
                    break;
                }
                else {
                    other_worker_id = (other_worker_id + 1) % nvmev_vdev->config.nr_io_workers;
                    if (other_worker_id == my_worker->id) {
                        other_worker = NULL;
                        break;
                    }
                    other_worker = &nvmev_vdev->io_workers[other_worker_id];
                }
            }
        }
        */
            
        //printk("\t\tWORK %d [%d]", curr, r_count);
                

        /*
        if (atomic_read(&my_worker->resubmit_waiting) > 0) {
            for (i = 0; i < 2; i++) {
                if (atomic_read(&my_worker->split_job[i]) != my_worker->id) {
                    preempt_disable();
                    local_irq_disable();

                    worker = &nvmev_vdev->io_workers[atomic_read(&my_worker->split_job[i])];
                    is_parallel= true;
                    tracking = i;

                    // Get curr
                    curr = atomic_read(&my_worker->split_curr[i]);
                    r_count_goal = 1;
                    break;
                }
            }
        }
        */

        r_count_goal = resubmit_waiting[existing_job_id];

        preempt_disable();
        local_irq_disable();

        while (r_count_goal > 0) {
            struct nvmev_io_work *w;
            int ret;
            u64 ebpf_time_end;
            struct wt_ebpf_scratch *scratch;
            uint64_t descent_index;

            int parallel_jobs = 0;
            int job_num = 0;

            bool all_zero = false;
            u64 etc;

            int i;

            int idx;
            int loc;
            struct resubmit_data *on_meta;
            u32 prev_pkru;

            loc = curr_rq[existing_job_id];
            idx = target_rq[existing_job_id];
            target_rq[existing_job_id] = 5;
            resubmit_waiting[existing_job_id] -= 1;

            worker = &nvmev_vdev->io_workers[idx];
            if (worker->id != my_worker->id) {
                is_parallel = true;
            }
            else {
                is_parallel = false;
            }

            w = &worker->work_queue[WQ(loc)][QE(loc)];

            //w->subtask_status[existing_job_id] = 0;
            //w->on_meta = atomic64_read(&my_worker->on_meta[loc]);

            on_meta = w->on_meta;

            //printk("%1d JOB %d, %d\n", my_worker->id, curr, worker->id);

            if (on_meta->is_parallel && !is_parallel) {
                //ebpf_time = ktime_get_ns();
                prev_pkru = read_pkru();
                write_pkru((u32)(-1));
                ret = resubmit_logic(w->sqid, w->sq_entry, w, existing_job_id);
                //ebpf_time_end = ktime_get_ns();
            }
            else if (on_meta->is_parallel && is_parallel) {
                //ebpf_time = ktime_get_ns();
                ret = resubmit_logic(w->sqid, w->sq_entry, w, existing_job_id);
                //ebpf_time_end = ktime_get_ns();
            }
            else {
                u32 prev_pkru;
                //ebpf_time = ktime_get_ns();
                prev_pkru = read_pkru();
                write_pkru((u32)(-1));
                ret = resubmit_logic_single(w->sqid, w->sq_entry, w);
                write_pkru(prev_pkru);
                //ebpf_time_end = ktime_get_ns();
            }

            // for parallel resubmission
            w->subtask_result[existing_job_id] = 1;
            //printk("%1d JOB %d, %d...done\n", my_worker->id, curr, worker->id);

            if (!is_parallel && on_meta->is_parallel) {
                u64 max_offset = 0;
                int target_scratch = -1;
                struct wt_ebpf_scratch *scratch, *target;
                uint64_t *scratch_ptr, *target_ptr;
                uint64_t offset, src_offset;

                scratch_ptr = (uint64_t *)on_meta->xrp_data_page;
                target = (struct wt_ebpf_scratch *)(on_meta->xrp_scratch_page);

                target_ptr = (uint64_t *)target;
                    
                offset = 1024;
                offset += 512 * target->level;
                offset /= sizeof(uint64_t);
                src_offset = 0;
                for (i = 0; i < 512 / sizeof(uint64_t); ++i, ++offset, ++src_offset) {
                    *(target_ptr + (offset & (0xfff >> 3))) = *(scratch_ptr + (src_offset & (0xfff >> 3)));
                }

                parallel_jobs = 0;
                while (parallel_jobs != 4) {
                    parallel_jobs = 0;
                    for (job_num = 0; job_num < 4; job_num++) {
                        parallel_jobs += w->subtask_result[job_num];
                    }
                }

                write_pkru(prev_pkru);

                // Check next offset or is_done
                max_offset = 0;
                for (job_num = 0; job_num < 4; job_num++) {
                    if (w->subtask_status[job_num] == 1) {
                        int j = 0;
                        int zero = 0, one = 0, two = 0, three = 0;

                        if (max_offset == 0) {
                            max_offset = 1;
                            for (j = 0; j < 4; j++) {
                                if (w->done_status[j] == 0) {
                                    zero++;
                                }
                                else if (w->done_status[j] == 1) {
                                    target_scratch = j;
                                    break;
                                }
                                else if (w->done_status[j] == 2) {
                                    two++;
                                }
                                else if (w->done_status[j] == 3) {
                                    three++;
                                }
                                //printk("[%d] DONE STTAUS %d", j, w->done_status[j]);
                            }
                            if (zero == 4) {
                                target_scratch = job_num;
                                all_zero = true;
                            }
                            else if (one == 0 && two > 0 && three > 0) {
                                for (j = 0; j < 4; j++) {
                                    if (w->done_status[j] == 2) {
                                        target_scratch = j;
                                        break;
                                    }
                                }
                            }
                            else if (one == 0 && two > 0 && three == 0) {
                                target_scratch = 0;
                            }
                            else if (one == 0 && two == 0 && three > 0) {
                                target_scratch = 3;
                            }
                        }
                        else {
                            printk("Exception: 1169");
                        }
                        break;
                    }

                    if (max_offset < w->subtask_status[job_num]) {
                        max_offset = w->subtask_status[job_num];
                        target_scratch = job_num;
                    }
                }

                //printk("TARGET NEXT OFFSET %d, Target %d", max_offset, target_scratch);
                if (max_offset != 1) {
                    struct xrp_mapping mapping;

                    u64 disk_offset;
                    loff_t file_offset;
                    struct nvme_rw_command *cmd = (struct nvme_rw_command *)on_meta->xrp_command;

                    struct wt_ebpf_scratch *scratch, *target;
                    uint64_t *scratch_ptr, *target_ptr;
                    uint64_t offset;
                    uint64_t src_offset = 0;
                    int i = 0;

                    //u64 copy_time;
                    
                    // Get the next offset
                    // FIXME: support variable data_len and more than one next_addr
                    file_offset = max_offset;
                    disk_offset = 0;

                    //printk("file offset %d", file_offset);
                    if (on_meta->i_op == &ext4_file_inode_operations) {
                        memset(&mapping, 0, sizeof(struct xrp_mapping));
                        driver_retrieve_mapping(on_meta->extents, on_meta->num_extents, file_offset, 512, &mapping);
                        //xrp_retrieve_mapping(on_meta->inode, file_offset, data_len, &mapping);
                        if (!mapping.exist || mapping.len < 512 || mapping.address & 0x1ff) {
                            struct xrp_scratch *scratch = (struct xrp_scratch *)on_meta->xrp_scratch_page;
                            struct value *val = &scratch->values[0];

                            //ebpf_dump_page((uint8_t *) ebpf_context.scratch, 4096);
                            printk("nvme_handle_cqe: mapping does not exist\n");
                        } else {
                            disk_offset = mapping.address;
                        }
                    } else {
                        disk_offset = file_offset;
                    }

                    if (disk_offset != 0) {
                        uint64_t src_offset;
                        on_meta->slba = cpu_to_le64(nvme_sect_to_lba(on_meta->queuedata, (disk_offset >> 9) + on_meta->xrp_partition_start_sector));
                        cmd->slba = on_meta->slba;

                        w->slba = on_meta->slba;

                        // Set scratch page

                        /*
                        memcpy(on_meta->xrp_scratch_page, on_meta->subtask_scratch[target_scratch], PAGE_SIZE);

                        for (job_num = 0; job_num < 4; job_num++) {
                            if (job_num != target_scratch) {
                                memcpy(on_meta->subtask_scratch[job_num], on_meta->subtask_scratch[target_scratch], PAGE_SIZE);
                            }
                        }
                        */

                        //copy_time = ktime_get_ns();
                        scratch = (struct wt_ebpf_scratch *)on_meta->subtask_scratch[target_scratch];

                        target = (struct wt_ebpf_scratch *)(on_meta->xrp_scratch_page);
                        target->descent_index_arr[(scratch->level - 1) & 0x7] = scratch->descent_index_arr[(scratch->level - 1) & 0x7]; 

                        target->nr_page = scratch->nr_page;
                        target->level = scratch->level;

                        //printk("DESCENT %d", scratch->descent_index_arr[scratch->level - 1]);

                        //scratch = (struct wt_ebpf_scratch *)on_meta->xrp_scratch_page;
                        //printk("ANSWER  %d", scratch->descent_index_arr[scratch->level - 1]);

                        scratch = (struct wt_ebpf_scratch *)on_meta->subtask_scratch[target_scratch];
                        for (job_num = 0; job_num < 4; job_num++) {
                            if (job_num != target_scratch) {
                                target = (struct wt_ebpf_scratch *)(on_meta->subtask_scratch[job_num]);
                                target->descent_index_arr[(scratch->level - 1) & 0x7] = scratch->descent_index_arr[(scratch->level - 1) & 0x7]; 
                                target->nr_page = scratch->nr_page;
                                target->level = scratch->level;
                            }
                        }

                        ret = 0;
                    }
                    else {
                        //printk("LOG %d, %d", ktime_get_ns() - copy_time, ktime_get_ns() - ebpf_time);
                        printk("DEBUG");
                        ret = 1;
                    }
                }
                else {
                    struct wt_ebpf_scratch *scratch, *target;
                    uint64_t *scratch_ptr, *target_ptr;
                    uint64_t offset;
                    uint64_t src_offset;

                    scratch = (struct wt_ebpf_scratch *)on_meta->subtask_scratch[target_scratch];

                    target = (struct wt_ebpf_scratch *)(on_meta->xrp_scratch_page);
                    if (!all_zero) {
                        target->descent_index_arr[(scratch->level - 1) & 0x7] = scratch->descent_index_arr[(scratch->level - 1) & 0x7]; 
                    }

                    target->nr_page = scratch->nr_page;
                    target->level = scratch->level;

                    /*
                    target = (struct wt_ebpf_scratch *)(w->on_meta->xrp_scratch_page);
                    scratch = (struct wt_ebpf_scratch *)w->on_meta->subtask_scratch[target_scratch];
                    target_ptr = (uint64_t *)target;
                    scratch_ptr = (uint64_t *)scratch;

                    printk("--------------------------HELL");
                    printk("%d, %d vs. %d, %d", target->nr_page, target->level, scratch->nr_page, scratch->level);
                    for (i = 0; i < target->level; ++i) {
                        printk("%016lx\t\t%016lx", target->descent_index_arr[i], scratch->descent_index_arr[i]);
                    }
                    printk("--------------------------HELL...END");
                    */

                    //memcpy(w->on_meta->xrp_scratch_page, w->on_meta->subtask_scratch[target_scratch], PAGE_SIZE);

                    ret = 1;
                }
            }
            else if (on_meta->is_parallel) {
                // Done
                ret = -2;
            }

            if (ret != 0 && ret != -2) {
                on_meta->status = 2;
                    
                //printk("QOUT2 on_meta=%016lx, %d, %d", w->on_meta, worker->id, curr);

                kunmap_atomic(on_meta);
                w->on_meta = NULL;

                __fill_cq_result_complete(w);

                w->is_completed = true;
                smp_mb();

                w->is_resubmit = false;
            }
            else if (ret != -2) {
                nvmev_init_latency_emulation(w, 0);

                w->is_copied = false;
                smp_mb();

                w->is_resubmit = false;
            }

#ifdef PROFILE
            printk("LOG %lu, %lu, %lu, %lu, %lu", ebpf_time, ebpf_time_end, etc, time_beg, time_end);
#endif

            /*
            worker->total_ebpf_time += ebpf_time_end - ebpf_time;
            worker->total_ebpf_num += 1;

            record[record_idx] = ebpf_time_end - ebpf_time + time_end - time_beg;
            record_idx = (record_idx + 1) % moving;
            */

            /*
            record[record_idx] = ebpf_time_end - ebpf_time + time_end - time_beg;
            recording_time = ktime_get_ns();
            record_time[record_idx] = recording_time;
            record_idx = (record_idx + 1) % moving;
            
            avg_time = 0;
            avg_case = 0;
            for (i = 0; i < moving; i++) {
                if (((int)recording_time - (int)record_time[i]) <= 1000000000) {
                    avg_time += (int)record[i];
                    avg_case++;
                }
                else {
                    record[i] = 0;
                    record_time[i] = 0;
                }
            }
            
            worker->ebpf_time = avg_time / avg_case;
            */

            /*
            total_resubmit_time += time_end - time_beg;
            w->rest_time = time_end - time_beg;

            if (w->logic_time > max_logic) {
                max_logic = w->logic_time;
                max_ebpf = w->ebpf_time;
                max_rest = w->rest_time;
                //max_descent_index = descent_index;
            }
            
            if (w->logic_time < min_logic) {
                min_logic = w->logic_time;
                min_ebpf = w->ebpf_time;
                min_rest = w->rest_time;
                //min_descent_index = descent_index;
            }

            resubmit_cnt += 1;
            if ((resubmit_cnt % 1000000) == 0) {
                printk("in-device: logic %d ns, rest %d ns",
                        total_resubmit_logic_time / resubmit_cnt,
                        total_resubmit_time / resubmit_cnt,
                        worker->ebpf_time
                        );
                printk("MIN %d(%d) + %d <%d>, MAX %d(%d) + %d <%d>",
                        min_logic, min_ebpf, min_rest, min_descent_index,
                        max_logic, max_ebpf, max_rest, max_descent_index
                        );
                total_resubmit_logic_time = 0;
                total_resubmit_time = 0;
                resubmit_cnt = 0;
            }
            */

            r_count_goal = resubmit_waiting[existing_job_id];
        }

        local_irq_enable();
        preempt_enable();


        cond_resched();
    }

    return 0;
}

spinlock_t global_worker_lock;

static int nvmev_io_worker(void *data)
{
	struct nvmev_io_worker *worker = (struct nvmev_io_worker *)data;
	struct nvmev_ns *ns;

#ifdef PERF_DEBUG
	static unsigned long long intr_clock[NR_MAX_IO_QUEUE + 1];
	static unsigned long long intr_counter[NR_MAX_IO_QUEUE + 1];

	unsigned long long prev_clock;
#endif

    u64 xrp_ebpf_time = 2500;
    //int counter = 0;


    int in = 0, out = 0;
    int id = smp_processor_id();
    int waiting = 0, prev_waiting = 0;

    int i = 0;

	NVMEV_INFO("%s started on cpu %d (node %d)\n", worker->thread_name, smp_processor_id(),
		   cpu_to_node(smp_processor_id()));

	while (!kthread_should_stop()) {
		unsigned long long curr_nsecs_wall = __get_wallclock();
		unsigned long long curr_nsecs_local = local_clock();
		long long delta = curr_nsecs_wall - curr_nsecs_local;

		volatile unsigned int curr = worker->io_seq;
        struct nvmev_io_work *w;
		int qidx;
    
        //printk("HEAD %d", curr);
        while (curr != -1) {
			unsigned long long curr_nsecs;
            bool is_copied_local;

			curr_nsecs = local_clock() + delta;
            w = &worker->work_queue[WQ(curr)][QE(curr)];
			worker->latest_nsecs = curr_nsecs;

			if (w->is_resubmit == true || w->is_completed == true) {
				curr = w->next;
				continue;
			}


			if (w->is_copied == false) {
#ifdef PERF_DEBUG
				w->nsecs_copy_start = local_clock() + delta;
#endif
				if (w->is_internal) {
                    ;
				} else if (io_using_dma) {
					__do_perform_io_using_dma(w->sqid, w->sq_entry);
				} else {
#if (BASE_SSD == KV_PROTOTYPE)
					struct nvmev_submission_queue *sq =
						nvmev_vdev->sqes[w->sqid];
					ns = &nvmev_vdev->ns[0];
					if (ns->identify_io_cmd(ns, sq_entry(w->sq_entry))) {
						w->result0 = ns->perform_io_cmd(
							ns, &sq_entry(w->sq_entry), &(w->status));
					} else {
						__do_perform_io(w->sqid, w->sq_entry, w);
					}
#else 
					__do_perform_io(w->sqid, w->sq_entry, w);
#endif
				}

#ifdef PERF_DEBUG
				w->nsecs_copy_done = local_clock() + delta;
#endif
				w->is_copied = true;

				NVMEV_DEBUG_VERBOSE("%s: copied %u, %d %d %d\n", worker->thread_name, curr,
					    w->sqid, w->cqid, w->sq_entry);
			}
            
			if (w->nsecs_target <= curr_nsecs) {
                if (w->is_sode && w->is_resubmit == false) {
                    int subtask_idx;
                    struct nvmev_io_worker *other_worker;
                    struct resubmit_data *on_meta;

                    struct ebpf_page_header *header;
                    //struct wt_ebpf_scratch *scratch;

#ifdef SINGLE_MODE 
                    w->on_meta->is_parallel = false;
#endif
                    if (w->on_meta->is_parallel == false) {
                        //struct Node *node = (struct Node *)w->on_meta->xrp_data_page;
                        waiting = resubmit_waiting[worker->id];
                        //if (waiting <= (RQ_LEN - 1) && waiting * worker->ebpf_time < (xrp_ebpf_time + 2000)) {
                        if (waiting == 0) {
                            w->is_resubmit = true;

                            target_rq[worker->id] = worker->id;
                            curr_rq[worker->id] = curr;
                            resubmit_waiting[worker->id] += 1;

                            curr = w->next;
                            continue;
                        }
                    }
                    else {
                        if (spin_trylock(&global_worker_lock)) {
                            header = (struct ebpf_page_header *)w->on_meta->xrp_data_page;

                            waiting = 0;
                            for (subtask_idx = 0; subtask_idx < 4; subtask_idx++) {
                                waiting += resubmit_waiting[subtask_idx];
                            }

                            //972
                            //printk("PAGES %d, %d", w->on_meta->xrp_bpf_prog->jited_len, w->on_meta->xrp_bpf_prog->len);
                            //scratch = (struct wt_ebpf_scratch *)(w->on_meta->xrp_scratch_page);
                            //if ((worker->ebpf_time < (xrp_ebpf_time + 2500)) && waiting == 0 && header->type != 7
                            if (waiting == 0 && header->type != 7 && (header->u.entries / 2 <= 16)) {
                                int i = 0;

                                w->is_resubmit = true;
                                for (i = 0; i < NUM_R_CPU; i++) {
                                    w->subtask_status[i] = 0;
                                    w->subtask_result[i] = 0;
                                    w->done_status[i] = 0;
                                }

                                // Main job
                                target_rq[worker->id] = worker->id;
                                curr_rq[worker->id] = curr;
                                resubmit_waiting[worker->id] += 1;

                                for (subtask_idx = 0; subtask_idx < 4; subtask_idx++) {
                                    other_worker = &nvmev_vdev->io_workers[subtask_idx];
                                    if (worker->id != other_worker->id) {
                                        target_rq[subtask_idx] = worker->id;
                                        curr_rq[subtask_idx] = curr;
                                        resubmit_waiting[subtask_idx] += 1;
                                    }
                                }

                                spin_unlock(&global_worker_lock);

                                curr = w->next;
                                continue;
                            }
                            spin_unlock(&global_worker_lock);
                        }
                    }

                    //printk("QOUT1 on_meta=%016lx, %d, %d", w->on_meta, worker->id, curr);

                    on_meta = w->on_meta;
                    on_meta->status = -1;
                    kunmap_atomic(w->on_meta);
                    w->on_meta = NULL;

                    __fill_cq_result_complete(w);

                    mb();
                    w->is_completed = true;

                    curr = w->next;
                    continue;
                }

				if (w->is_internal) {
#if (SUPPORTED_SSD_TYPE(CONV) || SUPPORTED_SSD_TYPE(ZNS))
					buffer_release((struct buffer *)w->write_buffer,
						       w->buffs_to_release);
#endif
				} else {
					__fill_cq_result(w);
				}

				NVMEV_DEBUG_VERBOSE("%s: completed %u, %d %d %d\n", worker->thread_name, curr,
					    w->sqid, w->cqid, w->sq_entry);

#ifdef PERF_DEBUG
				w->nsecs_cq_filled = local_clock() + delta;
				trace_printk("%llu %llu %llu %llu %llu %llu\n", w->nsecs_start,
					     w->nsecs_enqueue - w->nsecs_start,
					     w->nsecs_copy_start - w->nsecs_start,
					     w->nsecs_copy_done - w->nsecs_start,
					     w->nsecs_cq_filled - w->nsecs_start,
					     w->nsecs_target - w->nsecs_start);
#endif
				mb(); /* Reclaimer shall see after here */
				w->is_completed = true;
			}

			curr = w->next;
		}

        /*
        counter += 1;
        if ((counter % 100000000) == 0) {
            printk("<%d> in-storage %d, in-kernel %d, %d, %d, %d", id, in, out, worker->ebpf_time, get_curr_roundtrip_latency(), xrp_ebpf_time);
            in = 0;
            out = 0;
            counter = 0;
        }
        */

		for (qidx = 1; qidx <= nvmev_vdev->nr_cq; qidx++) {
			struct nvmev_completion_queue *cq = nvmev_vdev->cqes[qidx];

#ifdef CONFIG_NVMEV_IO_WORKER_BY_SQ
			if ((worker->id) != __get_io_worker(qidx))
				continue;
#endif
			if (cq == NULL || !cq->irq_enabled)
				continue;

			if (spin_trylock(&cq->irq_lock)) {
				if (cq->interrupt_ready == true) {
#ifdef PERF_DEBUG
					prev_clock = local_clock();
#endif
					cq->interrupt_ready = false;
					nvmev_signal_irq(cq->irq_vector);

#ifdef PERF_DEBUG
					intr_clock[qidx] += (local_clock() - prev_clock);
					intr_counter[qidx]++;

					if (intr_counter[qidx] > 1000) {
						NVMEV_DEBUG("Intr %d: %llu\n", qidx,
							    intr_clock[qidx] / intr_counter[qidx]);
						intr_clock[qidx] = 0;
						intr_counter[qidx] = 0;
					}
#endif
				}
				spin_unlock(&cq->irq_lock);
			}
		}
		cond_resched();
	}

	return 0;
}

void NVMEV_IO_WORKER_INIT(struct nvmev_dev *nvmev_vdev)
{
	unsigned int i, worker_id;

	nvmev_vdev->io_workers =
		kcalloc(sizeof(struct nvmev_io_worker), nvmev_vdev->config.nr_io_workers, GFP_KERNEL);
	nvmev_vdev->io_worker_turn = 0;

    //atomic_set(&xrp_waiting, 0);
    spin_lock_init(&global_worker_lock);

	for (worker_id = 0; worker_id < nvmev_vdev->config.nr_io_workers; worker_id++) {
		struct nvmev_io_worker *worker = &nvmev_vdev->io_workers[worker_id];
        struct nvmev_io_work *work_queue;
        int j;
        int last;
        int queue = 0;

        worker->working_signal = false;

        for (j = 0; j < NUM_R_CPU; j++) {
            work_queue =
                kzalloc(sizeof(struct nvmev_io_work) * NR_MAX_PARALLEL_IO, GFP_KERNEL);

            worker->work_queue[j] = work_queue;

            if (work_queue == NULL) {
                break;
            }

            queue += 1;

            for (i = 0; i < NR_MAX_PARALLEL_IO; i++) {
                worker->work_queue[j][i].next = j * NR_MAX_PARALLEL_IO + i + 1;
                worker->work_queue[j][i].prev = j * NR_MAX_PARALLEL_IO + i - 1;

                worker->work_queue[j][i].is_completed = true;
                worker->work_queue[j][i].is_copied = true;
                worker->work_queue[j][i].nsecs_target = 0;
                worker->work_queue[j][i].is_resubmit = false;
            }

        }

        last = NR_MAX_PARALLEL_IO * queue - 1;
        worker->work_queue[WQ(last)][QE(last)].next = -1;

		worker->id = worker_id;
		worker->free_seq = 0;
		worker->free_seq_end = NR_MAX_PARALLEL_IO * queue - 1;
		worker->io_seq = -1;
		worker->io_seq_end = -1;

#ifdef PROFILE
        worker->profiler = kzalloc(sizeof(struct profile) * NR_MAX_PARALLEL_IO, GFP_KERNEL);
#endif

        //spin_lock_init(&worker->lock);
        //worker->r_seq = -1;
        //worker->r_seq_end = -1;
        //worker->r_count_goal = 0;

        resubmit_waiting[worker_id] = 0;

        target_rq[worker_id] = 5;
        curr_rq[worker_id] = 0;

		snprintf(worker->thread_name, sizeof(worker->thread_name), "nvmev_io_worker_%d", worker_id);

		worker->task_struct = kthread_create(nvmev_io_worker, worker, "%s", worker->thread_name);
		kthread_bind(worker->task_struct, nvmev_vdev->config.cpu_nr_io_workers[worker_id]);

        worker->resubmit_task = kthread_create(nvmev_resubmit_worker, worker, "%s", "resubmit_thread");
        kthread_bind(worker->resubmit_task, nvmev_vdev->config.cpu_nr_io_workers[worker_id] + nvmev_vdev->config.nr_io_workers);
	}

	for (worker_id = 0; worker_id < nvmev_vdev->config.nr_io_workers; worker_id++) {
		struct nvmev_io_worker *worker = &nvmev_vdev->io_workers[worker_id];
		wake_up_process(worker->task_struct);
        wake_up_process(worker->resubmit_task);
    }
}

void NVMEV_IO_WORKER_FINAL(struct nvmev_dev *nvmev_vdev)
{
	unsigned int i;
    unsigned int j;

	for (i = 0; i < nvmev_vdev->config.nr_io_workers; i++) {
		struct nvmev_io_worker *worker = &nvmev_vdev->io_workers[i];

		if (!IS_ERR_OR_NULL(worker->task_struct)) {
			kthread_stop(worker->task_struct);
		}

        for (j = 0; j < NUM_R_CPU; j++) {
		    kfree(worker->work_queue[j]);
        }
	}

	kfree(nvmev_vdev->io_workers);
}
