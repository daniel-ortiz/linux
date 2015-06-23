#include "sort.h"
#include "hist.h"
#include "symbol.h"
#include "numa_metrics.h"
#include <numaif.h>
#include <time.h>
#include <signal.h>
#include <omp.h>
#include <uthash.h>

int get_access_type(struct hists *hists, struct hist_entry *entry,int pid){
	
	u64 addr,mask;
	int count,*nodes,node,calling_cpu,home_count, remote_count;
	void *page, **pages;
	int* status,st;
	long ret;
	struct page_stats *sear,*hashtable;
	
	st=-1;
	
	if (!entry->mem_info)
		return -1;

	addr= entry->mem_info->daddr.addr;
		
	//Assuming page size is 4K the 12 LSBs are discharged to 
	//obtain the page number
	mask= 0xFFF; // 12 bytes
	addr= addr & ~mask ;
	count=1;
	node=0;
	nodes=NULL;
	page= (void *) addr;
	pages=&page;
	status=&st;
	
	hashtable=hists->multiproc_traffic->page_accesses;
	HASH_FIND_PTR( hashtable,&page,sear );
	if(!sear) return;
	
	printf("%p %d %d **", sear->page_addr ,sear->proc0_acceses,sear->proc1_acceses );
	
	ret= move_pages(pid, count, pages, nodes, status,0);
	printf ("MP success %d home proc %d requesting cpu %d requesting proc %d addr %p \n ",ret, status[0],
	entry->cpu,hists->multiproc_traffic->cpu_to_processor[entry->cpu],entry->mem_info->daddr.addr );	
	//only if the pages query is successful and the home node is different than the node that queries the address the page is migrated
	if (ret != 0)
		return;
	//the home proc can only be different to the requesting proc and this home proc must be 0 or 1
	calling_cpu=hists->multiproc_traffic->cpu_to_processor[entry->cpu];
	if ( calling_cpu== st || st < 0 || st>1  )
		return;
		
	//the page is only moved if the number of accesses from the calling processor is greater than on the home
	home_count= status[0]==0 ? sear->proc0_acceses : sear->proc1_acceses;
	remote_count= status[0]==0 ? sear->proc1_acceses : sear->proc0_acceses;
	
	if(	remote_count <= home_count) return;
	
	//determine the new home processor
	node = hists->multiproc_traffic->cpu_to_processor[entry->cpu]; 
	nodes=&node;
	
	//we want to move the page to its new home
	ret= 	move_pages(pid, count, pages, nodes, status,0);
			printf("MPG: %d \n",node);
	if (ret!=0){
		printf("error moving page \n");
		return;
	}
	
	//we will query again for the home of the page
	nodes=NULL;
	ret= move_pages(pid, count, pages, nodes, status,0);
	printf ("MPR success %d home proc %d requesting cpu %d requesting proc %d addr %p \n ",ret, status[0],
	entry->cpu,hists->multiproc_traffic->cpu_to_processor[entry->cpu],entry->mem_info->daddr.addr );	
	return ret;
}

int filter_local_accesses(struct hist_entry *entry){
	//This method is based on util/sort.c:hist_entry__lvl_snprintf
	int filter=0;	
	u64 m =  PERF_MEM_LVL_NA;
	u64 hit, miss;
	//masks defined according to the declaration of mem_lvl
	u64 l1_mask= 0x1 << 3;
	u64 lfb_mask= 0x1 << 4;
	u64 l2_mask= 0x1 << 5;
	u64 l3_mask= 0x1 << 6;
	int init_remote=8;
	int end_remote=11;
	int i=0;
	
	
	if (entry->mem_info)
		m  = entry->mem_info->data_src.mem_lvl;
	
	hit = m & PERF_MEM_LVL_HIT;
	miss = m & PERF_MEM_LVL_MISS;	
	m &= ~(PERF_MEM_LVL_HIT|PERF_MEM_LVL_MISS); // bits 1 and 2 are cleared
	
	//L1, L2 and LFB accesses are discarded
	if( (l1_mask & m) || (l2_mask & m) || (lfb_mask & m)){
		return 1;
	}
	// L3 hits are filtered
	if( (l3_mask & m) && hit){
		return 1;
	}
	
	//l3 misses are filtered
	if( (l3_mask & m) && miss){
		return 0;
	}
	
	//Remote accesses are not filtered
	for (i=init_remote; i<=end_remote;  i++){
		if( (1<<i) & m )
			return 0;
	}
	//any other accesses are filtered but passed as different information
	return 2;
}

