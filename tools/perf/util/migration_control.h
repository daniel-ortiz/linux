#ifndef __MIGCTRL_HIST_H
#define __MIGCTRL_HIST_H


#define NUMATOOL_ERROR	0
#define	NUMATOOL_SUCCESS 1
#define RUN_COMMAND	0
#define RUN_ATTACHED 1
#define DEFAULT_SENSING_TIME 10


int init_numa_analysis(int mode, int pid,char **command_args,int command_argv);
int run_numa_analysis(void *arg);
void * measure_aux_thread(void *arg);
int __cmd_top(struct perf_top *top);
#endif
