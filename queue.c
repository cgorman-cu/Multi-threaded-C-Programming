#include <stdlib.h>

#include "queue.h"

int queue_init(queue* q, int size){
    
    int i;

    //user specified size or default
    if(size>0) {
	q->maxSize = size;
    }
    else {
	printf("Error need a user specified size");
	return QUEUE_FAILURE;
    }

    //malloc array
    q->array = malloc(sizeof(queue_node) * (q->maxSize));
    if(!(q->array)){	
	perror("Error on queue Malloc");
	return QUEUE_FAILURE;
    }

    for(i=0; i < q->maxSize; ++i){
	q->array[i].payload = NULL;
    }

    q->front = 0;
    q->end = 0;

    return q->maxSize;
}

int queue_is_empty(queue* q){
    if((q->front == q->end) && (q->array[q->front].payload == NULL)){
	return 1;
    }
    else{
	return 0;
    }
}

int queue_is_full(queue* q){
    if((q->front == q->end) && (q->array[q->front].payload != NULL)){
	return 1;
    }
    else{
	return 0;
    }
}

void* dequeue(queue* q){
    void* ret_payload;
	
    if(queue_is_empty(q)){
	return NULL;
    }
	
    ret_payload = q->array[q->front].payload;
    q->array[q->front].payload = NULL;
    q->front = ((q->front + 1) % q->maxSize);

    return ret_payload;
}

int enqueue(queue* q, void* new_payload){
    
    if(queue_is_full(q)){
	return QUEUE_FAILURE;
    }

    q->array[q->end].payload = new_payload;

    q->end = ((q->end+1) % q->maxSize);

    return QUEUE_SUCCESS;
}

void queue_cleanup(queue* q)
{
    while(!queue_is_empty(q)){
	dequeue(q);
    }

    free(q->array);
}

