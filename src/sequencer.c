/* ECEN5623 - Real-Time Embedded Systems
 * Brian Ibeling
 * 7/25/2020
 * sequencer.c
 *
 * TODO
 *
 * References and Resources:
 *   - http://ecee.colorado.edu/~ecen5623/ecen/ex/Linux/sequencer_generic/seqgen3.c
 */
/*------------------------------------------------------------------------*/
// This is necessary for CPU affinity macros in Linux
//#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <semaphore.h>

#include <syslog.h>
#include <sys/time.h>
#include <sys/sysinfo.h>
#include <errno.h>

#include <signal.h>

#include "sequencer.h"
#include "project.h"

/*------------------------------------------------------------------------*/
#define USEC_PER_MSEC (1000)
#define NANOSEC_PER_MSEC (1000000)
#define NANOSEC_PER_SEC (1000000000)

#define CLOCK_TYPE CLOCK_MONOTONIC_RAW
#define SCHED_TYPE SCHED_FIFO

#define SEQ_TIMER_INTERVAL_NSEC (8333333) // 120 Hz
//#define SEQ_TIMER_INTERVAL_NSEC (10000000) // 100 Hz

#define ACQUIRE_FRAMES_EXEC_RATE_HZ (24)
#define DIFFERENCE_FRAMES_EXEC_RATE_HZ (2)
#define SELECT_FRAMES_EXEC_RATE_HZ (1)
#define WRITE_FRAMES_EXEC_RATE_HZ (1)

#define ACQUIRE_FRAMES_MOD_CALC (120 / ACQUIRE_FRAMES_EXEC_RATE_HZ)
#define DIFFERENCE_FRAMES_MOD_CALC (120 / DIFFERENCE_FRAMES_EXEC_RATE_HZ)
#define SELECT_FRAMES_MOD_CALC (120 / SELECT_FRAMES_EXEC_RATE_HZ)
#define WRITE_FRAMES_MOD_CALC (120 / WRITE_FRAMES_EXEC_RATE_HZ)

/*------------------------------------------------------------------------*/
static unsigned long long sequenceCount = 0;
static unsigned long long framesSaved = 0;

sem_t appCompleteSem;
sem_t *pAcqSema;
sem_t *pDiffSema;
sem_t *pProcSema;
sem_t *pWriteSema;
sem_t *pSeqSema; // TODO - remove?

/*------------------------------------------------------------------------*/
/*** METHODS ***/
/*------------------------------------------------------------------------*/
double getTimeMsec(void) {
  struct timespec event_ts = {0, 0};

  clock_gettime(CLOCK_TYPE, &event_ts);
  return ((event_ts.tv_sec) * 1000.0) + ((event_ts.tv_nsec) / 1000000.0);
}

/*------------------------------------------------------------------------*/
double realtime(struct timespec *tsptr) {
  return ((double)(tsptr->tv_sec) + (((double)tsptr->tv_nsec) / 1000000000.0));
}

/*------------------------------------------------------------------------*/
void sequencer(int signal) {
  /* Increment sequence count */
  sequenceCount++;

  /* Post a semaphore for each service based on sub-rate of sequencer */
  // Acquire Frames @ 24 Hz
  if((sequenceCount % (int)ACQUIRE_FRAMES_MOD_CALC) == 0) {
    sem_post(pAcqSema);
  }

  // Determine Frame Differences @ 2 Hz
  if((sequenceCount % (int)DIFFERENCE_FRAMES_MOD_CALC) == 0) {
    sem_post(pDiffSema);
  }

  // Process frame images @ 1 Hz
  if((sequenceCount % (int)SELECT_FRAMES_MOD_CALC) == 0) {
    sem_post(pProcSema);
  }

  // Write Frames to memory @ 1 Hz
  if((sequenceCount % (int)WRITE_FRAMES_MOD_CALC) == 0) {
    sem_post(pWriteSema);
    framesSaved++; // TODO - Determine better way to track this
  }

  /* Max desired frames saved - initiate shutdown of application */
  if(framesSaved == MAX_FRAME_COUNT) {
    sem_post(&appCompleteSem);
  }
}

/*------------------------------------------------------------------------*/
void *sequencerTask(void *arg) {
  timer_t seqTimer;
  struct itimerspec itime = {{1, 0}, {1, 0}};
  struct itimerspec last_itime;
  int flags = 0;

  /* get thread parameters */
  if(arg == NULL) {
    syslog(LOG_ERR, "invalid arg provided to %s", __func__);
    return NULL;
  }

  // TODO - update to use pointer to seqThreadParam struct
  seqThreadParams_t semaphores = *(seqThreadParams_t *)arg;
  pAcqSema = semaphores.pAcqSema;
  pDiffSema = semaphores.pDiffSema;
  pProcSema = semaphores.pProcSema;
  pWriteSema = semaphores.pWriteSema;
  pSeqSema = semaphores.pSeqSema;

  if(pAcqSema == NULL) {
    syslog(LOG_ERR, "invalid Acq semaphore provided to %s", __func__);
    return NULL;
  }
  if(pDiffSema == NULL) {
    syslog(LOG_ERR, "invalid Diff semaphore provided to %s", __func__);
    return NULL;
  }
  if(pProcSema == NULL) {
    syslog(LOG_ERR, "invalid Proc semaphore provided to %s", __func__);
    return NULL;
  }
  if(pWriteSema == NULL) {
    syslog(LOG_ERR, "invalid Write semaphore provided to %s", __func__);
    return NULL;
  }
  if(pSeqSema == NULL) {
    syslog(LOG_ERR, "invalid Seq semaphore provided to %s", __func__);
    return NULL;
  }

  /* Initialize appComplete semaphore */
  sem_init(&appCompleteSem, 0, 0);

  /* Create Timer to trigger Sequencer at 120 Hz (8.3333 msec) */
  timer_create(CLOCK_REALTIME, NULL, &seqTimer);
  signal(SIGALRM, sequencer);
  itime.it_interval.tv_sec = 0;
  itime.it_interval.tv_nsec = SEQ_TIMER_INTERVAL_NSEC;
  itime.it_value.tv_sec = 0;
  itime.it_value.tv_nsec = SEQ_TIMER_INTERVAL_NSEC;
  timer_settime(seqTimer, flags, &itime, &last_itime);

  // Block until released from sequencer after 1800 acquired frames
  sem_wait(&appCompleteSem);

  // Cleanup thread and exit
  timer_delete(seqTimer);
  sem_destroy(&appCompleteSem);

  return NULL;
}
/*------------------------------------------------------------------------*/