int main_numaan(int argc, const char **argv){
	
	int ret,pid,label,kill_rslt,iter,end_read,samples_taken;
	int labels[10]={0,0,0,0,0,0,0,0,0,0};
	const char **strs;
	time_t ctime; 
	omp_lock_t label_lock;
	char labelstr[20];
	int exit=0;
	omp_init_lock(&label_lock);
	end_read=0;
	samples_taken=0;
	
	if (argc <3){
			printf("not enough arguments");
	}
	
	#pragma omp parallel
    #pragma omp single nowait
    {
        #pragma omp task shared (label_lock,labels) private (iter,label)
        while (!exit){
		//record
		//used to find out whether the process is alive or not)
		pid=atoi(argv[2]);
		kill_rslt=kill(pid,0);
		if(kill_rslt==-1 &&  errno== ESRCH){
				printf("process pid %d not detected \n",pid);
				omp_set_lock(&label_lock);
				iter=0;
				while(labels[iter]!=0 && iter <10) iter++;
				labels[iter]=-1;	
	
				omp_unset_lock(&label_lock);
				break;
		}
		
		time(&ctime);
		label=(int)ctime%10000;
		launch_record(argc,argv,label); 
		omp_set_lock(&label_lock);
			iter=0;
			while(labels[iter]!=0 && iter <10) iter++;
			labels [iter]=label;
		omp_unset_lock(&label_lock);
		//exit=1;
		//this is a protection for when it fails
		if (samples_taken++ > 10){
			exit=1;
			iter=0;
			while(labels[iter]!=0 && iter <10) iter++;
			labels [iter]=-1;
		}
	
	}
        #pragma omp task shared (label_lock,labels) private (iter,label)
		
		while(!end_read){
			label=0;
			omp_set_lock(&label_lock);
				if (labels[0] != 0){
					label=labels[0];
					for(iter=0;iter<9 && labels[iter]!=0; iter++){
						labels[iter]=labels[iter+1];	
					}
					printf("retrieve %d \n", label);
				}
			omp_unset_lock(&label_lock);
			if (label==-1){
					end_read=1;
			}
			if(label>0){
					sprintf(labelstr,"numaan%d.data",label);
					launch_report(argc,argv,labelstr);
			}
		}
        #pragma omp taskwait
        printf("c");
    }
    
	
    //printf("Hello World %d ",(int) ctime % 4000);
	//if (argc >3 && !strcmp(*(argv+2),"run") ){
		//launch_record(argc,argv,(int)ctime%10000);
		//*(argv+2)=*(argv+3);

		//launch_report(argc,argv);
	//}
	//else{
		//launch_report(argc,argv);
	//}
}

void launch_record(int argc, const char **argv, int label){		
		const char **strs = calloc(12, sizeof(char *));
		char command[100];
		
		sprintf(command, "./perf mem record -W -d -e cpu/mem-loads/pp --cpu 0-31 -o numaan%d.data sleep 1.5",label);

		//Call string for record record -W -d -e cpu/mem-loads/pp --cpu 0-31 sleep tsleep
		strs[0] = strdup("record");
		strs[1] = strdup("-W");
		strs[2] = strdup("-d");
		strs[3] = strdup("-e");
		strs[4] = strdup("cpu/mem-loads/pp");
		strs[5] = strdup("--cpu");
		strs[6] = strdup("0-31");
		strs[7] = strdup("sleep");
		strs[8] = strdup("1,5");
		strs[9] = strdup(" ");
		printf("before launching record %d %s \n",argc,argv[0]);
		//ret = cmd_record(9, strs, NULL);
		
		printf("command issued: %s \n",command);
		system(command);
		printf("end of sampling stage %d %s \n",argc,argv[0]);

}

void launch_report(int argc, const char **argv, char* file){
		int pid,ret;
		const char **strs = calloc(6, sizeof(char *));
		
		if(argc < 3)
			return;
		
		pid=atoi(*(argv+2));
		
		if(pid==0){
			printf("pid error \n");
			return;
		}
		
		//Call string for record record -W -d -e cpu/mem-loads/pp --cpu 0-31 sleep tsleep
		strs = calloc(9, sizeof(char *));
		strs[0] = strdup("numaan");
		strs[1] = strdup(*(argv+2));
		strs[2] = strdup("");
		strs[3] = strdup("--mem-mode");
		strs[4] = strdup("-n");
		strs[5] = strdup("-i");
		strs[6] = strdup(file);
		strs[7] = strdup("--dso");
		strs[8] = strdup("touch");
		
		printf("before launching report %d %s \n",argc,argv[0]);
		ret = cmd_report(7, strs, NULL);
		ret++;
}

void init_processor_mapping(struct numa_metrics *multiproc_info){
	int map[32]={0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0};
	int i;
	
	for(int i=0; i<32;i++) 
	multiproc_info->cpu_to_processor[i]=map[i];
}


void add_mem_access( struct numa_metrics *multiproc_info, void *page_addr, int accessing_cpu){
	struct page_stats *current=NULL; 
	struct page_stats *cursor=NULL; 
	int proc;
	int hm;
	int ad;
	
	//search the node for apparances

	HASH_FIND_PTR(multiproc_info->page_accesses, &page_addr,current);
	proc=multiproc_info->cpu_to_processor[accessing_cpu];
	if (current==NULL){
		current=malloc(sizeof(struct page_stats));
		current->proc0_acceses=0;
		current->proc1_acceses=0;
		
		current->page_addr=page_addr;
		
		HASH_ADD_PTR(multiproc_info->page_accesses,page_addr,current);
		//printf ("add %d \n",HASH_COUNT(multiproc_info->page_accesses));
		
		for(cursor=multiproc_info->page_accesses; cursor != NULL; cursor=cursor->hh.next) {
        //printf("%p\n", cursor->page_addr);
		}
	}
	
	if (proc==0){
		current->proc0_acceses++;
	}else if(proc==1){
		current->proc1_acceses++;
	}
	
}



