#include <stdio.h>
#include <stdlib.h>
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
int minute = 0;
long long total_wait_time = 0;

typedef struct Guest {
	int id;
	struct Guest* next;
	int arrival_time;
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

void addQueue(Queue* q, int guest_id) {
	Guest* new_guest = (Guest*)malloc(sizeof(Guest));

    if (!new_guest) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
	new_guest->id = guest_id;
	new_guest->next = NULL;
	new_guest->arrival_time = minute;

	if(q->back == NULL) {
		q->front = q->back = new_guest;
	} else {
		q->back->next = new_guest;
		q->back = new_guest;
	}
	q->size++;

	if(q->size > q->longest_line) {
		q->longest_line = q->size;
		q->longest_wait_time = minute;
	}
    pthread_cond_broadcast(&cond);
}

int removeQueue(Queue* q) {
	if(q->front == NULL) {
		return -1;
	} else {
		Guest* temp = q->front;
		int guest_id = temp->id;

		int wait_time = minute - temp->arrival_time;
		total_wait_time += wait_time;
		total_ridden++;

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

        if(waiting_line.size == 0) {
            pthread_mutex_unlock(&lock);
            continue;
        }

        //Load guest
        int toLoad = (waiting_line.size < MAXPERCAR) ? waiting_line.size : MAXPERCAR;
        
        for (int i = 0; i < toLoad; i++) {
            int guest_id = removeQueue(&waiting_line);
            if (guest_id == -1) {
                fprintf(stderr, "Queue removal failed unexpectedly.\n");
                break;
            }
        }
    // uncomment for debugging
	// fprintf(output_file, "Car %d: Loaded=%d, Remaining Waiting=%d\n", carID, toLoad, total_waiting); 
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

        // uncomment to debug
        // printf("Current minute in arrival thread:%d\n", minute); 
        int meanArrival;
        if (minute < 120) {               // 0900-1059 (2 hours = 120 minutes)
            meanArrival = 25;
        } else if (minute < 300) {        // 1100 -1359 (2hrs + 3hrs = 5hrs = 300 minutes)
            meanArrival = 45;
        } else if (minute < 420) {        // 1400-1559 (2hrs + 5hrs = 7hrs = 420 minutes)
            meanArrival = 35;
        } else {                          // 1600-1859 
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

	// Open file for writing will be used to LOG INFO
    char filename[50];
    sprintf(filename, "N%dM%d.txt", CARNUM, MAXPERCAR);
	output_file = fopen(filename, "w");
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

	double average_wait_time = 0.0;
	int avg_minutes = 0;
	int avg_seconds = 0;
	if (total_ridden > 0) {
		average_wait_time = (double) total_wait_time / total_ridden;
		avg_minutes = (int)average_wait_time;
		avg_seconds = (average_wait_time - avg_minutes) * 60;
	}

    pthread_mutex_destroy(&lock);
    pthread_cond_destroy(&cond);

    fprintf(output_file, "Simulation Complete!\n");
    fprintf(output_file, "Total Arrived: %d\n", total_arrived);
    fprintf(output_file, "Total Riders: %d\n", total_ridden);
    fprintf(output_file, "Total Rejected: %d\n", total_rejected);
    fprintf(output_file, "Average Wait Time: %.2d minutes and %d seconds\n",avg_minutes, avg_seconds);
    fprintf(output_file, "Worst Case Line: %d at Minute %d\n", waiting_line.longest_line, waiting_line.longest_wait_time);

    return 0;

	// Close file
	fclose(output_file);

    return 0;
}
