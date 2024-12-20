#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include "random437.h"
#include <time.h>

#define MAXWAITPEOPLE 800
#define TOTALMINUTES 600
#define VIRTUALMINUTE 100   // 1 virtual minute = 100ms

FILE *output_file;
int CARNUM = 0;             // set by user input
int MAXPERCAR = 0;          // set by user inpute
int total_arrived = 0;
int total_rejected = 0;
int total_ridden = 0;
int total_waiting = 0;
int minute = 0;

typedef struct Guest {
	int id;
	struct Guest* next;
} Guest;

typedef struct Queue {
	int size;
	Guest* front;
	Guest* back;
	int longest_line;
	int longest_wait_time;
} Queue;

pthread_mutex_t lock;
pthread_cond_t cond;
Queue waiting_line = {0, NULL, NULL, 0, 0};

void addQueue(Queue* q, int guest_id);
int removeQueue(Queue* q);
void* generateArrivals(void* arg);
void* ride(void* arg);
void calcAvgWaitTime(Queue* q);


void addQueue(Queue* q, int guest_id) {
	Guest* new_guest = (Guest*)malloc(sizeof(Guest));
    if (!new_guest) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
	new_guest->id = guest_id;
	new_guest->next = NULL;

	if(q->back == NULL) {
		q->front = q->back = new_guest;
	} else {
		q->back->next = new_guest;
		q->back = new_guest;
	}
	q->size++;

	if(q->size > q->longest_line) {
		q->longest_line = q->size;
	}
    pthread_cond_broadcast(&cond);
}

int removeQueue(Queue* q) {
	if(q->front == NULL) {
		return -1;
	} else {
		Guest* temp = q->front;
		int guest_id = temp->id;
		struct timeval departure_time;
		q->front = q->front->next;
		if(q->front == NULL) {
			q->back = NULL;
		}
		free(temp);
		q->size--;
		return guest_id;
	}
}

void* ride(void *arg) {

	int carID = *(int*)arg;
    int last_minute = -1;

    while(1) {
        pthread_mutex_lock(&lock);
        // printf("Current minute in Ride Thread:%d\n", minute); // Uncomment to debug
        while (minute == last_minute && minute < TOTALMINUTES) {
            pthread_cond_wait(&cond, &lock);
        }
        if (minute >= TOTALMINUTES) {
            pthread_mutex_unlock(&lock);
            break;
        }

        last_minute = minute;

        if(total_waiting == 0) {
            pthread_mutex_unlock(&lock);
            continue;
        }

        //Load guest
        int toLoad = (total_waiting < MAXPERCAR) ? total_waiting : MAXPERCAR;
        
        for (int i = 0; i < toLoad; i++) {
            int guest_id = removeQueue(&waiting_line);
            if (guest_id == -1) {
                fprintf(stderr, "Queue removal failed unexpectedly.\n");
                break;
            }
        }

        total_waiting -= toLoad;
        total_ridden += toLoad;

		// fprintf(output_file, "Car %d: Loaded=%d, Remaining Waiting=%d\n", carID, toLoad, total_waiting); uncomment for debugging
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}

/*
People arriving to the ride 
*/
void* generateArrivals(void* arg) {
    Queue* q = (Queue*)arg;

    for(minute = 0; minute < TOTALMINUTES; minute++) {
        pthread_mutex_lock(&lock);

        // printf("Current minute in arrival thread:%d\n", minute); // uncomment to debug
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
        total_arrived += arrivals;                  
        int accepted = arrivals; 

        int rejected_on_arrival = 0;
        for (int i = 0; i < arrivals; i++)
        {
		    if(q->size >= MAXWAITPEOPLE) {         
		    	total_rejected++;
                rejected_on_arrival++;
		    } else {
		    	addQueue(q, i + minute*100);
		    	total_waiting++; 
		    }       
        }

        // It will write a status line into an output file:
        // “XXX arrive YYY reject ZZZ wait-line WWW at HH:MM:SS”
        // XXX is the time step ranging from 0 to 599,
        // YYY is the number of persons arrived 
        // ZZZ is the number of persons rejected
        // WWW is number of persons in the waiting line, HH:MM:SS is hours, minutes, and seconds.

        fprintf(output_file, "%03d arrive %d reject %d wait-line %d at %02d:%02d:%02d\n",
         minute, arrivals, rejected_on_arrival, q->size, 9 + minute / 60, minute % 60, 0);

        if (arrivals > rejected_on_arrival) {
            pthread_cond_broadcast(&cond);
        }
        pthread_mutex_unlock(&lock);
        usleep(VIRTUALMINUTE * 1000);
    }

    pthread_mutex_lock(&lock);
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&lock);

    return NULL;
}

void calcAvgWaitTime(Queue* q) {
	if(total_ridden > 0) {
		double average_waiting_time = (double)q->longest_wait_time / total_ridden / 60.0; 
		printf("Average Wait Time Per Person: %.2f minutes\n", average_waiting_time);
	} else {
		printf("Average Wait Time Per Person: No guests loaded\n");
	}
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
	
	// /** THIS IS NOT NEEDED THIS IS FOR MY OWN TESTING */
	// time_t now = time(NULL);
	// struct tm *current_time = localtime(&now);
	// char time_str[100];
	// strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", current_time);
	// fprintf(output_file, "-**************************************************************-\n");
	// fprintf(output_file, "SIMULATION STARTED AT: %s\n\n", time_str);
	// fprintf(output_file, "CARNUM SET TO: %d\n", CARNUM);
	// fprintf(output_file, "MAXPERCAR SET TO: %d\n", MAXPERCAR);
	// fprintf(output_file, "-**************************************************************-\n\n\n");
	// fflush(output_file); 
	// /** THIS IS NOT NEEDED THIS IS FOR MY OWN TESTING */

	pthread_t arrival_t, ride_t[CARNUM];
	pthread_mutex_init(&lock, NULL);
    pthread_cond_init(&cond, NULL);
    pthread_create(&arrival_t, NULL, generateArrivals, &waiting_line);

    int carIDs[CARNUM];
    for (int i = 0; i < CARNUM; i++) {
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
    printf("Worst Case Line: %d at Minute %d\n", waiting_line.longest_line, waiting_line.longest_wait_time);

    return 0;

	// Close file
	fclose(output_file);

    return 0;
}
