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
 * @file sequencer.c
 * @brief Sequencer service that drives execution of all other services via synchronized semaphores
 * 
 * The Sequencer acts as the executive dispatch service and provides a base rate frequency from 
 * which all other threads are driven from. This is done by updating blocking semaphores to all 
 * other service threads within the system at specified intervals. These dispatch intervals are 
 * a substrate of the Sequencer’s base rate, which is set to a default value 120 Hz. 
 * 
 * This service is driven from the high frequency oscillator provided by the RPi3, checking a 
 * comparator against the Interval Timer (IT) and sending an interrupt signal to the Sequencer 
 * at the desired frequency.
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
/* MACROS */
#define USEC_PER_MSEC (1000)
#define NANOSEC_PER_MSEC (1000000)
#define NANOSEC_PER_SEC (1000000000)

#define SCHED_TYPE SCHED_FIFO

#define SEQ_TIMER_INTERVAL_NSEC (8333333) // 120 Hz base rate for sequencer

#define ACQUIRE_FRAMES_EXEC_RATE_HZ (24) /* Freq to trigger AcquireFrames thread service */
#define DIFFERENCE_FRAMES_EXEC_RATE_HZ (2) /* Freq to trigger DifferenceFrames thread service */
#define PROC_WRITE_FRAMES_EXEC_RATE_HZ (1) /* Freq to trigger FrameProcessing and FrameWrite thread services */

#define ACQUIRE_FRAMES_MOD_CALC (120 / ACQUIRE_FRAMES_EXEC_RATE_HZ)
#define DIFFERENCE_FRAMES_MOD_CALC (120 / DIFFERENCE_FRAMES_EXEC_RATE_HZ)
#define PROC_WRITE_FRAMES_MOD_CALC (120 / PROC_WRITE_FRAMES_EXEC_RATE_HZ)

/*------------------------------------------------------------------------*/
/* GLOBAL VARIABLES */
static unsigned long long sequenceCount = 0;

seqThreadParams_t sequencerParams;
struct timespec timeNow;
sem_t appCompleteSem;
timer_t seqTimer;

/*------------------------------------------------------------------------*/
/*** METHODS ***/
/*------------------------------------------------------------------------*/
/*
 * Signal Handler method for app the shutdown after the desired number of frames has been received
 * @param sig - received signal
 */
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
/*
 * Sequencer to trigger updates to semaphores of other service threads at the 
 * specified rates based on a sub-rate of the base-rate.
 * 
 * @param signal - Signal type received
 */
void sequencer(int signal) {
/* Used for capture sequencer jitter - comment out for data processing
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
  if((sequenceCount % (int)PROC_WRITE_FRAMES_MOD_CALC) == 0) {
    sem_post(sequencerParams.pProcSema);
    sem_post(sequencerParams.pWriteSema);
  }


/* Used for capture sequencer jitter - comment out for data processing
#if defined(TIMESTAMP_SYSLOG_OUTPUT)
  clock_gettime(SYSLOG_CLOCK_TYPE, &timeNow);
  syslog(LOG_INFO, "%s cycle done (msec):, %.2f", __func__, TIMESPEC_TO_MSEC(timeNow));
#endif
*/
}

/*------------------------------------------------------------------------*/
/*
 * Service started from main() to setup all necessary data types (semaphores),
 * register the appShutdown signal handler, and initiate the timer which drives
 * the sequencer base-rate task at the desired frequency. App blocks on the 
 * appCompleteSemaphore until a signal is received from the writeFrame service 
 * to indicate the max number of frames have been written to memory. 
 * 
 * @arg - void pointer for pthread arguments passed from main().
 */
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