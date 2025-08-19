#include "muxer_queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
void initFrameQueue(FrameQueue* queue) {
    queue->front = 0;
    queue->rear = 0;
    queue->count = 0;
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->cond_not_full, NULL);
    pthread_cond_init(&queue->cond_not_empty, NULL);
    for (int i = 0; i < MAX_FRAMES; ++i) {
        queue->buffer[i] = (FrameData*)malloc(sizeof(FrameData));
        queue->buffer[i]->dataptr = (char*)malloc(MAX_DATA_SIZE);
    }
}

void destroyFrameQueue(FrameQueue* queue) {
    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->cond_not_full);
    pthread_cond_destroy(&queue->cond_not_empty);
    for (int i = 0; i < MAX_FRAMES; ++i) {
        free(queue->buffer[i]->dataptr);
        queue->buffer[i]->dataptr = NULL;
        
        free(queue->buffer[i]);
        queue->buffer[i] = NULL;
    }
}

void FrameQueuePush(FrameQueue* queue, const char* data, int size, bool endflag) {
    //TODO
    static unsigned long lostNum = 0;
    pthread_mutex_lock(&queue->mutex);
    // while (queue->count == MAX_FRAMES) {
    if(queue->count == MAX_FRAMES) {
        // pthread_cond_wait(&queue->cond_not_full, &queue->mutex);
        pthread_mutex_unlock(&queue->mutex);
        lostNum ++;
        if(lostNum % 60 == 0)
            printf("l %d, queue is full, lost %ld frames", __LINE__, lostNum);
        // assert(false);
        return;
    }
    
    memcpy(queue->buffer[queue->rear]->dataptr, data, size);
    queue->buffer[queue->rear]->size = size;
    queue->buffer[queue->rear]->endflag = endflag;
    queue->rear = (queue->rear + 1) % MAX_FRAMES;
    ++queue->count;
    pthread_cond_signal(&queue->cond_not_empty);
    pthread_mutex_unlock(&queue->mutex);
}

FrameData* FrameQueuePop(FrameQueue* queue) {
    pthread_mutex_lock(&queue->mutex);
    while (queue->count == 0) {
        pthread_cond_wait(&queue->cond_not_empty, &queue->mutex);
    }
    FrameData* data = queue->buffer[queue->front];
    queue->front = (queue->front + 1) % MAX_FRAMES;
    --queue->count;
    pthread_cond_signal(&queue->cond_not_full);
    pthread_mutex_unlock(&queue->mutex);
    return data;
}
