#include <stdio.h>
#include "csapp.h"


void Pthread_create(pthread_t *tidp, pthread_attr_t *attrp, 
		    void * (*routine)(void *), void *argp);
void *thread(void *vargp);
void Pthread_join(pthread_t tid, void **thread_return);


int main(void){

    pthread_t tid[4];
    int i;

    for ( i = 0; i < 5; i++)
    {
        Pthread_create(&tid[i],NULL,thread,&i);
    }
    for ( i = 0; i < 5; i++)
    {
        Pthread_join(tid[i],NULL);
    }
    



    return 0;
}
void *thread(void *vargp){
    int myid = *((int *)vargp);
    printf("Hello from thread %d\n", myid);
    return NULL;
}



void Pthread_join(pthread_t tid, void **thread_return) {
    int rc;

    if ((rc = pthread_join(tid, thread_return)) != 0)
	posix_error(rc, "Pthread_join error");
}

void Pthread_create(pthread_t *tidp, pthread_attr_t *attrp, 
		    void * (*routine)(void *), void *argp) 
{
    int rc;

    if ((rc = pthread_create(tidp, attrp, routine, argp)) != 0)
	posix_error(rc, "Pthread_create error");
}