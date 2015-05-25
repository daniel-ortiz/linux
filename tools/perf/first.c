#include <sched.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
//#include <linux/types.h>


int main(int argc, char* argv[])
 {
	char* aff=getenv("OMP_NUM_THREADS");
	char str[2];
	int N=100000000;
        int M=1000000000;
	long *endm;

	double* A;
	A  = (double*)malloc(M *sizeof(double) );
	endm=A+(long)(M*sizeof(double));
	srand ( time(NULL) );
	printf("OMP: Here's my PID: %d, memory start %p, end %p \n", getpid(),A,endm);
	#pragma omp master
{	
	int cpu, node;
	cpu= sched_getcpu();
	
//	#pragma omp for
		for (int i = 0; i < M; i++) {
		  A[i] = 3.1416;
		}
	printf ("end of initialization  %d %s %d  \n",omp_get_num_threads(), aff, cpu );
	}	
	//scanf ("%79s",str);  
	printf ("all set\n");
  
	#pragma omp parallel 
   {
      if (omp_get_thread_num( )!=0){
        int cpu= sched_getcpu();
        printf ("modifying mem %d %s %d  \n",omp_get_num_threads(), aff, cpu );
        #pragma omp parallel for 
        for (int j=0; j< 1; j++){for (int i = 0; i < N; i++) {
         	 int index= rand() % M ;
          	  A[index] += 1;
          //printf(" %d ",index );
        }}
      }
   }
   return 0;
 }




