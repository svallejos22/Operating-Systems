#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <pthread.h>

typedef struct {
	int balance[2];
	pthread_mutex_t lock; 
} Bank_t;

void MakeTransactions(Bank_t *Bank) {
	int i, j, tmp1, tmp2, rint;
	double dummy;

	for (i=0; i<100; i++) {
		rint = (rand()%30)-15;  //Generate a random number between -15 and 15

		pthread_mutex_lock(&(Bank->lock)); //Lock CS to prevent race condition

		if(((tmp1 = (*Bank).balance[0]) + rint) >= 0 && ((tmp2 = (*Bank).balance[1]) - rint) >= 0) {
			(*Bank).balance[0] = tmp1 + rint; //Update account A
			
			for(j=0; j < rint*1000; j++) {
				dummy = 2.345*8.765 / 1.234;
			}
			(*Bank).balance[1] = tmp2 - rint;  //Update account B
				}
		pthread_mutex_unlock(&(Bank->lock));  //Unlock the CS
	}
}

int main (int argc, char **argv) {
	int fd;
	pid_t pid;
	Bank_t *Bank;

	srand(getpid());  //Initialize random num generator with PID

	/*I had some issues with the shared memory section and used ChatGPT for some of the formatting such as O_CREAT, O_RDWR, and mmap*/	
	fd = shm_open("/bankacct", O_CREAT | O_RDWR, 0666);
	if (fd == -1) {
		perror("shm_open failed");
		exit(1);
	}
	
	if (ftruncate(fd, sizeof(Bank_t)) == -1) {
		perror("ftruncate failed");
		exit(1);
	}
	
	Bank = mmap(NULL, sizeof(Bank_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	
	if(Bank == MAP_FAILED) {
		perror("mmap failed");
		exit(1);
	}

	/*Initialized Bank Balances and Mutex */
	/* ChatGPT was used to understand how mutex works */
	Bank ->balance[0] = 100;
	Bank ->balance[1] = 100;
	pthread_mutexattr_t bankattr;
	pthread_mutexattr_init(&bankattr);
	pthread_mutexattr_setpshared(&bankattr, PTHREAD_PROCESS_SHARED);
	pthread_mutex_init(&(Bank->lock), &bankattr);

	printf("Init balances A: %d + B: %d ==> %d!\n", (*Bank).balance[0], (*Bank).balance[1], (*Bank).balance[0] + (*Bank).balance[1]);

	pid = fork();
	if(pid < 0) {
		perror("Fork failed");
		exit(1);
	}
	else if(pid == 0) {
		MakeTransactions(Bank);
		exit(0);
	}
	else {
		MakeTransactions(Bank);
		wait(NULL);  //Wait for child to finish
	}

	printf("Let's check the balances A: %d + B: %d ==> %d ?= 200\n", (*Bank).balance[0], (*Bank).balance[1], (*Bank).balance[0] + (*Bank).balance[1]);

	//Cleanup
	pthread_mutex_destroy(&(Bank->lock));

	if(shm_unlink("/bankacct") == -1) {
		perror("shm_unlink failed");
		exit(1);
	}
	return 0;
}
