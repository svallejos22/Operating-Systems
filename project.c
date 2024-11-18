#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include "random437.h"

#define CARNUM 4
#define MAXPERCAR 7
#define MAXWAITPEOPLE 800
#define LOADING 7
#define RIDETIME 53
#define UNLOADING 7
#define CYCLETIME (LOADING + RIDETIME + UNLOADING)

typedef struct Guest {
	int id;
	struct Guest* next;
} Person;

typedef struct Queue {
	int size;
	Guest* front;
	Guest* back;
} Queue;

void* ride(void* arg) {
	Queue* q = (Queue*) arg;
	while(1) {
		int loaded_guests = 0;
		for (int i = 0; i <= MAXPERCAR; i++) {
			if(q->size > 0) {
				int guest_id = removeQueue(q);
				if(guest_id != -1) {
					printf(" Guest %d loaded onto ride.\n", guest_id);
					loaded_guests++;
			} 
		}
	}
	printf("Total number of Passengers loaded: %d\n", loaded_guests);
	printf("Ride in Progress...\n");
	sleep(LOADING_TIME/ 60.0);
	sleep(RIDE_TIME / 60.0);
	sleep(UNLOADING_TIME / 60.0);
	printf("Ride Cycle Complete.\n")
	}
	return NULL;
}

void addQueue(Queue* q, int guest_id) {
	if (q->size >= MAXWAITPEOPLE) {
		printf("Queue is Full.Guest ID: %d is rejected.\n", 
	} else {
		Guest* new_guest = (Guest*)malloc(sizeof(Guest))
		new_guest->id = guest_id;
		new_guest->next =NULL;
		
		if(q->back == NULL) {
			q->front = q->back = new_guest;
		} else {
			q->back->next = new_guest;
			q->back = new_guest;
		}
		q->size++;
		printf("Guest ID: %d added to Queue. Guests in Queue: %d\n", guest_id, q->size);
	}

}

void removeQueue(Queue* q) {
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

	for(int minute = 0; minute , 600; minute++) {
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
	//Add New Guest to Queue
	for (int i = 0; i < numArrivals; i++) {
		addQueue(q, i + minute*100);
	}	
	sleep(1); //simulate 1 minute passing (each minute is equal to 1 second)
	}
	return NULL;
}


int main(int argc, char* arg[]) {
	pthread_t arrival_t;
	pthread_t ride_t[CARNUM];

	//Queue Initialization
	Queue waitingline;
	waitingline.front = NULL;
	waitingline.back = NULL;
	waitingline.size = 0;

	//Create Arrival Thread
	if (pthread_create(%arrival_t, NULL, generate_arrivals, (void*)&waitingline) != 0) {
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
return 0;
}
