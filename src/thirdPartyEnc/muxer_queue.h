#pragma once

#include <pthread.h>
#include <stdbool.h>

#define MAX_FRAMES 10
#define MAX_DATA_SIZE 1024U * 500U * 10U //500KB * 10

typedef struct {
    char* dataptr;
    int size;
    bool endflag;
} FrameData;

typedef struct FrameQueue{
    FrameData* buffer[MAX_FRAMES];
    int front, rear, count;
    pthread_mutex_t mutex;
    pthread_cond_t cond_not_full;
    pthread_cond_t cond_not_empty;
} FrameQueue;


void initFrameQueue(FrameQueue* queue);
void destroyFrameQueue(FrameQueue* queue);
void FrameQueuePush(FrameQueue* queue, const char* data, int size, bool endflag);
FrameData* FrameQueuePop(FrameQueue* queue);