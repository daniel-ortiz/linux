#ifndef __NUMAMM_HIST_H
#define __NUMAMM_HIST_H

struct numa_metrics {
	int n_cpus;
	int pid_uo;
	int remote_accesses[32];
	int process_accesses[32];
	int cpu_to_processor[32];
};

int get_access_type(struct hists *hists, struct hist_entry *entry,int pid);

void init_processor_mapping(struct numa_metrics *multiproc_info);

int main_numaan(int argc, const char **argv);
	

int filter_local_accesses(struct hist_entry *entry);
#endif
