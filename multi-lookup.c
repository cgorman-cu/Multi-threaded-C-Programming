#include "multi-lookup.h"

void* requester(things* input){

	FILE *inputfp = NULL;
	int fileCount = 0;
	char* inputFile = NULL;

	pthread_mutex_lock(&input->jobsLock);
	while(!queue_is_empty(&input->jobQueue)){
		inputFile = dequeue(&input->jobQueue);
		pthread_mutex_unlock(&input->jobsLock);

		inputfp = fopen(inputFile, "r");
		if(!inputfp) {
	    		fprintf(stderr, "Error Opening Input File: %s", inputFile);
	    		return NULL;
		}
	
		while(1) {
			char *hostName = (char *)malloc(sizeof(char) * MAX_NAME_LENGTH);
			// check for failed malloc
			if(hostName == NULL)
			{
				fprintf(stderr, "heap full; req thrd hostname malloc failed\n");
				break;
			}
			// everything in file has been read
			if(fscanf(inputfp, INPUTFS, hostName) <= 0)
			{
				free(hostName);
				break;
			}
			sem_wait(&input->empty);
			pthread_mutex_lock(&input->qLock);
			// critical section
			enqueue(&input->hostNameQueue, hostName);
			pthread_mutex_unlock(&input->qLock);
			sem_post(&input->full);
		}
		fclose(inputfp);
		fileCount++;
		pthread_mutex_lock(&input->jobsLock);
		if(queue_is_empty(&input->jobQueue)){
			pthread_mutex_unlock(&input->jobsLock);
			break;
		}
	}
	//once a thread is done serviceing files it writes to file
	pthread_mutex_lock(&input->servicedLock);
	fprintf(input->servicedfp, "Thread 0x%lx serviced %d files\n", pthread_self(), fileCount);
	pthread_mutex_unlock(&input->servicedLock);
	return NULL;
}

void* resolver(things* input){
	char* hostName;
	char ipAddress[INET6_ADDRSTRLEN];
	
	while(1) {
		//lock the queue to check so that one thread can check and modify the queue
		sem_wait(&input->full);
		pthread_mutex_lock(&input->qLock);
		if(!queue_is_empty(&input->hostNameQueue)) {
			hostName = dequeue(&input->hostNameQueue);
			if(hostName != NULL)
			{
				pthread_mutex_unlock(&input->qLock);
				sem_post(&input->empty);
				//checks the dnslookup
				int dnsRC = dnslookup(hostName, ipAddress, sizeof(ipAddress));
				
				if(dnsRC == UTIL_FAILURE)
			    	{
					fprintf(stderr, "Bogus HostName: %s\n", hostName);
					strncpy(ipAddress, "", sizeof(ipAddress));
			    	}
				pthread_mutex_lock(&input->outputLock);
				fprintf(input->outputfp, "%s,%s\n", hostName, ipAddress);
				pthread_mutex_unlock(&input->outputLock);

			}
			free(hostName);
			
		} else {
			pthread_mutex_unlock(&input->qLock);
			break;
		}
	}
	return NULL; 
}

