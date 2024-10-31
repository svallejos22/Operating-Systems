#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>

typedef struct {
	int balance[2]; 
} Bank_t;

Bank_t *Bank;
sem_t *sem;

void MakeTransactions(Bank_t *Bank) {
	int i, j, tmp1, tmp2, rint;
	double dummy;

	for (i=0; i<100; i++) {
		rint = (rand()%30)-15;  //Generate a random number between -15 and 15

		sem_wait(sem); //sem lock

		if(((tmp1 = (*Bank).balance[0]) + rint) >= 0 && ((tmp2 = (*Bank).balance[1]) - rint) >= 0) {
			(*Bank).balance[0] = tmp1 + rint; //Update account A
			
			for(j=0; j < rint*1000; j++) {
				dummy = 2.345*8.765 / 1.234;
			}
			(*Bank).balance[1] = tmp2 - rint;  //Update account B
				}
		sem_post(sem); //Release sem lock
	}
}

int main (int argc, char **argv) {
	int fd;
	pid_t pid;

	srand(getpid());  //Initialize random num generator with PID
	
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

	/*Initialized Bank Balances */
	Bank ->balance[0] = 100;
	Bank ->balance[1] = 100;
	
	/*Semaphore for Synchronization*/
	sem = sem_open("/sem_bank", O_CREAT, 0666,1);
	if(sem == SEM_FAILED) {
		perror("Sem Failed");
		exit(1);
	}

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
	munmap(Bank, sizeof(Bank_t));
	shm_unlink("/bankacct");
	sem_close(sem);
	sem_unlink("/sem_bank");

	return 0;
}
