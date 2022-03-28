#include "readers-writers.h"
#include <stdlib.h>
#include <pthread.h>
#include "err.h"


ReadWrite *readwrite_new() {
    ReadWrite *rw = malloc(sizeof(ReadWrite));
    if(!rw) {
        fatal("Malloc failed!");
    }
    int err;
    if ((err = pthread_mutex_init(&rw->lock, 0)) != 0)
        syserr(err, "mutex readwrite_new failed");
    if ((err = pthread_cond_init(&rw->readers, 0)) != 0)
        syserr(err, "cond readwrite_new readers failed");
    if ((err = pthread_cond_init(&rw->writers, 0)) != 0)
        syserr(err, "cond readwrite_new writers failed");
    if ((err = pthread_cond_init(&rw->lowPriorities, 0)) != 0)
        syserr(err, "cond readwrite_new writers failed");

    rw->readersCount = 0;
    rw->writersCount = 0;
    rw->readersWait = 0;
    rw->writersWait = 0;
    rw->change = 0;

    return rw;
}

void readwrite_destroy(ReadWrite *rw) {
    int err;

    if ((err = pthread_cond_destroy(&rw->readers)) != 0)
        syserr(err, "cond readwrite_destroy 1 failed");
    if ((err = pthread_cond_destroy(&rw->writers)) != 0)
        syserr(err, "cond readwrite_destroy 2 failed");
    if ((err = pthread_cond_destroy(&rw->lowPriorities)) != 0)
        syserr(err, "cond readwrite_destroy 2 failed");
    if ((err = pthread_mutex_destroy(&rw->lock)) != 0)
        syserr(err, "mutex readwrite_destroy failed");

    free(rw);
}

void readerStart(ReadWrite *rw) {
    int err;
    int start = 1;
    if ((err = pthread_mutex_lock(&rw->lock)) != 0)
        syserr(err, "lock failed");

    if (rw->writersWait > 0 || rw->writersCount > 0 || rw->change > 0) {
        while (start == 1 || rw->writersCount > 0 || rw->change > 0) {
            start = 0;
            rw->readersWait++;
            if ((err = pthread_cond_wait(&rw->readers, &rw->lock)) != 0)
                syserr(err, "cond wait failed");
            rw->readersWait--;
            rw->change = 0;
        }
    }
    rw->readersCount++;
    if (rw->readersWait > 0) {
        rw->change = 1;
        if ((err = pthread_cond_signal(&rw->readers)) != 0)
            syserr(err, "cond signal failed");
    }

    if ((err = pthread_mutex_unlock(&rw->lock)) != 0)
        syserr(err, "unlock failed");
}

void readerEnd(ReadWrite *rw) {
    int err;
    if ((err = pthread_mutex_lock(&rw->lock)) != 0)
        syserr(err, "unlock failed");

    rw->readersCount--;

    if (rw->readersCount == 0 && rw->writersWait > 0) {
        rw->change = 1;
        if ((err = pthread_cond_signal(&rw->writers)) != 0)
            syserr(err, "cond signal failed");
    } else {
        if ((err = pthread_cond_signal(&rw->lowPriorities)) != 0)
            syserr(err, "cond signal failed");
    }

    if ((err = pthread_mutex_unlock(&rw->lock)) != 0)
        syserr(err, "unlock failed");
}

void writerStart(ReadWrite *rw) {
    int err;
    if ((err = pthread_mutex_lock(&rw->lock)) != 0)
        syserr(err, "lock failed");

    while (rw->readersCount > 0 || rw->writersCount > 0 || rw->change > 0) {
        rw->writersWait++;
        if ((err = pthread_cond_wait(&rw->writers, &rw->lock)) != 0)
            syserr(err, "cond wait failed");
        rw->writersWait--;
        rw->change = 0;
    }
    rw->writersCount++;
    if ((err = pthread_mutex_unlock(&rw->lock)) != 0)
        syserr(err, "unlock failed");
}

void writerEnd(ReadWrite *rw) {
    int err;

    if ((err = pthread_mutex_lock(&rw->lock)) != 0)
        syserr(err, "lock failed");

    rw->writersCount--;
    if (rw->readersWait > 0) {
        rw->change = 1;
        if ((err = pthread_cond_signal(&rw->readers)) != 0)
            syserr(err, "cond signal failed");
    } else if (rw->writersWait > 0) {
        rw->change = 1;
        if ((err = pthread_cond_signal(&rw->writers)) != 0)
            syserr(err, "cond signal failed");
    } else {
        if ((err = pthread_cond_signal(&rw->lowPriorities)) != 0)
            syserr(err, "cond signal failed");
    }

    if ((err = pthread_mutex_unlock(&rw->lock)) != 0)
        syserr(err, "unlock failed");
}

void lowPriorityStart(ReadWrite *rw) {
    int err;

    if ((err = pthread_mutex_lock(&rw->lock)) != 0)
        syserr(err, "lock failed");

    while (rw->readersCount > 0 || rw->writersCount > 0 || rw->readersWait >
    0 || rw->writersWait) {
        if ((err = pthread_cond_wait(&rw->lowPriorities, &rw->lock)) != 0)
            syserr(err, "cond wait failed");
    }

    if ((err = pthread_mutex_unlock(&rw->lock)) != 0)
        syserr(err, "unlock failed");
}

