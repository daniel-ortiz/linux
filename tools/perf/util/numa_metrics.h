#ifndef __NUMAMM_HIST_H
#define __NUMAMM_HIST_H

struct numa_metrics {
	int n_cpus;
	int pid_uo;
	int remote_accesses[32];
	int process_accesses[32];
};

int get_access_type(struct hist_entry *entry,int pid);

int main_numaan(int argc, const char **argv);
	

int filter_local_accesses(struct hist_entry *entry);
#endif
