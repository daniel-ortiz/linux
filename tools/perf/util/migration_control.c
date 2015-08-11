#include "migration_control.h"
#include "numa_metrics.h"
#include "util/top.h"
#include "util/util.h"
#include <pthread.h>



//This represents the main logic of the tool

int run_numa_analysis(void *arg){
	struct perf_top* top=(struct perf_top*)arg;
	pthread_t aux_thread;
	
	//phase 1: will run the tool for a specific measurement period
	printf("children thread launched\n");
	
	
	if(pthread_create(&aux_thread,NULL,measure_aux_thread, top)){
		printf("could not create aux thread \n");
	}
	
	//launch top and gather data
	__cmd_top(top);
	
	pthread_join(aux_thread, NULL); 
	printf("will now begin phase two");
	//relaunch top
}

void measure_aux_thread(void *arg){
	struct perf_top *top=(struct perf_top*) arg;
	printf("will try to fall asleep\n");
	sleep(5);
	printf("\n has woken up from thread");
	top->numa_metrics->timer_up=true;
}

/*
 * Entry point to the application can run and analyze the process 
 * specified as parameter or can attach to an already existing process  
 */
 
int init_numa_analysis(int mode, int pid,char **command_args,int command_argc ,char** perf_argv, int perf_argc){
	struct perf_top *top;
	pthread_t numatool_thread;
	int pid_uo=0;
	struct numa_metrics* nm;
	int x;
	//Numa-migrate stuff is initialized here
	nm=malloc(sizeof(struct numa_metrics));
	memset(nm, 0, sizeof(struct numa_metrics));
	nm->page_accesses=NULL;
	nm->lvl_accesses=NULL;
	
	
	
	init_processor_mapping(nm);
	
	if (mode== RUN_ATTACHED){
		if(pid==0)
			return NUMATOOL_ERROR;
			
	}
	else if (mode==RUN_COMMAND){

	}
	else{
		return NUMATOOL_ERROR;
	}
	
	top=cmd_top(perf_argc,perf_argv,NULL);
	
	nm->logging_detail_level=top->numa_migrate_logdetail;
	nm->moved_pages=0;
	top->numa_metrics=nm;
	nm->pid_uo=mode==RUN_ATTACHED ? top->numa_migrate_pid_filter : pid;
	nm->file_label=top->numa_filelabel;
	nm->timer_up=false;
	
	if (!top)
		return NUMATOOL_ERROR;
		
	
	if(pthread_create(&numatool_thread,NULL,run_numa_analysis, top)){
		return NUMATOOL_ERROR;
	}
	
	//in case of using run_command will launch the process
	
	if(mode==RUN_COMMAND)
		nm->pid_uo=launch_command(command_args, command_argc);
		
	pthread_join(numatool_thread, NULL); 
		return NUMATOOL_SUCCESS;
}

int main(int argc, const char **argv){
	int resp;
	
	page_size = sysconf(_SC_PAGE_SIZE);
	char* arr[]={"/usr/bin/uname"};
	resp=init_numa_analysis(RUN_COMMAND, 0,arr,1 ,argv, argc);
	printf("end of measurement %d \n", resp);
}

