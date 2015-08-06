#ifndef __NUMAMM_HIST_H
#define __NUMAMM_HIST_H

#include <uthash.h>
#define WEIGHT_BUCKETS_NR 19
#define WEIGHT_BUCKET_INTERVAL 50
#define LINE_SIZE 500
struct numa_metrics {
	int n_cpus;
	unsigned int pid_uo;
	int remote_accesses[32];
	int process_accesses[32];
	int cpu_to_processor[32];
	int logging_detail_level;
	int access_by_weight[WEIGHT_BUCKETS_NR];
	struct page_stats *page_accesses;
	struct access_stats *lvl_accesses;
	int moved_pages;
	FILE *report;
	char* report_filename;
	char* command2_launch;
	const char* file_label;
	};

struct page_stats{
	int proc0_acceses;
	int proc1_acceses;
	void* page_addr;
	UT_hash_handle hh;
};

struct access_stats{
	int count;
	int mem_lvl;
	UT_hash_handle hh;
};

static const char * const mem_lvl[] = {
	"N/A",
	"HIT",
	"MISS",
	"L1",
	"LFB",
	"L2",
	"L3",
	"Local RAM",
	"Remote RAM (1 hop)",
	"Remote RAM (2 hops)",
	"Remote Cache (1 hop)",
	"Remote Cache (2 hops)",
	"I/O",
	"Uncached",
};
#define NUM_MEM_LVL (sizeof(mem_lvl)/sizeof(const char *))

void sort_entries(struct numa_metrics *nm);

int do_migration(struct numa_metrics *nm, int pid, struct perf_sample *sample);

void init_processor_mapping(struct numa_metrics *multiproc_info);

void add_mem_access( struct numa_metrics *multiproc_info, void *page_addr, int accessing_cpu);

int filter_local_accesses(union perf_mem_data_src *entry);

void  print_migration_statistics(struct numa_metrics *nm);

void add_lvl_access( struct numa_metrics *multiproc_info, union perf_mem_data_src *entry, int weight );
	
void print_access_info(struct numa_metrics *multiproc_info);

char* print_access_type(int entry);

void init_report_file(struct numa_metrics *nm);

void close_report_file(struct numa_metrics *nm);

void launch_command(struct numa_metrics *nm, char** argv, int argc);

void print_info(FILE* file, const char* format, ...);

char ** put_end_params(char **argv,int argc);

char* get_command_string(const char ** argv, int argc);

long id_sort(struct page_stats *a, struct page_stats *b);
#endif
