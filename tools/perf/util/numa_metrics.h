#ifndef __NUMAMM_HIST_H
#define __NUMAMM_HIST_H

struct numa_metrics {
	int n_cpus;
	int remote_accesses[32];
	int process_accesses[32];
};

int filter_local_accesses(struct hist_entry *entry);
#endif
