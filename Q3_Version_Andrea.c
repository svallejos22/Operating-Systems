#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ipc.h> 
#include <sys/shm.h> 

typedef struct {
	int balance[2];
} Bank_t;

void MakeTransactions(Bank_t *Bank) {
	int i, j, tmp1, tmp2, rint;
	double dummy;
	for (i=0; i<100; i++) {
        rint = (rand()%30)-15; 

		if(((tmp1 = (*Bank).balance[0]) + rint) >= 0 && ((tmp2 = (*Bank).balance[1]) - rint) >= 0) {
			(*Bank).balance[0] = tmp1 + rint;       
			for(j=0; j < rint*1000; j++) {
				dummy = 2.345*8.765 / 1.234;
			}
			(*Bank).balance[1] = tmp2 - rint;       
		}
	}    
}

int main (int argc, char **argv) {
	int fd;
	pid_t pid;
	Bank_t *Bank;
	key_t key = ftok("bankacct", 'A');

	if((key == -1)) {
		perror("ftok");
		exit(1);
	}

	srand(getpid());  //Initialize random num generator with PID
	/* Create the first memory segement */
	if((fd = shmget(key, sizeof(*Bank), IPC_CREAT | 0666)) < 0) {
		perror("shmget");
		exit(1);
	}
	// Attached the memory segments and check if successful
	Bank = shmat(fd, NULL, 0);
	if(Bank == (Bank_t *) -1) {
		perror("shmat");
		exit(1);
	}

	Bank ->balance[0] = 100;
	Bank ->balance[1] = 100;

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

	shmdt(Bank);
	shmctl(fd, IPC_RMID, NULL);

	return 0;
}
