#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>

/* Simple program that creates 2 threads */
/* This will output two lines with the same PID since processes can contain multiple threads,you can have  multiple threads inside a single process*/
/*You cannot have multiple processes inside a single thread */
/*All threads can access all variables, all have a common set of address spaces */

void* routine() {
	printf("Process ID: %d\n", getpid());
	sleep(3);			//Test if the threads are running in parallel. If they are will get:
	printf("Ending thread\n");	//Test from Thread Test from Thread Ending thread Ending thread
}

int main(int argc, char* argv[]) {
	pthread_t t1, t2; //Place where the API can store information about the thread which is a variable
	if(pthread_create(&t1, NULL, &routine, NULL) != 0) { //Initialize the thread
		return 1; //return an error code if it did not create the thread correctly
	}
	if(pthread_create(&t2, NULL, &routine, NULL) != 0) { //Initialize the thread t2
		return 2; //return an error code if it did not create the thread correctly
	}

	if(pthread_join(t1, NULL) != 0) { //wait for the thread to finish
		return 3; //return an error code if it did not work
	}
	if(pthread_join(t2, NULL) != 0) {  //Wait for the thread to finish
		return 4; //return an error if it did not work
	}
	return 0;
}
