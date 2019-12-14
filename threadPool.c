#include <unistd.h>
#include "threadPool.h"

void* threadHandler(void* tp);
void freePool(ThreadPool* tp);

/*
 * Create the thread pool.
 */
ThreadPool* tpCreate(int numOfThreads) {
    // create a new thread pool and fill all fields.
    ThreadPool* tp = (ThreadPool*) malloc (sizeof(ThreadPool));
    if(tp == NULL) {
        printf("Error allocating memory for threadpool.");
        return NULL;
    }
    // initialize a mutex.
    if (pthread_mutex_init(&(tp->lock), NULL) != 0) {
        write(STDERR_FILENO, "Error in system call.\n", 512);
        freePool(tp);
        return NULL;
    }
    // initialize cond.
    if (pthread_cond_init(&(tp->condition), NULL) != 0) {
        write(STDERR_FILENO, "Error in system call.\n", 512);
        freePool(tp);
        return NULL;
    }
    // create a task queue.
    tp->tasksQueue = osCreateQueue();
    // initialize number of threads and allocate memory for threads array according to it.
    tp->threadsNum = numOfThreads;
    tp->threadsArray = (pthread_t*) malloc (sizeof(pthread_t) * tp->threadsNum);
    if(tp->threadsArray == NULL) {
        printf("Error allocating memory for threads array.");
        return NULL;
    }
    // allow inserting new tasks.
    tp->shouldAcceptNewTasks = 1;
    // create the threads.
    int i;
    for (i = 0; i < tp->threadsNum; i++) {
        pthread_create(&(tp->threadsArray[i]), NULL, threadHandler, (void*) tp);
    }
    return tp;
}

/*
 * Destroy the thread pool.
 */
void tpDestroy(ThreadPool* threadPool, int shouldWaitForTasks) {
    pthread_mutex_lock(&(threadPool->lock));
    // if this is not the first time calling the function, return. else, stop accepting new tasks.
    if (threadPool->shouldAcceptNewTasks == 0) {
        pthread_mutex_unlock(&(threadPool->lock));
        return;
    } else if (threadPool->shouldAcceptNewTasks == 1) {
        threadPool->shouldAcceptNewTasks = 0;
        pthread_mutex_unlock(&(threadPool->lock));
    }
    if (pthread_cond_broadcast(&(threadPool->condition)) != 0) {
        write(STDERR_FILENO, "Error in system call.\n", 512);
        freePool(threadPool);
        return;
    }
    // wait for the threads to finish before main thread exits.
    int i;
    for(i = 0; i < threadPool->threadsNum; i++) {
        pthread_join(threadPool->threadsArray[i], NULL);
    }
    // if should not wait for current pending tasks, empty the queue. tasks that are already running are not affected.
    if(shouldWaitForTasks == 0) {
        pthread_mutex_lock(&(threadPool->lock));
        while(!osIsQueueEmpty(threadPool->tasksQueue)) {
            Task* t = osDequeue(threadPool->tasksQueue);
            free(t);
        }
        pthread_mutex_unlock(&(threadPool->lock));
    }
    freePool(threadPool);
}

/*
 * Insert a new task to the task queue of the thread pool.
 */
int tpInsertTask(ThreadPool* threadPool, void (*computeFunc) (void *), void* param) {
    if(threadPool == NULL) {
        return -1;
    }
    // if destroy function was called, return -1 and don't add the new func.
    if (!threadPool->shouldAcceptNewTasks) {
        return -1;
    }
    // create a new task.
    Task* t = (Task*) malloc (sizeof(Task));
    // set the func and param to the new task.
    t->func = computeFunc;
    t->parameter = param;
    // lock the thread pool, enqueue the task and unlock it.
    pthread_mutex_lock(&(threadPool->lock));
    if(threadPool->tasksQueue != NULL) {
        osEnqueue(threadPool->tasksQueue, t);
    } else {
        free(t);
    }
    if (pthread_cond_signal(&(threadPool->condition)) != 0) {
        pthread_mutex_unlock(&(threadPool->lock));
        freePool(threadPool);
        return 1;
    }
    pthread_mutex_unlock(&(threadPool->lock));
    return 0;
}

/*
 * Handle a single thread's execution. Each thread is running in a while loop, and picking tasks from the queue
 * each time it finishes executing the old task.
 * When the pool is destroyed, the threads will finish the execution of current tasks and then quit.
 */
void* threadHandler(void* tp) {
    ThreadPool* threadPool = (ThreadPool*) tp;
    while(1) {
        // if queue is empty and should not accept new tasks, finish.
        if((osIsQueueEmpty(threadPool->tasksQueue)) && (!threadPool->shouldAcceptNewTasks)) {
            break;
        }
        // lock the thread pool and block until owning the lock.
        pthread_mutex_lock(&(threadPool->lock));
        while((osIsQueueEmpty(threadPool->tasksQueue)) && (threadPool->shouldAcceptNewTasks)) {
            pthread_cond_wait(&(threadPool->condition), &(threadPool->lock));
        }
        // if queue is not empty, dequeue a task from the tasks queue and execute it.
        if(!osIsQueueEmpty(threadPool->tasksQueue)) {
            Task* t = osDequeue(threadPool->tasksQueue);
            pthread_mutex_unlock(&(threadPool->lock));
            (*(t->func))(t->parameter);
            free(t);
            // if the queue is empty, just unlock the thread pool.
        } else {
            pthread_mutex_unlock(&(threadPool->lock));
        }
    }
    return(NULL);
}

/*
 * Free all the allocated memory of the thread pool.
 */
void freePool(ThreadPool* tp) {
    pthread_mutex_lock(&(tp->lock));
    // empty the tasks queue.
    while(!osIsQueueEmpty(tp->tasksQueue)) {
        Task* t = osDequeue(tp->tasksQueue);
        free(t);
    }
    pthread_mutex_unlock(&(tp->lock));
    // after all threads finished, free all of the remaining memory.
    free(tp->threadsArray);
    osDestroyQueue(tp->tasksQueue);
    pthread_mutex_destroy(&(tp->lock));
    free(tp);
}

