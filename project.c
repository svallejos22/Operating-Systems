#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/time.h>
#include "random437.h"

#define MAXWAITPEOPLE 800
#define LOADING 7
#define RIDETIME 53
#define UNLOADING 7
#define CYCLETIME (LOADING + RIDETIME + UNLOADING)

int CARNUM = 4; //default
int MAXPERCAR = 7; //default

typedef struct Guest {
	int id;
	struct Guest* next;
	struct timeval arrival_time;
} Guest;

typedef struct Queue {
	int size;
	Guest* front;
	Guest* back;
	int worst_case_length;
	int worst_case_time;
} Queue;

void addQueue(Queue* q, int guest_id);
int removeQueue(Queue* q);
void* generate_arrivals(void* arg);
void* ride(void* arg);
void calcAvg_waitTime(Queue* q);

int total_guests_loaded = 0;
int total_guests_rejected = 0;
int total_guests_arrived = 0;
volatile int stop_ride = 0;
long long total_wait_time = 0;

pthread_barrier_t barrier;

void* ride(void* arg) {
	int thread_index = *((int*)arg);
	free(arg);

	Queue* q = (Queue*) arg;

	while (!stop_ride) {
		printf("Ride is running thread %d\n", thread_index);
		int loaded_guests = 0;
		for (int i = 0; i <= MAXPERCAR; i++) {
			if(q->size > 0) {
				int guest_id = removeQueue(q);
				if(guest_id != -1) {
					loaded_guests++;
					total_guests_loaded++;
			} 
		}
	}
	
	sleep(LOADING/ 60.0);
	sleep(RIDETIME / 60.0);
	sleep(UNLOADING / 60.0);

	pthread_barrier_wait(&barrier);
	}

	printf("Ride thread %d has completed for the day.\n", thread_index);
	return NULL;
}

void calcAvg_waitTime(Queue* q) {
	if(total_guests_loaded > 0) {
		double average_waiting_time = (double)total_wait_time / total_guests_loaded / 60.0; 
		printf("Average Wait Time Per Person: %.2f minutes\n", average_waiting_time);
	} else {
		printf("Average Wait Time Per Person: No guests loaded\n");
	}
}

void addQueue(Queue* q, int guest_id) {
	Guest* new_guest = (Guest*)malloc(sizeof(Guest));
	new_guest->id = guest_id;
	new_guest->next = NULL;
	gettimeofday(&new_guest->arrival_time, NULL);

	if (q->size >= MAXWAITPEOPLE) {
	       total_guests_rejected++;
       		free(new_guest);	       
	} else {
		if(q->back == NULL) {
			q->front = q->back = new_guest;
		} else {
			q->back->next = new_guest;
			q->back = new_guest;
		}
		q->size++;
		total_guests_arrived++;

		if(q->size > q->worst_case_length){
			q->worst_case_length = q->size;
			q->worst_case_time = total_guests_arrived;
		}
	}
}

int removeQueue(Queue* q) {
	if(q->front == NULL) {
		return -1;
	} else {
		Guest* temp = q->front;
		int guest_id = temp->id;
		struct timeval departure_time;
		gettimeofday(&departure_time, NULL);

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
	FILE* file = fopen("ride_log.txt", "w");

	if(file ==NULL) {
		perror("Failed to open file");
		return NULL;
	}

	for(int minute = 0; minute < 600; minute++) {
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
		int rejected = 0;

	//Add New Guest to Queue
	for (int i = 0; i < numArrivals; i++) {
		if(q->size >= MAXWAITPEOPLE) {
			rejected++;
			total_guests_rejected++;
		} else {
			addQueue(q, i + minute*100);
			total_guests_arrived++;  //////////
		}
	}
	
	fprintf(file, "%03d arrive: %d, Rejected %d, Waiting Line %d at %02d:%02d:%02d\n", minute, numArrivals, rejected, q->size, 9 + (minute / 60), minute % 60, 0);	
	sleep(1); //simulate 1 minute passing (each minute is equal to 1 second)
	}

	fclose(file);
	stop_ride = 1;
	pthread_barrier_wait(&barrier);
	return NULL;
}


int main(int argc, char *argv[]) {
	int option;
	while((option = getopt(argc, argv, "N:M:")) != -1) {
		switch (option) {
			case 'N':
				CARNUM = atoi(optarg);
				break;
			case 'M':
				MAXPERCAR = atoi(optarg);
				break;
			default:
				fprintf(stderr, "Usage: %s [-N CARNUM] [-M MAXPERCAR]\n", argv[0]);
				exit(EXIT_FAILURE);
		}
	}
	
	pthread_barrier_init(&barrier, NULL, CARNUM + 1); 
	pthread_t arrival_t;
	pthread_t ride_t[CARNUM];

	//Queue Initialization
	Queue waitingline;
	waitingline.front = NULL;
	waitingline.back = NULL;
	waitingline.size = 0;
	waitingline.worst_case_length = 0;
	waitingline.worst_case_time = 0;

	//Create Arrival Thread
	if (pthread_create(&arrival_t, NULL, generate_arrivals, (void*)&waitingline) != 0) {
		perror("Failed to add new arrivals to the thread\n");
		return 1;
	}

	//Create Ride Threads

	for(int i = 0; i < CARNUM; i++) {
		int* thread_index = malloc(sizeof(int));
		*thread_index = i + 1;

		if(pthread_create(&ride_t[i], NULL, ride, (void*)thread_index) != 0) {
			perror("Failed to create ride thread\n");
			return 1;
		}	
	}

	if(pthread_join(arrival_t, NULL) != 0) {
		perror("Failed to join ride thread\n");
		return 1;
	}

	stop_ride = 1; //ride stops after arrivals have completed
	
	for(int i = 0; i < CARNUM; i++) {
		if(pthread_join(ride_t[i], NULL) != 0) {
			perror("Failed to join ride thread\n");
			return 1;
		}
	}

	calcAvg_waitTime(&waitingline);

	printf("\n Simulation Results:\n");
	printf("Total Number of Arrivals: %d\n", total_guests_arrived);
	printf("Total Number of Rejected Guests: %d\n", total_guests_rejected);
	printf("Total Number of Loaded Guests: %d\n", total_guests_loaded);
	calcAvg_waitTime(&waitingline);
	printf("Worst Case Line Length: %d at minute %d\n", waitingline.worst_case_length, waitingline.worst_case_time);

	pthread_barrier_destroy(&barrier);
	
	return 0;
}

