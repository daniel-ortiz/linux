
	//the home proc c#include "sort.h"
#include "hist.h"
#include "symbol.h"
#include "numa_metrics.h"
#include <numaif.h>
#include <time.h>
#include <signal.h>
#include <omp.h>
#include <uthash.h>
#include <stdarg.h>

void print_migration_statistics(struct numa_metrics *nm){
	struct page_stats *current,*tmp;
	//TODO number cpus
	int i=0, total_accesses=0, remote_accesses=0;
	float rem2loc =0;
	for(i=0; i<32; i++){
		total_accesses+= nm->process_accesses[i];
		remote_accesses+= nm->remote_accesses[i];
	}
	total_accesses= total_accesses==0 ? 1 : total_accesses;
	rem2loc=(100*(float)remote_accesses/(float)total_accesses); 
	print_info(nm->report,"\t\t MIGRATION STATISTICS \n");
	
	print_info(nm->report," sampled accesses: %d\n remote accesses: %d \n pzn remote %f \n\n\n", total_accesses, remote_accesses,rem2loc);
	print_info(nm->report," moved pages: %d \n",nm->moved_pages);
	
	for(i=0; i<32; i++){
		print_info(nm->report,"CPU: %d	sampled: %d \t remote %d \n",i, nm->process_accesses[i], nm->remote_accesses[i]);
	}
	
	sort_entries(nm);
	HASH_ITER(hh, nm->page_accesses, current, tmp) {
		if(nm->report)
			fprintf(nm->report,"Accessed %p count %d %d \n", current->page_addr, current->proc0_acceses,current->proc1_acceses);
	}
	
	
	
}

int do_migration(struct numa_metrics *nm, int pid, struct perf_sample *sample){
	
	u64 mask,addr;
	int count,*nodes,node,calling_cpu,home_count, remote_count;
	void *page, **pages;
	int* status,st;
	long ret;
	struct page_stats *sear=NULL,*hashtable;
	union perf_mem_data_src data_src; 
	
	st=-1;
	
	data_src.val=sample->data_src;
	
	if (!data_src.val)
		return -1;

		
	//Assuming page size is 4K the 12 LSBs are discharged to 
	//obtain the page number
	//TODO page number must be parametrized
	mask= 0xFFF; // 12 bytes
	addr=sample->addr;
	addr= addr & ~mask ;
	count=1;
	node=0;
	nodes=NULL;
	page= (void *) addr;
	pages=&page;
	status=&st;
	
	hashtable=nm->page_accesses;
	HASH_FIND_PTR( hashtable,&page,sear );
	if(!sear) return 0;
	
	if(nm->logging_detail_level > 3)
		printf("Hash table lookup %p %d %d **\n", sear->page_addr ,sear->proc0_acceses,sear->proc1_acceses );
	
	ret= move_pages(pid, count, pages, nodes, status,0);
	
	//only if the pages query is successful and the home node is different than the node that queries the address the page is migrated
	if (ret != 0){
		if(nm->logging_detail_level > 3)	
		printf ("Error on query\n");
		return 0;
		
	}
	//an only be different to the requesting proc and this home proc must be 0 or 1
	calling_cpu=nm->cpu_to_processor[sample->cpu];
	if ( calling_cpu== st ){
		if(nm->logging_detail_level > 3)	
		printf ("Page is already at requesting node (L3 hit) %d \n",status[0]);
		return 0;
		
	}
	if ( st < 0 || st>1  ){
		if(nm->logging_detail_level > 3)	
		printf ("Problematic status %d \n",status[0]);
		return 0;
		
	}
		
	if(nm->logging_detail_level > 3)	
		printf ("Move candidate:  home proc %d - requesting proc %d (core %d)\n ",
	status[0], nm->cpu_to_processor[sample->cpu],sample->cpu);		
	//the page is only moved if the number of accesses from the calling processor is greater than on the home
	home_count= status[0]==0 ? sear->proc0_acceses : sear->proc1_acceses;
	remote_count= status[0]==0 ? sear->proc1_acceses : sear->proc0_acceses;
	
	if(	remote_count <= home_count) {
		if(nm->logging_detail_level > 3)	
		printf ("Moving criteria not met\n");
		return;
	}
	
	//determine the new home processor
	node = nm->cpu_to_processor[sample->cpu]; 
	nodes=&node;
	
	//we want to move the page to its new home
	ret= 	move_pages(pid, count, pages, nodes, status,0);
	
	if (ret!=0){
		printf("error moving page \n");
		return;
	}
	
	//we will query again for the home of the page
	nodes=NULL;
	ret= move_pages(pid, count, pages, nodes, status,0);
		if(nm->logging_detail_level > 3)
		printf ("Page %p moved new home %d\n ",page,status[0] );	
	if(ret==0 && status[0] >=0)
		nm->moved_pages++;
	return ret;
	
}

//used by numa-an to determine the type of access

