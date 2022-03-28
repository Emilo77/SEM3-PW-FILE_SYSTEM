#pragma once

#include <stdbool.h>
#include <sys/types.h>

typedef struct ReadWrite ReadWrite;

struct ReadWrite {
    pthread_mutex_t lock;
    pthread_cond_t readers;
    pthread_cond_t writers;
    pthread_cond_t lowPriorities;
    int readersCount, writersCount, readersWait, writersWait;
    int change;
};

struct ReadWrite;

/* Initialize a buffer */
ReadWrite *readwrite_new();

/* Destroy the buffer */
void readwrite_destroy(ReadWrite *rw);

/* Starting protocol of reader */
void readerStart(ReadWrite *rw);

/* Ending protocol of reader */
void readerEnd(ReadWrite *rw);

/* Starting protocol of writer */
void writerStart(ReadWrite *rw);

/* Ending protocol of writer */
void writerEnd(ReadWrite *rw);

/* Starting protocol of lowPriority */
void lowPriorityStart(ReadWrite *rw);
