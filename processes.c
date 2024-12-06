#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>

/* This program will print out two different PID's, unlike the thread program */
/* When forking, we are making a copy of the parent process so we get two different PIDs and they can be */
/* modified individually.*/

int main(int argc, char* argv[]) {
	int pid = fork();
	if(pid == -1) {
		return 1; //return an error
	}
	printf("Process ID: %d\n", getpid());
	if(pid != 0) {
		wait(NULL);
	}

return 0;
}
