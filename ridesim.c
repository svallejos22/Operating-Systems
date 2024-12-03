#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <unistd.h> // Required for usleep function
#include "random437.h"

#define MAXWAITPEOPLE 800
#define SIMULATION_TIME 600 // 600 minutes from 9:00am to 7:00pm

int waitingLine = 0;
int totalArrived = 0;
int totalRides = 0;
int totalRejected = 0;
int worstLineLength = 0;
int worstLineTime = 0;

pthread_mutex_t lineMutex;
sem_t rideSemaphore;

int CARNUM = 4;
int MAXPERCAR = 7;

FILE *outputFile;

void* rideThread(void* arg) {
    for (int minute = 0; minute < SIMULATION_TIME; ++minute) {
        if (minute >= SIMULATION_TIME) {
            break;
        }
        sem_wait(&rideSemaphore); // Wait for car to be loaded

        pthread_mutex_lock(&lineMutex);
        int passengers = (waitingLine >= MAXPERCAR) ? MAXPERCAR : waitingLine;
        waitingLine -= passengers;
        totalRides += passengers;
        pthread_mutex_unlock(&lineMutex);

        usleep(7000); // Loading time: 7 milliseconds to simulate 1 minute as 1 second
        usleep(53000); // Ride time: 53 milliseconds to simulate 1 minute as 1 second
    }
    return NULL;
}

void* arrivalThread(void* arg) {
    for (int minute = 0; minute < SIMULATION_TIME; ++minute) {
        int meanArrival;
        if (minute < 120) meanArrival = 25;
        else if (minute < 240) meanArrival = 45;
        else if (minute < 300) meanArrival = 35;
        else meanArrival = 25;

        int arrivals = poissonRandom(meanArrival);
        pthread_mutex_lock(&lineMutex);

        totalArrived += arrivals;
        int availableSpace = MAXWAITPEOPLE - waitingLine;
        int accepted = (arrivals <= availableSpace) ? arrivals : availableSpace;
        int rejected = arrivals - accepted;

        waitingLine += accepted;
        totalRejected += rejected;

        if (waitingLine > worstLineLength) {
            worstLineLength = waitingLine;
            worstLineTime = minute;
        }

        pthread_mutex_unlock(&lineMutex);
        sem_post(&rideSemaphore); // Signal that passengers are ready to load

        // Logging the status line to the output file
        fprintf(outputFile, "%03d arrive %d reject %d wait-line %d at %02d:%02d:00\n", minute, arrivals, rejected, waitingLine, 9 + minute / 60, minute % 60);

        usleep(1000); // Synchronize to real-time minute or virtual minute (1 millisecond to simulate 1 minute as 1 second)
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc != 5) {
        printf("Usage: %s -N <CARNUM> -M <MAXPERCAR>\n", argv[0]);
        return 1;
    }

    CARNUM = atoi(argv[2]);
    MAXPERCAR = atoi(argv[4]);

    outputFile = fopen("ride_simulation_output.txt", "w");
    if (outputFile == NULL) {
        perror("Failed to open output file");
        return 1;
    }

    pthread_mutex_init(&lineMutex, NULL);
    sem_init(&rideSemaphore, 0, 0);

    pthread_t arrivalThreadId;
    pthread_create(&arrivalThreadId, NULL, arrivalThread, NULL);

    pthread_t rideThreadIds[CARNUM];
    for (int i = 0; i < CARNUM; ++i) {
        pthread_create(&rideThreadIds[i], NULL, rideThread, NULL);
    }

    pthread_join(arrivalThreadId, NULL);
    for (int i = 0; i < CARNUM; ++i) {
        pthread_join(rideThreadIds[i], NULL);
    }

    // Writing final summary data to the output file
    fprintf(outputFile, "Total arrived: %d\n", totalArrived);
    fprintf(outputFile, "Total rides given: %d\n", totalRides);
    fprintf(outputFile, "Total rejected: %d\n", totalRejected);
    fprintf(outputFile, "Worst line length: %d at minute %d\n", worstLineLength, worstLineTime);

    fclose(outputFile);

    pthread_mutex_destroy(&lineMutex);
    sem_destroy(&rideSemaphore);

    return 0;
}

