#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include "/home/tj0048/tmp/valgrind_src/gleipnir/gleipnir.h"

void *mythread(void* arg)
{
	int tid = (long) arg;

	printf("hello I'm thread %d\n", tid);

	pthread_exit(NULL);
}

int main(void)
{
	GL_START_INSTR;
	pthread_t tid[4];

	long i = 0;
	int rc;

	for(i=0; i<4; i++){
		rc = pthread_create(&tid[i], NULL, mythread, (void *) i);
	}

	GL_STOP_INSTR;

	pthread_exit(NULL);
}
