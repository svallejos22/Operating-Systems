#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include "random437.h"
#include <time.h> // Include this for time functions

#define MAXWAITPEOPLE 800
#define LOADING 7
#define RIDETIME 53
#define UNLOADING 7
#define CYCLETIME (LOADING + RIDETIME + UNLOADING)
#define STARTHOUR 9
#define TOTALMINUTES 600
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

void addQueue(Queue* q, int guest_id);
int removeQueue(Queue* q);
void* generate_arrivals(void* arg);
void* ride(void* arg);

void* ride(void* arg) {
	Queue* q = (Queue*) arg;
	while(1) {
		int loaded_guests = 0;
		for (int i = 0; i <= MAXPERCAR; i++) {
			if(q->size > 0) {
				int guest_id = removeQueue(q);
				if(guest_id != -1) {
					fprintf(output_file, " Guest %d loaded onto ride.\n", guest_id);
					loaded_guests++;
			} 
		}
	}
	fprintf(output_file, "Total number of Passengers loaded: %d\n", loaded_guests);
	fprintf(output_file, "Ride in Progress...\n");
	sleep(LOADING/ 60.0);
	sleep(RIDETIME / 60.0);
	sleep(UNLOADING / 60.0);
	fprintf(output_file, "Ride Cycle Complete.\n");
	fflush(output_file); // Ensure output is written to file
	}
	return NULL;
}

void addQueue(Queue* q, int guest_id) {
	if (q->size >= MAXWAITPEOPLE) {
		fprintf(output_file, "Queue is Full. Guest ID: %d is rejected.\n", guest_id); 
	} else {
		Guest* new_guest = (Guest*)malloc(sizeof(Guest));
		new_guest->id = guest_id;
		new_guest->next =NULL;
		
		if(q->back == NULL) {
			q->front = q->back = new_guest;
		} else {
			q->back->next = new_guest;
			q->back = new_guest;
		}
		q->size++;
		fprintf(output_file, "Guest ID: %d added to Queue. Guests in Queue: %d\n", guest_id, q->size);
	}
	fflush(output_file); // Ensure output is written to file
}

int removeQueue(Queue* q) {
	if(q->front == NULL) {
		return -1;
	} else {
		Guest* temp = q->front;
		int guest_id = temp->id;
		q->front = q->front->next;

		if(q->front == NULL) {
			q->back = NULL;
		}
		free(temp);
		q->size--;
		return guest_id;
	}
}

void* generate_arrivals(void* arg) {
	Queue* q = (Queue*)arg;

	for(int minute = 0; minute<TOTALMINUTES; minute++) {
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

		int numArrivals = poissonRandom(meanArrival);
	fprintf(output_file, "Number of arrival: %d\n", numArrivals);
	//Add New Guest to Queue
	for (int i = 0; i < numArrivals; i++) {
		addQueue(q, i + minute*100);
	}	
	usleep(10000); // 10ms = 10,000 microseconds
	}
	return NULL;
}

int main(int argc, char *argv[]) {
	pthread_t arrival_t;

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

	
	/** THIS IS NOT NEEDED THIS IS FOR MY OWN TESTING LOL */
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
	/**********************************/

	pthread_t ride_t[CARNUM];

	//Queue Initialization
	Queue waitingline;
	waitingline.front = NULL;
	waitingline.back = NULL;
	waitingline.size = 0;

	//Create Arrival Thread
	if (pthread_create(&arrival_t, NULL, generate_arrivals, (void*)&waitingline) != 0) {
		perror("Failed to add new arrivals to the thread\n");
		return 1;
	}

	//Create Ride Threads
	for(int i = 0; i < CARNUM; i++) {
		if(pthread_create(&ride_t[i], NULL, ride, (void*)&waitingline) != 0) {
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

	// Close file
	fclose(output_file);

    return 0;
}
