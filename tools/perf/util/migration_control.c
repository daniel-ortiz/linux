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
	 //(unsigned int (*)[150]) 
	 
	//phase 1: will run the tool for a specific measurement period
	printf("children thread launched\n");
	
	
	if(pthread_create(&aux_thread,NULL,measure_aux_thread,(void *) *tops)){
		printf("could not create aux thread \n");
	}
	
	//launch top and gather data
	__cmd_top(*tops);
	
	pthread_join(aux_thread, NULL); 
	
	if(pthread_create(&aux_thread,NULL,measure_aux_thread,(void *) *(tops+1))){
		printf("could not create aux thread \n");
	}
	printf("will now begin phase two \n");
	
	__cmd_top(*(tops+1));
	//relaunch top
}


void * measure_aux_thread(void *arg){
	struct perf_top *top=(struct perf_top*) arg;
	int sleep_time=top->numa_sensing_time;
	sleep_time=sleep_time<1 ?  DEFAULT_SENSING_TIME  : sleep_time;
	printf("will sleep for %d seconds\n",sleep_time);
	sleep(sleep_time);
	printf("\n has woken up from thread \n");
	top->numa_metrics->timer_up=true;
	return NULL;
}

/*
 * Entry point to the application can run and analyze the process 
 * specified as parameter or can attach to an already existing process  
 */
 
int init_numa_analysis(int mode, int pid,char **command_args,int command_argc ){
	struct perf_top *tops[2];
	int argc2,i,size;
	char **argv2,*tmp;
	
	pthread_t numatool_thread;
	int pid_uo=0;
	struct numa_metrics* nm,*nm2;
	int x;
	//Numa-migrate stuff is initialized here
	nm=malloc(sizeof(struct numa_metrics));
	memset(nm, 0, sizeof(struct numa_metrics));
	nm->page_accesses=NULL;
	nm->lvl_accesses=NULL;
	
	argc2=command_argc;
	argv2=malloc(argc2*sizeof(char*));
	
	for(int i=0; i<argc2; i++){
		size=sizeof(char)*strlen(*(command_args+i));
		tmp=malloc(size);
		strcpy(tmp,*(command_args+i));
		*(argv2+i)=tmp;
		printf("%s %s %d\n", *(command_args+i),tmp, size);
	}
	nm2=malloc(sizeof(struct numa_metrics));
	memset(nm2, 0, sizeof(struct numa_metrics));
	nm2->page_accesses=NULL;
	nm2->lvl_accesses=NULL;
	
	
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
	tops[1]=cmd_top(argc2,argv2,NULL);
	
	nm->logging_detail_level=tops[0]->numa_migrate_logdetail;
	nm->moved_pages=0;
	tops[0]->numa_metrics=nm;
	nm->pid_uo=mode==RUN_ATTACHED ? tops[0]->numa_migrate_pid_filter : pid;
	nm->file_label=tops[0]->numa_filelabel;
	nm->timer_up=false;
	
	nm2->logging_detail_level=tops[1]->numa_migrate_logdetail;
	nm2->moved_pages=0;
	tops[1]->numa_metrics=nm2;
	nm2->pid_uo=mode==RUN_ATTACHED ? tops[1]->numa_migrate_pid_filter : pid;
	nm2->file_label=tops[1]->numa_filelabel;
	nm2->timer_up=false;
	
	if (!tops[0])
		return NUMATOOL_ERROR;
		
	
	if(pthread_create(&numatool_thread,NULL,run_numa_analysis, tops)){
		return NUMATOOL_ERROR;
	}
	
	//in case of using run_command will launch the process
	
	if(mode==RUN_COMMAND){
		nm->pid_uo=launch_command(command_args, command_argc);
		nm2->pid_uo=nm->pid_uo;
	}
		
	pthread_join(numatool_thread, NULL); 
		return NUMATOOL_SUCCESS;
}

int main(int argc, const char **argv){
	int resp;
	
	page_size = sysconf(_SC_PAGE_SIZE);

	resp=init_numa_analysis(RUN_COMMAND, 0,argv, argc);
	printf("end of measurement %d \n", resp);
}

