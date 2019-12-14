#ifndef __THREAD_POOL__
#define __THREAD_POOL__
#include <sys/types.h>
#include "osqueue.h"
#include <malloc.h>
#include <pthread.h>
#include <stdlib.h>

typedef struct thread_pool
{
    pthread_cond_t condition;
    pthread_mutex_t lock;
    pthread_t* threadsArray;
    OSQueue* tasksQueue;
    int threadsNum;
    int shouldAcceptNewTasks;
}ThreadPool;

typedef struct Task
{
    void (*func)(void* parameter);
    void* parameter;
}Task;

ThreadPool* tpCreate(int numOfThreads);

void tpDestroy(ThreadPool* threadPool, int shouldWaitForTasks);

int tpInsertTask(ThreadPool* threadPool, void (*computeFunc) (void *), void* param);

#endif