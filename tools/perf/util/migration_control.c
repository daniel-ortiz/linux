#include "migration_control.h"
#include "numa_metrics.h"
#include "util/top.h"
#include "util/util.h"
#include <pthread.h>


//This represents the main logic of the tool

int run_numa_analysis(void *arg){
	struct perf_top** tops ;
	pthread_t aux_thread;
	int j;
	
	tops=(struct perf_top**)arg;
	 
	//phase 1: will run the tool for a specific measurement period
	printf("MIG-CTRL> children thread launched\n");
	
	
	if(pthread_create(&aux_thread,NULL,measure_aux_thread,(void *) *(tops))){
		printf("MIG-CTRL> could not create aux thread \n");
	}
	
	//launch top and gather data
	__cmd_top(*(tops));
	
	pthread_join(aux_thread, NULL); 
	
}

int wait_watch_process(int seconds,struct numa_metrics* nm){
	int i=0,exit=0;
	int sigres,*st=0,errn;
	//every second it wakes up to make sure the process is alive

	while(!exit){
		sleep(1);
		sigres=kill(nm->pid_uo,0);
		waitpid(nm->pid_uo,st,WNOHANG);
		if(sigres==-1 ){
			errn=errno;
			if (errn== ESRCH || errn==ECHILD){
					return -1;
				}
			}
		exit= (seconds > 1 && ++i<seconds) || seconds < 1 ? 0 : 1;
	}
	return 0;
}

void * measure_aux_thread(void *arg){
	struct perf_top *top=(struct perf_top*) arg;
	int sleep_time=top->numa_sensing_time,wait_res;
	sleep_time=sleep_time<1 ?  DEFAULT_SENSING_TIME  : sleep_time;
	
	

	wait_res=wait_watch_process(sleep_time,top->numa_metrics);
	if(wait_res) goto end_noproc;
	printf("MIG-CTRL> End of sampling period\n");
	top->numa_analysis_enabled=false;
	//print the overall statistics before moving pages
	print_migration_statistics(top->numa_metrics);
	if(top->migrate_track_levels){
		print_access_info(top->numa_metrics);
	}
	if(top->migrate_filereport){
		//TODO review this
		//close_report_file(nm);
	}
	top->numa_metrics->page_accesses=NULL;
	top->numa_metrics->lvl_accesses=NULL;
	top->numa_metrics->moved_pages=0;
	//TODO adjust to core size
	memset(top->numa_metrics->remote_accesses ,0,32);
	memset(top->numa_metrics->process_accesses ,0,32);
	memset(top->numa_metrics->access_by_weight ,0,WEIGHT_BUCKETS_NR);

	
	printf("MIG-CTRL> Call page migration \n");
	do_great_migration(top->numa_metrics);
	top->numa_analysis_enabled=true;
	wait_res=wait_watch_process(-1,top->numa_metrics);

	print_migration_statistics(top->numa_metrics);
	if(top->migrate_track_levels){
		print_access_info(top->numa_metrics);
	}

	
end_noproc:
	printf("MIG-CTRL> End of measurement due to end of existing process");
	top->numa_metrics->timer_up=true;
	return NULL;
}

/*
 * Entry point to the application can run and analyze the process 
 * specified as parameter or can attach to an already existing process  
 */
 
int init_numa_analysis(int mode, int pid,const char **command_args,int command_argc ){
	struct perf_top *tops[2];
	pthread_t numatool_thread;

	struct numa_metrics* nm;
	//Numa-migrate stuff is initialized here
	nm=malloc(sizeof(struct numa_metrics));
	memset(nm, 0, sizeof(struct numa_metrics));
	nm->page_accesses=NULL;
	nm->lvl_accesses=NULL;
	nm->pages_2move=NULL;

	
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
	
	tops[0]=cmd_top(command_argc,command_args,NULL);
	
	nm->logging_detail_level=tops[0]->numa_migrate_logdetail;
	nm->moved_pages=0;
	tops[0]->numa_metrics=nm;
	nm->pid_uo=mode==RUN_ATTACHED ? tops[0]->numa_migrate_pid_filter : pid;
	nm->file_label=tops[0]->numa_filelabel;
	nm->timer_up=false;
	
	
	if (!tops[0])
		return NUMATOOL_ERROR;
		
	
	if(pthread_create(&numatool_thread,NULL,run_numa_analysis, tops)){
		return NUMATOOL_ERROR; 
	}
	
	//in case of using run_command will launch the process
	
	if(mode==RUN_COMMAND){
		nm->pid_uo=launch_command(command_args, command_argc);
	}
		
	pthread_join(numatool_thread, NULL); 
		return NUMATOOL_SUCCESS;
}

int main(int argc, const char **argv){
	int resp;
	
	page_size = sysconf(_SC_PAGE_SIZE);

	resp=init_numa_analysis(RUN_COMMAND, 0,argv, argc);
}

