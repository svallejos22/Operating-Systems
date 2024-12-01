#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include "random437.h"
#include <time.h> // Include this for time functions

#define MAXWAITPEOPLE 800
#define TOTALMINUTES 600
#define VIRTUALMINUTE 100 // 1 virtual minute = 100ms

int CARNUM = 0;
int MAXPERCAR = 0;

typedef struct Guest {
	int id;
	struct Guest* next;
} Guest;

typedef struct Queue {
	int size;
	Guest* front;
	Guest* back;
} Queue;

FILE *output_file;
int total_arrived = 0, total_rejected = 0, total_ridden = 0, total_waiting = 0, longest_line = 0, longest_wait_time = 0;

pthread_mutex_t lock;
pthread_cond_t cond;

void* generate_arrivals(void* arg);
void* ride(void* arg);

void* ride(void *arg) {
	printf("RIDE\n");

	int carID = *(int*)arg;

    for (int minute = 0; minute < TOTALMINUTES; minute++) {
        pthread_mutex_lock(&lock);

        while (total_waiting == 0) {
            pthread_cond_wait(&cond, &lock);
        }

        int toLoad = (total_waiting < MAXPERCAR) ? total_waiting : MAXPERCAR;
        total_waiting -= toLoad;
        total_ridden += toLoad;

		fprintf(output_file, "Car %d: Loaded=%d, Remaining Waiting=%d\n", carID, toLoad, total_waiting);

        pthread_mutex_unlock(&lock);
		usleep(VIRTUALMINUTE * 1000); //ride duration(load time + ride time)
    }
    return NULL;
}

/*
People arriving to the ride 
*/
void* generate_arrivals(void* arg) {
    for(int minute = 0; minute < TOTALMINUTES; minute++) {
        pthread_mutex_lock(&lock);
        printf("Current minute:%d\n", minute);
        int meanArrival;
        if (minute < 120) {
            meanArrival = 25;
        } else if (minute < 240) {
            meanArrival = 35;
        } else if (minute < 360) {
            meanArrival = 45;
        } else if (minute < 480) {
            meanArrival = 35;
        } else {
            meanArrival = 25;
        }

        int arrivals = poissonRandom(meanArrival);
        int accepted = arrivals; 
        total_arrived += arrivals;

        if(total_waiting + arrivals > MAXWAITPEOPLE) {
            accepted = MAXWAITPEOPLE - total_waiting;
            total_rejected += arrivals - accepted;
        }
        total_waiting += accepted;

        if(total_waiting > longest_line) {
            longest_line  = total_waiting;    //Update the longest line
            longest_wait_time = minute;       //Update the longest wait time
        }
        fprintf(output_file, "Minute %d: Arrived=%d, Rejected=%d, Waiting=%d\n", 
                minute, arrivals, (arrivals-accepted), total_waiting);
        if (total_waiting > 0) {
            pthread_cond_broadcast(&cond);
        }
        pthread_mutex_unlock(&lock);
        usleep(VIRTUALMINUTE * 1000);
    }

    pthread_mutex_lock(&lock);
    pthread_cond_broadcast(&cond); // Final signal I GOT THIS FROM CHATGPT
    pthread_mutex_unlock(&lock);

    return NULL;
}

int main(int argc, char *argv[]) {

    int opt;
    while ((opt = getopt(argc, argv, "N:M:")) != -1) { 
        switch (opt) {
            case 'N':
                CARNUM = atoi(optarg);
                break;
            case 'M': 
                MAXPERCAR = atoi(optarg);
                break;
            default:
                fprintf(stderr, "Usage: %s -N <CARNUM> -M <MAXPERCAR>\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

	// Open file for writing WILL BE USE TO LOG INFO
	output_file = fopen("simulation.txt", "w");
	if (!output_file) {
		perror("Failed to open file for writing");
		exit(EXIT_FAILURE);
	}

	
	/** THIS IS NOT NEEDED THIS IS FOR MY OWN TESTING */
	time_t now = time(NULL);
	struct tm *current_time = localtime(&now);
	char time_str[100];
	strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", current_time);
	fprintf(output_file, "-**************************************************************-\n");
	fprintf(output_file, "SIMULATION STARTED AT: %s\n\n", time_str);
	fprintf(output_file, "CARNUM SET TO: %d\n", CARNUM);
	fprintf(output_file, "MAXPERCAR SET TO: %d\n", MAXPERCAR);
	fprintf(output_file, "-**************************************************************-\n\n\n");
	fflush(output_file); 
	/** THIS IS NOT NEEDED THIS IS FOR MY OWN TESTING */

	pthread_t arrival_t, ride_t[CARNUM];
	pthread_mutex_init(&lock, NULL);
    pthread_cond_init(&cond, NULL);

    pthread_create(&arrival_t, NULL, generate_arrivals, NULL);

    int carIDs[CARNUM];
    for (int i = 0; i < CARNUM; i++) {
		printf("Creating ride threads: %d\n", i); // TODO REMOVE THIS
        carIDs[i] = i + 1;
		if(pthread_create(&ride_t[i], NULL, ride, &carIDs[i]) != 0) {
			perror("Failed to create ride thread\n");
			return 1;
		}	

    }

	if(pthread_join(arrival_t, NULL) != 0) {
		perror("Failed to join ride thread\n");
		return 1;
	}
	for(int i = 0; i < CARNUM; i++) {
		if(pthread_join(ride_t[i], NULL) != 0) {
			perror("Failed to join ride thread\n");
			return 1;
		}
	}

    pthread_mutex_destroy(&lock);
    pthread_cond_destroy(&cond);

    printf("Simulation Complete!\n");
    printf("Total Arrived: %d\n", total_arrived);
    printf("Total Riders: %d\n", total_ridden);
    printf("Total Rejected: %d\n", total_rejected);
    printf("Average Wait Time: TBD\n");
    printf("Worst Case Line: %d at Minute %d\n",longest_line, longest_wait_time);

    return 0;

	// Close file
	fclose(output_file);

    return 0;
}