int filter_local_accesses(union perf_mem_data_src *entry){
	//This method is based on util/sort.c:hist_entry__lvl_snprintf
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
	
	
	if (entry->val)
		m  = entry->mem_lvl;
	
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



void init_processor_mapping(struct numa_metrics *multiproc_info){
	int map[32]={0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0};
	int i;
	
	for(int i=0; i<32;i++) 
	multiproc_info->cpu_to_processor[i]=map[i];
}

void add_lvl_access( struct numa_metrics *multiproc_info, union perf_mem_data_src *entry, int weight ){
	struct access_stats *current=NULL;
	int key=(int)entry->mem_lvl;
	int weight_bucket=weight / WEIGHT_BUCKET_INTERVAL;
	
	HASH_FIND_INT(multiproc_info->lvl_accesses, &key,current);
	weight_bucket=weight_bucket > (WEIGHT_BUCKETS_NR-1) ? WEIGHT_BUCKETS_NR-1 : weight_bucket;
	
	multiproc_info->access_by_weight[weight_bucket]++;
	if(!current){
		current=malloc(sizeof(struct access_stats));
		current->count=0;
		current->mem_lvl=(int)entry->mem_lvl;
		HASH_ADD_INT(multiproc_info->lvl_accesses,mem_lvl,current);
	}
	
	current->count++;
	
}

void add_mem_access( struct numa_metrics *multiproc_info, void *page_addr, int accessing_cpu){
	struct page_stats *current=NULL; 
	struct page_stats *cursor=NULL; 
	int proc;

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

void print_access_info(struct numa_metrics *multiproc_info ){
	struct access_stats *current=NULL,*tmp;
	FILE *file=multiproc_info->report;
	int i=0,ubound,lbound;
	HASH_ITER(hh, multiproc_info->lvl_accesses, current, tmp) {
		print_info(multiproc_info->report,"LEVEL %d count %d %s \n", current->mem_lvl, current->count,print_access_type(current->mem_lvl) );
	}
	print_info(multiproc_info->report, "\n\n\n Access breakdown by weight \n\n");
	
	for(i=0; i<WEIGHT_BUCKETS_NR; i++){
		lbound=i*WEIGHT_BUCKET_INTERVAL;
		ubound=(i+1)*WEIGHT_BUCKET_INTERVAL;
		print_info(multiproc_info->report,"%d - %d : %d\n",lbound,ubound, multiproc_info->access_by_weight[i]);
	}
	
}

//This method is based on util/sort.c:hist_entry__lvl_snprintf
char* print_access_type(int entry)
{
	char out[64];
	size_t sz = sizeof(out) - 1; /* -1 for null termination */
	size_t i, l = 0;
	u64 m =  PERF_MEM_LVL_NA;
	u64 hit, miss;


	m  = (u64)entry;

	out[0] = '\0';

	hit = m & PERF_MEM_LVL_HIT;
	miss = m & PERF_MEM_LVL_MISS;

	/* already taken care of */
	m &= ~(PERF_MEM_LVL_HIT|PERF_MEM_LVL_MISS);

	for (i = 0; m && i < NUM_MEM_LVL; i++, m >>= 1) {
		if (!(m & 0x1))
			continue;
		if (l) {
			strcat(out, " or ");
			l += 4;
		}
		strncat(out, mem_lvl[i], sz - l);
		l += strlen(mem_lvl[i]);
	}
	if (*out == '\0')
		strcpy(out, "N/A");
	if (hit)
		strncat(out, " hit", sz - l);
	if (miss)
		strncat(out, " miss", sz - l);

	return  out;
}

long id_sort(struct page_stats *a, struct page_stats *b) {
    long rst= (long )a->page_addr - (long)b->page_addr;
    return rst;
}

void sort_entries(struct numa_metrics *nm){
	printf("\n Sorting page access entries \n ...");
	HASH_SORT( nm->page_accesses, id_sort );
}

void init_report_file(struct numa_metrics *nm){
	time_t ctime;
	int label=0;
	FILE *file;
	char fname[15];
	
	time(&ctime);
	label=(int)ctime%10000;
	sprintf(fname,"mig-rpt-%d.txt",label);
	
	file = fopen(fname, "w");
	
	if (file == NULL) {
		fprintf(stderr, "Can't open output file %s!\n", fname);
		return;
	}
	fprintf(stdout,"Will output report to %s \n", fname);
	(*nm).report=file;
	(*nm).report_filename=fname;	

}

void close_report_file(struct numa_metrics *nm){
	if(nm->report){
		fclose(nm->report);
	}
}

//makes sure that the list of arguments ends with a null pointer
char ** put_end_params(char **argv,int argc){
	char ** list_args=malloc(sizeof(char*)*(argc+1));
	int i;
	
	for(i=0; i<argc; i++){
		*(list_args+i)=*(argv+i);
	}
	
	*(list_args+argc)=NULL;
	
	return list_args;
}

void launch_command(struct numa_metrics *nm, char** argv, int argc){
	int pid;
	char ** args;
	if(argc< 1 || !argv[0])
		return;
	args=put_end_params(argv,argc);
	if ((pid = fork()) == 0){
  
           setenv("OMP_NUM_THREADS","2",0);
           setenv("GOMP_CPU_AFFINITY", "7,14",1);
           system("sleep 0.5");
           execv(argv[0],args);
           printf ("\n Child has ended execution \n");
           _exit(2);
           
	   }
    else{
           printf("Command launched with pid %d \n",pid);
           nm->pid_uo=pid;
	}
	
}

void print_info(FILE* file, const char* format, ...){
	va_list a_list;
	char line[500];
	va_start( a_list, format );
		if(file!=NULL)
			vfprintf(file,format,a_list);
		
	va_end(a_list);
	
	va_start( a_list, format );

		vprintf(format,a_list);
		
	va_end(a_list);

}

char* get_command_string(char ** argv, int argc){
	int i=0,length=0;
	char* str;
	
	for(int i=0; i<argc;i++){
		length+=strlen(argv[i]);
	}
	str=malloc(sizeof(char)*length+ 2*argc);
	
	for(int i=0; i<argc;i++){
		strcat(str,argv[i]);
		strcat(str," ");
	}
	strcat(str,"\0");
	return str;
}
