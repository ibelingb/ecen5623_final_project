/***********************************************************************************
 * @author Brian Ibeling
 * ibelingb@colorado.edu
 * 
 * Real-time Embedded Systems
 * ECEN5623 - Sam Siewert
 * @date 25Jul2020
 * Ubuntu 18.04 LTS and RPi 3B+
 ************************************************************************************
 *
 * @file frameWrite.c
 * @brief 
 *
 ************************************************************************************
 * References and Resources:
 *   - http://ecee.colorado.edu/~ecen5623/ecen/ex/Linux/sequencer_generic/seqgen3.c
 *   - https://www.ibm.com/support/knowledgecenter/en/SSLTBW_2.3.0/com.ibm.zos.v2r3.bpxbd00/ptkill.htm
 ************************************************************************************
 */
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

#define SCHED_TYPE SCHED_FIFO

#define SEQ_TIMER_INTERVAL_NSEC (8333333) // 120 Hz

#define ACQUIRE_FRAMES_EXEC_RATE_HZ (24)
#define DIFFERENCE_FRAMES_EXEC_RATE_HZ (2)
#define PROC_WRITE_FRAMES_EXEC_RATE_HZ (1)
//#define WRITE_FRAMES_EXEC_RATE_HZ (1)

#define ACQUIRE_FRAMES_MOD_CALC (120 / ACQUIRE_FRAMES_EXEC_RATE_HZ)
#define DIFFERENCE_FRAMES_MOD_CALC (120 / DIFFERENCE_FRAMES_EXEC_RATE_HZ)
#define PROC_WRITE_FRAMES_MOD_CALC (120 / PROC_WRITE_FRAMES_EXEC_RATE_HZ)

/*------------------------------------------------------------------------*/
static unsigned long long sequenceCount = 0;

seqThreadParams_t sequencerParams;
struct timespec timeNow;
sem_t appCompleteSem;
timer_t seqTimer;

/*------------------------------------------------------------------------*/
/*** METHODS ***/
/*------------------------------------------------------------------------*/
void shutdownApp(int sig) {
  clock_gettime(SYSLOG_CLOCK_TYPE, &timeNow);
  syslog(LOG_INFO, "Sequecer - Shutdown Signal received, signaling all other threads to shutdown at:, %.2f", 
         TIMESPEC_TO_MSEC(timeNow));
  printf("Shutting down App...\n");

  /* Max desired frames saved - initiate shutdown of application in reverse order services were started */
  pthread_kill(sequencerParams.tidProcThread, SIGNAL_KILL_PROC);
  sleep(3); /* Delay so frameProc can avoid getting blocked by MQ read/write calls */
  pthread_kill(sequencerParams.tidDiffThread, SIGNAL_KILL_DIFF);
  pthread_kill(sequencerParams.tidAcqThread, SIGNAL_KILL_ACQ);

  sem_post(&appCompleteSem);
}

/*------------------------------------------------------------------------*/
void sequencer(int signal) {
/*
#if defined(TIMESTAMP_SYSLOG_OUTPUT)
  clock_gettime(SYSLOG_CLOCK_TYPE, &timeNow);
  syslog(LOG_INFO, "%s cycle start (msec):, %.2f", __func__, TIMESPEC_TO_MSEC(timeNow));
#endif
*/

  /* Increment sequence count */
  sequenceCount++;

  /* Post a semaphore for each service based on sub-rate of sequencer */
  // Acquire Frames @ 24 Hz
  if((sequenceCount % (int)ACQUIRE_FRAMES_MOD_CALC) == 0) {
    sem_post(sequencerParams.pAcqSema);
  }

  // Determine Frame Differences @ 2 Hz
  if((sequenceCount % (int)DIFFERENCE_FRAMES_MOD_CALC) == 0) {
    sem_post(sequencerParams.pDiffSema);
  }

  // Process frame images @ 1 Hz
  // Write Frames to memory @ 1 Hz
  // Additionally check if max number of frames saved
  if((sequenceCount % (int)PROC_WRITE_FRAMES_MOD_CALC) == 0) {
    sem_post(sequencerParams.pProcSema);
    sem_post(sequencerParams.pWriteSema);
  }


/*
#if defined(TIMESTAMP_SYSLOG_OUTPUT)
  clock_gettime(SYSLOG_CLOCK_TYPE, &timeNow);
  syslog(LOG_INFO, "%s cycle done (msec):, %.2f", __func__, TIMESPEC_TO_MSEC(timeNow));
#endif
*/
}

/*------------------------------------------------------------------------*/
void *sequencerTask(void *arg) {
  struct itimerspec itime = {{1, 0}, {1, 0}};
  struct itimerspec last_itime;
  int flags = 0;

  /* get thread parameters */
  if(arg == NULL) {
    syslog(LOG_ERR, "invalid arg provided to %s", __func__);
    return NULL;
  }
  sequencerParams = *(seqThreadParams_t *)arg;

  if(sequencerParams.pAcqSema == NULL) {
    syslog(LOG_ERR, "invalid Acq semaphore provided to %s", __func__);
    return NULL;
  }
  if(sequencerParams.pDiffSema == NULL) {
    syslog(LOG_ERR, "invalid Diff semaphore provided to %s", __func__);
    return NULL;
  }
  if(sequencerParams.pProcSema == NULL) {
    syslog(LOG_ERR, "invalid Proc semaphore provided to %s", __func__);
    return NULL;
  }
  if(sequencerParams.pWriteSema == NULL) {
    syslog(LOG_ERR, "invalid Write semaphore provided to %s", __func__);
    return NULL;
  }

  /* Register the signal handler */ 
  signal(SIGNAL_KILL_SEQ, shutdownApp);

  /* Initialize appComplete semaphore and shutdownApp variable */
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