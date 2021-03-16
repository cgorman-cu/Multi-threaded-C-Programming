#ifndef MULTI_LOOKUP_H
#define MULTI_LOOKUP_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include "util.h"
#include "queue.h"

#define ARRAY_SIZE 20
#define MAX_INPUT_FILES 10
#define MAX_RESOLVER_THREADS 10
#define MAX_REQUESTER_THREADS 5
#define MAX_NAME_LENGTH 1025
#define INPUTFS "%1024s"
#define MAX_IP_LENGTH INET6_ADDRSTRLEN

struct timeval start, end;

typedef struct NotGlobalVars{
	pthread_mutex_t qLock;
	pthread_mutex_t servicedLock;
	pthread_mutex_t outputLock;
	pthread_mutex_t jobsLock;
	sem_t empty;
	sem_t full;
	queue hostNameQueue;
	queue jobQueue;
	FILE* servicedfp;
	FILE* outputfp;
} things;

void* requester(things* input);

void* resolver(things* input);

#endif
