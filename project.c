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
int CARNUM = 0;             // set by user input
int MAXPERCAR = 0;          // set by user input
int total_arrived = 0;
int total_rejected = 0;
int total_ridden = 0;
int total_waiting = 0;

// Define a Queue structure to manage guests waiting for the ride
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

// Global synchronization variables
int global_minute = 0;
int current_car_turn = 0; // Indicates which car should run next
pthread_mutex_t global_lock = PTHREAD_MUTEX_INITIALIZER; // Mutex for accessing the global clock
pthread_cond_t global_cond = PTHREAD_COND_INITIALIZER;   // Condition variable for the global clock
pthread_cond_t car_cond = PTHREAD_COND_INITIALIZER;      // Condition variable for car turns

// Mutex for queue operations
pthread_mutex_t queue_lock = PTHREAD_MUTEX_INITIALIZER;

// New synchronization variables
pthread_mutex_t arrival_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t arrival_cond = PTHREAD_COND_INITIALIZER;
int arrival_done[TOTALMINUTES] = {0};

// Function prototypes
void* timekeeper(void* arg);
void* generate_arrivals(void* arg);
void* ride(void* arg);
void addQueue(Queue* q, int guest_id);
int removeQueue(Queue* q);

void* timekeeper(void* arg) {
    for (int minute = 0; minute < TOTALMINUTES; minute++) {
        usleep(VIRTUALMINUTE * 1000);  // Sleep for the duration of a virtual minute

        pthread_mutex_lock(&global_lock);
        global_minute++;
        pthread_cond_broadcast(&global_cond); // Notify all threads of the minute update
        pthread_mutex_unlock(&global_lock);
    }
    return NULL;
}

void* generate_arrivals(void* arg) {
    Queue* q = (Queue*)arg;

    for (int minute = 0; minute < TOTALMINUTES; minute++) {
        // Synchronize with the global clock
        pthread_mutex_lock(&global_lock);
        while (global_minute != minute + 1) {
            pthread_cond_wait(&global_cond, &global_lock);
        }
        pthread_mutex_unlock(&global_lock);

        // Perform arrivals processing
        pthread_mutex_lock(&queue_lock);
        int meanArrival = (minute < 120) ? 25 : (minute < 240) ? 35 : (minute < 360) ? 45 : (minute < 480) ? 35 : 25;
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

        // Log the arrival information for this minute
        fprintf(output_file, "%03d arrive %d reject %d wait-line %d at %02d:%02d:00\n",
                minute, arrivals, rejected_on_arrival, q->size, 9 + minute / 60, minute % 60);
        fflush(output_file); // Ensure that the data is written immediately
        pthread_mutex_unlock(&queue_lock);

        // Signal ride threads that arrival processing for this minute is done
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
        // Wait for the global clock to reach the current minute
        pthread_mutex_lock(&global_lock);
        while (global_minute != minute + 1) {
            pthread_cond_wait(&global_cond, &global_lock);
        }
        pthread_mutex_unlock(&global_lock);

        // Wait until arrival processing for this minute is done
        pthread_mutex_lock(&arrival_mutex);
        while (arrival_done[minute] == 0) {
            pthread_cond_wait(&arrival_cond, &arrival_mutex);
        }
        pthread_mutex_unlock(&arrival_mutex);

        // Wait for this car's turn to load passengers
        pthread_mutex_lock(&global_lock);
        while (current_car_turn != carID) {
            pthread_cond_wait(&car_cond, &global_lock);
        }
        pthread_mutex_unlock(&global_lock);

        // Car loading passengers
        pthread_mutex_lock(&queue_lock);
        int toLoad = (total_waiting < MAXPERCAR) ? total_waiting : MAXPERCAR;

        for (int i = 0; i < toLoad; i++) {
            int guest_id = removeQueue(&waiting_line);
            if (guest_id == -1) break;
        }

        total_waiting -= toLoad;
        total_ridden += toLoad;

        fprintf(output_file, "Car %d: Loaded=%d, Remaining Waiting=%d\n", carID, toLoad, total_waiting);
        fflush(output_file); // Ensure that the data is written immediately
        pthread_mutex_unlock(&queue_lock);

        // Update the car turn for the next car
        pthread_mutex_lock(&global_lock);
        current_car_turn = (current_car_turn + 1) % CARNUM;
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

    // Track the longest line
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

    // Parse command-line arguments for CARNUM and MAXPERCAR
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

    // Check if CARNUM and MAXPERCAR have been set
    if (CARNUM == 0 || MAXPERCAR == 0) {
        fprintf(stderr, "Usage: %s -N <CARNUM> -M <MAXPERCAR>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Open file for writing logs
    output_file = fopen("simulation.txt", "w");
    if (!output_file) {
        perror("Failed to open file for writing");
        exit(EXIT_FAILURE);
    }

    // Seed the random number generator
    time_t t;
    time(&t);
    srand48(t);

    // Create threads
    pthread_t arrival_thread, timekeeper_thread, *ride_threads;
    ride_threads = (pthread_t*)malloc(sizeof(pthread_t) * CARNUM);

    pthread_create(&timekeeper_thread, NULL, timekeeper, NULL);
    pthread_create(&arrival_thread, NULL, generate_arrivals, &waiting_line);

    int *carIDs = (int*)malloc(sizeof(int) * CARNUM);
    for (int i = 0; i < CARNUM; i++) {
        carIDs[i] = i;  // Cars are indexed from 0 to CARNUM-1
        pthread_create(&ride_threads[i], NULL, ride, &carIDs[i]);
    }

    // Wait for threads to finish
    pthread_join(arrival_thread, NULL);
    for (int i = 0; i < CARNUM; i++) {
        pthread_join(ride_threads[i], NULL);
    }
    pthread_join(timekeeper_thread, NULL);

    // Print simulation summary
    printf("Simulation Complete!\n");
    printf("Total Arrived: %d\n", total_arrived);
    printf("Total Riders: %d\n", total_ridden);
    printf("Total Rejected: %d\n", total_rejected);
    printf("Worst Case Line: %d\n", waiting_line.longest_line);

    // Close log file
    fclose(output_file);

    // Free allocated memory
    free(ride_threads);
    free(carIDs);

    return 0;
}

