#ifndef __NUMAMM_HIST_H
#define __NUMAMM_HIST_H

#include <uthash.h>
struct numa_metrics {
	int n_cpus;
	int pid_uo;
	int remote_accesses[32];
	int process_accesses[32];
	int cpu_to_processor[32];
	struct page_stats *page_accesses;
};

struct page_stats{
	int proc0_acceses;
	int proc1_acceses;
	void* page_addr;
	UT_hash_handle hh;
};

int do_migration(struct numa_metrics *nm, int pid, struct perf_sample *sample);

void init_processor_mapping(struct numa_metrics *multiproc_info);

void add_mem_access( struct numa_metrics *multiproc_info, void *page_addr, int accessing_cpu);

int filter_local_accesses(union perf_mem_data_src *entry);

void  print_migration_statistics(struct numa_metrics *nm);
#endif
