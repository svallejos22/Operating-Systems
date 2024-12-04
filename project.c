#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include "random437.h"
#include <time.h>

#define MAXWAITPEOPLE 800
#define TOTALMINUTES 600
#define VIRTUALMINUTE 100 // 1 virtual minute = 100ms

FILE *output_file;
int CARNUM = 0;            
int MAXPERCAR = 0;         
int total_arrived = 0;
int total_rejected = 0;
int total_ridden = 0;
int total_waiting = 0;


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

Queue waiting_line = {0, NULL, NULL, 0, 0};


int global_minute = 0;
int current_car_turn = 0; // Ensures that the cars run in order rather than at random
pthread_mutex_t global_lock = PTHREAD_MUTEX_INITIALIZER; 
pthread_cond_t global_cond = PTHREAD_COND_INITIALIZER;  
pthread_cond_t car_cond = PTHREAD_COND_INITIALIZER;   
pthread_mutex_t queue_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t arrival_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t arrival_cond = PTHREAD_COND_INITIALIZER;
int arrival_done[TOTALMINUTES] = {0};


void* timekeeper(void* arg);
void* generate_arrivals(void* arg);
void* ride(void* arg);
void addQueue(Queue* q, int guest_id);
int removeQueue(Queue* q);

void* timekeeper(void* arg) {
    for (int minute = 0; minute < TOTALMINUTES; minute++) {
        usleep(VIRTUALMINUTE * 1000); 

        pthread_mutex_lock(&global_lock);
        global_minute++;
        pthread_cond_broadcast(&global_cond);
        pthread_mutex_unlock(&global_lock);
    }
    return NULL;
}

void* generate_arrivals(void* arg) {
    Queue* q = (Queue*)arg;

    for (int minute = 0; minute < TOTALMINUTES; minute++) {
        pthread_mutex_lock(&global_lock); //Ensures that the arrivals are synchronized with the global clock
        while (global_minute != minute + 1) {
            pthread_cond_wait(&global_cond, &global_lock);
        }
        pthread_mutex_unlock(&global_lock);

        // ARRIVALS
        pthread_mutex_lock(&queue_lock);
        int meanArrival;
	if (minute < 120) {
		meanArrival = 25;
	} else if (minute < 240) {
		meanArrival = 45;
	} else if (minute < 360) {
		meanArrival = 35;
	} else if (minute < 540) {
		meanArrival = 35;
	} else {
		meanArrival = 25;
	}

        int arrivals = poissonRandom(meanArrival);

        total_arrived += arrivals;
        int rejected_on_arrival = 0;

        for (int i = 0; i < arrivals; i++) {
            if (q->size >= MAXWAITPEOPLE) {
                total_rejected++;
                rejected_on_arrival++;
            } else {
                addQueue(q, i + minute * 100);
                total_waiting++;
            }
        }

        fprintf(output_file, "%03d arrive %d reject %d wait-line %d at %02d:%02d:00\n",
                minute, arrivals, rejected_on_arrival, q->size, 9 + minute / 60, minute % 60);
        fflush(output_file); // Without this line, the thread logs were sometimes written out of order
        pthread_mutex_unlock(&queue_lock);

        // Arrivals completed for the current minute
        pthread_mutex_lock(&arrival_mutex);
        arrival_done[minute] = 1;
        pthread_cond_broadcast(&arrival_cond);
        pthread_mutex_unlock(&arrival_mutex);
    }
    return NULL;
}

void* ride(void* arg) {
    int carID = *(int*)arg;

    for (int minute = 0; minute < TOTALMINUTES; minute++) {
        pthread_mutex_lock(&global_lock); // Car threads waits until the simulation reaches the next minute
        while (global_minute != minute + 1) {
            pthread_cond_wait(&global_cond, &global_lock);
        }
        pthread_mutex_unlock(&global_lock);

        pthread_mutex_lock(&arrival_mutex); //Car thread waits for new arrivals to be added to the queue
        while (arrival_done[minute] == 0) {
            pthread_cond_wait(&arrival_cond, &arrival_mutex);
        }
        pthread_mutex_unlock(&arrival_mutex);

        pthread_mutex_lock(&global_lock);
        while (current_car_turn != carID) { // This is where we force the cars to wait their turn before loading
            pthread_cond_wait(&car_cond, &global_lock);
        }
        pthread_mutex_unlock(&global_lock);

        // LOADING
        pthread_mutex_lock(&queue_lock);
        int toLoad;
	if (total_waiting < MAXPERCAR) {
		toLoad = total_waiting;
	} else {
		toLoad = MAXPERCAR;
	}

        for (int i = 0; i < toLoad; i++) {
            int guest_id = removeQueue(&waiting_line);
            if (guest_id == -1) break;
        }

        total_waiting -= toLoad;
        total_ridden += toLoad;

        fprintf(output_file, "Car %d: Loaded=%d, Remaining Waiting=%d\n", carID, toLoad, total_waiting);
        fflush(output_file);
        pthread_mutex_unlock(&queue_lock);

        pthread_mutex_lock(&global_lock);
        current_car_turn = (current_car_turn + 1) % CARNUM; //Update cars turn so the next car can begin loading
        pthread_cond_broadcast(&car_cond);
        pthread_mutex_unlock(&global_lock);
    }
    return NULL;
}

void addQueue(Queue* q, int guest_id) {
    Guest* new_guest = (Guest*)malloc(sizeof(Guest));
    if (!new_guest) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
    new_guest->id = guest_id;
    new_guest->next = NULL;

    if (q->back == NULL) {
        q->front = q->back = new_guest;
    } else {
        q->back->next = new_guest;
        q->back = new_guest;
    }
    q->size++;

    if (q->size > q->longest_line) {
        q->longest_line = q->size;
    }
}

int removeQueue(Queue* q) {
    if (q->front == NULL) {
        return -1;
    } else {
        Guest* temp = q->front;
        int guest_id = temp->id;

        q->front = q->front->next;
        if (q->front == NULL) {
            q->back = NULL;
        }
        free(temp);
        q->size--;
        return guest_id;
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

    if (CARNUM == 0 || MAXPERCAR == 0) {
        fprintf(stderr, "Usage: %s -N <CARNUM> -M <MAXPERCAR>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    output_file = fopen("simulation.txt", "w");
    if (!output_file) {
        perror("Failed to open file for writing");
        exit(EXIT_FAILURE);
    }

    time_t t;
    time(&t);
    srand48(t);

    pthread_t arrival_thread, timekeeper_thread, *ride_threads;
    ride_threads = (pthread_t*)malloc(sizeof(pthread_t) * CARNUM);

    pthread_create(&timekeeper_thread, NULL, timekeeper, NULL);
    pthread_create(&arrival_thread, NULL, generate_arrivals, &waiting_line);

    int *carIDs = (int*)malloc(sizeof(int) * CARNUM); //car threads are given specific ID's 0 to 3
    for (int i = 0; i < CARNUM; i++) {
        carIDs[i] = i; 
        pthread_create(&ride_threads[i], NULL, ride, &carIDs[i]);
    }

    pthread_join(arrival_thread, NULL);
    for (int i = 0; i < CARNUM; i++) {
        pthread_join(ride_threads[i], NULL);
    }
    pthread_join(timekeeper_thread, NULL);

    printf("Simulation Complete!\n");
    printf("Total Arrived: %d\n", total_arrived);
    printf("Total Riders: %d\n", total_ridden);
    printf("Total Rejected: %d\n", total_rejected);
    printf("Worst Case Line: %d\n", waiting_line.longest_line);

    fclose(output_file);
    free(ride_threads);
    free(carIDs);

    return 0;
}