int main(int argc, char* argv[]){

	gettimeofday(&start, NULL);
	int numInput = argc - 5;
	int numReqTh = atoi(argv[1]);
	int numResTh = atoi(argv[2]);
	if(numInput < MAX_INPUT_FILES){
		fprintf(stderr, "Not enough inputfiles.\n");
	}
	things stuff;
	queue jobQ;
	queue_init(&jobQ, numInput);
	//gets all the input files
	for(int i = 0; i < numInput; i++){
		enqueue(&jobQ, argv[i+5]);
	}
	stuff.jobQueue = jobQ;
	int rc;
	//initializes mutexs
	pthread_mutex_t qLock;
	pthread_mutex_t servicedLock;
	pthread_mutex_t outputLock;
	pthread_mutex_t jobsLock;
	sem_t e;
	sem_t f;

	pthread_mutex_init(&qLock, NULL);
    	pthread_mutex_init(&servicedLock, NULL);
	pthread_mutex_init(&outputLock, NULL);
	pthread_mutex_init(&jobsLock, NULL);
	sem_init(&e, 0, ARRAY_SIZE);
	sem_init(&f, 0, 0);

	stuff.qLock = qLock;
	stuff.servicedLock = servicedLock;
	stuff.outputLock = outputLock;
	stuff.jobsLock = jobsLock;
	stuff.empty = e;
	stuff.full = f;

	//initializes queue
	queue hostNameQ;
    	queue_init(&hostNameQ, ARRAY_SIZE);
	stuff.hostNameQueue = hostNameQ;
    	//initializes threads
	pthread_t *requesterThreads;
	pthread_t *resolverThreads;
    	if(numReqTh <= MAX_REQUESTER_THREADS){
  		requesterThreads = (pthread_t *) malloc( sizeof(pthread_t)*numReqTh);
	} else {
		printf("Error you can only have a max of %d requester threads and a min of 1 setting requester thread count to max.\n", MAX_REQUESTER_THREADS);
		numReqTh = MAX_REQUESTER_THREADS;
  		requesterThreads = (pthread_t *) malloc( sizeof(pthread_t)*MAX_REQUESTER_THREADS);
	}
   	if(numResTh <= MAX_RESOLVER_THREADS && numResTh > 0){
  		resolverThreads = (pthread_t *) malloc( sizeof(pthread_t)*numResTh);
	} else {
		printf("Error you can only have a max of %d resolver threads and a min of 1 setting resolver thread count to max.\n", MAX_RESOLVER_THREADS);
		numResTh = MAX_RESOLVER_THREADS;
  		resolverThreads = (pthread_t *) malloc( sizeof(pthread_t)*MAX_RESOLVER_THREADS);
	}

    	pthread_attr_t attr;
    	pthread_attr_init(&attr);
    	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	//print number of threads
	printf("Number of requester threads is %d\n", numReqTh);
	printf("Number of resolver threads is %d\n", numResTh);

	//checks if file open fails
	stuff.outputfp = fopen(argv[4], "w");
    	if(!stuff.outputfp) {
		fprintf(stderr, "Error Opening Output File");
		return EXIT_FAILURE;
    	}
	stuff.servicedfp = fopen(argv[3], "w");
	if(!stuff.servicedfp) {
		fprintf(stderr, "Error Opening serviced File");
		return EXIT_FAILURE;
    	}
	//creates threads
	if(numReqTh == 1){
		rc = pthread_create(&(requesterThreads[0]), &attr, (void *)requester, &stuff); //pass it all the stuff
		if (rc) { 
			fprintf(stderr, "ERROR: return code from pthread_create() is %d\n", rc);
			exit(EXIT_FAILURE);
		}
	} else {
		for(int i = 0; i < numReqTh; i++) { 
			rc = pthread_create(&(requesterThreads[i]), &attr, (void *)requester, &stuff); //pass it all the stuff
			if (rc) { 
				fprintf(stderr, "ERROR: return code from pthread_create() is %d\n", rc);
				exit(EXIT_FAILURE);
			}
		}
	}
	
	if(numResTh == 1){
		rc = pthread_create(&(resolverThreads[0]), NULL, (void *)resolver, &stuff); //pass it all the stuff
		if (rc) { 
			fprintf(stderr, "ERROR: return code from pthread_create() is %d\n", rc);
			exit(EXIT_FAILURE);
		}
	} else {
		for(int i = 0; i < numResTh; i++) { 
			rc = pthread_create(&(resolverThreads[i]), NULL, (void *)resolver, &stuff); //pass it all the stuff
			if (rc)
			{ 
				fprintf(stderr, "ERROR: return code from pthread_create() is %d\n", rc);
				exit(EXIT_FAILURE);
			}
		}
	}
	//thread joins
	for(int i = 0; i < numReqTh;i++) {
		pthread_join(requesterThreads[i], NULL);
    	}
	fclose(stuff.servicedfp);
	for(int i = 0; i < numResTh; i++) {
		sem_post(&stuff.full);
	}
    	for(int i = 0; i < numResTh; i++) {
		pthread_join(resolverThreads[i], NULL);
    	}
	//make sure there is no memory leaks
	free(requesterThreads);
	free(resolverThreads);

	fclose(stuff.outputfp);

    	queue_cleanup(&hostNameQ);
	queue_cleanup(&jobQ);
 
    	pthread_mutex_destroy(&qLock);
     	pthread_mutex_destroy(&servicedLock);
	pthread_mutex_destroy(&outputLock);
	pthread_mutex_destroy(&jobsLock);
	sem_destroy(&e);
	sem_destroy(&f);

	gettimeofday(&end, NULL);
	double time_taken = (((end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec)))/1000000.00;
	printf("%s: Total time is %lf seconds\n", argv[0], time_taken);

    	return EXIT_SUCCESS;
	

}
