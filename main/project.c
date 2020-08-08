/***********************************************************************************
 * @author Joshua Malburg and Brian Ebeling
 * joshua.malburg@colorado.edu and ibelingb@colorado.edyy
 * 
 * Real-time Embedded Systems
 * ECEN5623 - Sam Siewert
 * @date 25Jul2020
 * Ubuntu 18.04 LTS and RPi 3B+
 ************************************************************************************
 *
 * @file project.c
 * @brief ECEN5623 final project
 *
 ************************************************************************************
 */

/*---------------------------------------------------------------------------------*/
/* INCLUDES */
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <mqueue.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <syslog.h>

/* opencv headers */
#include <opencv2/core.hpp>     // Basic OpenCV structures (cv::Mat, Scalar)

#include <iostream>             // for standard I/O
#include <string>               // for strings
#include <iomanip>              // for controlling float print precision
#include <sstream>              // string to number conversion
#include <vector>

using namespace cv;
using namespace std;

/* project headers */
#include "frameAcquisition.h"
#include "frameDifference.h"
#include "frameProcessing.h"
#include "frameWrite.h"
#include "sequencer.h"
#include "project.h"
#include "circular_buffer.h"
#include "circular_cv_buffer.h"

/*---------------------------------------------------------------------------------*/
/* MACROS / TYPES / CONST */
#define ERROR   (-1)
#define NUM_CPU_CORES (4)

/*---------------------------------------------------------------------------------*/
/* PRIVATE FUNCTIONS */

int set_attr_policy(pthread_attr_t *attr, cpu_set_t *cpuSet, int policy, uint8_t priorityOffset, int cpuCore);
int set_main_policy(int policy, uint8_t priorityOffset);
void print_scheduler(void);
void usage(void);

/*---------------------------------------------------------------------------------*/
/* GLOBAL VARIABLES */
int gAbortTest = 0;
const char *selectQueueName = "/image_mq";
const char *writeQueueName = "/write_mq";

/*---------------------------------------------------------------------------------*/

int main(int argc, char *argv[])
{
  pthread_t threads[TOTAL_THREADS];
  int i;

  /* starting logging; use cat /var/log/syslog | grep project
   * to view messages */
  openlog("project", LOG_PID | LOG_NDELAY | LOG_CONS, LOG_USER);
  syslog(LOG_INFO, ".");
  syslog(LOG_INFO, "..");
  syslog(LOG_INFO, "...");
  struct timespec startTime;
  clock_gettime(SYSLOG_CLOCK_TYPE, &startTime);
  syslog(LOG_INFO, "%s (tid = %lu) started at %f", __func__, pthread_self(),  TIMESPEC_TO_MSEC(startTime));

  if (argc < 4) {
    syslog(LOG_ERR, "incorrect number of arguments provided");
    cout  << "invalid parameter provided provided\n\n";
    usage();
    return -1;
  }

  /*---------------------------------------*/
  /* parse CLI */
  /*---------------------------------------*/
  threadParams_t threadParams[TOTAL_THREADS - 1];
  seqThreadParams_t seqThreadParams;
  for(int ind = 0; ind < TOTAL_THREADS - 1; ++ind) {
    memset(&threadParams[ind], 0, sizeof(threadParams_t));
    threadParams[ind].programStartTime.tv_nsec = startTime.tv_nsec;
    threadParams[ind].programStartTime.tv_sec = startTime.tv_sec;
  }
  

  /* hough_enable */
  int argIndex = 1;
  if((strcmp(argv[argIndex], "on") == 0) || (strcmp(argv[argIndex], "ON") == 0) || (strcmp(argv[argIndex], "On") == 0) || (strcmp(argv[argIndex], "oN") == 0)) {
    threadParams[Thread_e::PROC_THREAD].hough_enable = 1;
  } else if((strcmp(argv[argIndex], "off") == 0) || (strcmp(argv[argIndex], "OFF") == 0) || (strcmp(argv[argIndex], "Off") == 0) || (strcmp(argv[argIndex], "oFF") == 0) ||
            (strcmp(argv[argIndex], "oFf") == 0) || (strcmp(argv[argIndex], "OfF") == 0) || (strcmp(argv[argIndex], "OFf") == 0) || (strcmp(argv[argIndex], "ofF") == 0)) {
    threadParams[Thread_e::PROC_THREAD].hough_enable = 0;
  } else {
    syslog(LOG_ERR, "invalid hough_enable provided");
    cout  << "invalid 'filter_enable' parameter provided\n\n";
    usage();
    return -1;
  }
  /* filter_enable */
  ++argIndex;
  if((strcmp(argv[argIndex], "on") == 0) || (strcmp(argv[argIndex], "ON") == 0) || (strcmp(argv[argIndex], "On") == 0) || (strcmp(argv[argIndex], "oN") == 0)) {
    threadParams[Thread_e::PROC_THREAD].filter_enable = 1;
  } else if((strcmp(argv[argIndex], "off") == 0) || (strcmp(argv[argIndex], "OFF") == 0) || (strcmp(argv[argIndex], "Off") == 0) || (strcmp(argv[argIndex], "oFF") == 0) ||
            (strcmp(argv[argIndex], "oFf") == 0) || (strcmp(argv[argIndex], "OfF") == 0) || (strcmp(argv[argIndex], "OFf") == 0) || (strcmp(argv[argIndex], "ofF") == 0)) {
    threadParams[Thread_e::PROC_THREAD].filter_enable = 0;
  } else {
    syslog(LOG_ERR, "invalid filter_enable provided");
    cout  << "invalid 'filter_enable' parameter provided\n\n";
    usage();
    return -1;
  }

  /* save type */
  ++argIndex;
  threadParams[Thread_e::DIFF_THREAD].save_type = (SaveType_e)(atoi(argv[argIndex]) % SaveType_e::SAVE_TYPE_END);
  threadParams[Thread_e::PROC_THREAD].save_type = (SaveType_e)(atoi(argv[argIndex]) % SaveType_e::SAVE_TYPE_END);
  threadParams[Thread_e::WRITE_THREAD].save_type = (SaveType_e)(atoi(argv[argIndex]) % SaveType_e::SAVE_TYPE_END);
  
  /* todo: get from CLI */
  threadParams[Thread_e::ACQ_THREAD].cameraIdx = 0;

  syslog(LOG_INFO, "hough_enable: %d", threadParams[Thread_e::PROC_THREAD].hough_enable);
  syslog(LOG_INFO, "filter_enable: %d", threadParams[Thread_e::PROC_THREAD].filter_enable);
  syslog(LOG_INFO, "save_type: %d",  threadParams[Thread_e::DIFF_THREAD].save_type);
  syslog(LOG_INFO, "cam_index: %d", threadParams[Thread_e::ACQ_THREAD].cameraIdx);
  
  /*---------------------------------------*/
  /* setup select message queue */
  /*---------------------------------------*/

  /* ensure MQs properly cleaned up before starting */
  mq_unlink(selectQueueName);
  if(remove(selectQueueName) == -1 && errno != ENOENT) {
    syslog(LOG_ERR, "couldn't clean queue");
    return -1;
  }
	  
  struct mq_attr mq_select_attr;
  memset(&mq_select_attr, 0, sizeof(struct mq_attr));
  mq_select_attr.mq_maxmsg = SELECT_QUEUE_LENGTH;
  mq_select_attr.mq_msgsize = SELECT_QUEUE_MSG_SIZE;
  mq_select_attr.mq_flags = 0;

  /* this queue is setup as non-blocking because its used by RT threads */
  mqd_t selectQueue = mq_open(selectQueueName, O_CREAT | O_NONBLOCK, S_IRWXU, &mq_select_attr);
  if(selectQueue == (mqd_t)ERROR) {
    syslog(LOG_ERR, "couldn't create queue");
    return -1;
  }

  /*---------------------------------------*/
  /* setup write message queue */
  /*---------------------------------------*/

  /* ensure MQs properly cleaned up before starting */
  mq_unlink(writeQueueName);
  if(remove(writeQueueName) == -1 && errno != ENOENT) {
    syslog(LOG_ERR, "couldn't clean queue");
    return -1;
  }
	  
  struct mq_attr mq_write_attr;
  memset(&mq_write_attr, 0, sizeof(struct mq_attr));
  mq_write_attr.mq_maxmsg = WRITE_QUEUE_LENGTH;
  mq_write_attr.mq_msgsize = WRITE_QUEUE_MSG_SIZE;
  mq_write_attr.mq_flags = 0;

  /* allow write queue to block so writeTask just waits on messages */
  mqd_t writeQueue = mq_open(writeQueueName, O_CREAT, S_IRWXU, &mq_write_attr);
  if(writeQueue == (mqd_t)ERROR) {
    syslog(LOG_ERR, "couldn't create queue");
    return -1;
  }
  /*---------------------------------------*/
  /* create circular buffer */
  /*---------------------------------------*/
  circular_buffer<cv::Mat> imgBuff(CIRCULAR_BUFF_LEN);
  circular_cv_buffer imgBuff2(CIRCULAR_BUFF_LEN);
  threadParams[Thread_e::DIFF_THREAD].pCBuff = &imgBuff;
  threadParams[Thread_e::ACQ_THREAD].pCBuff = &imgBuff;
  threadParams[Thread_e::DIFF_THREAD].pCBuffcv = &imgBuff2;
  threadParams[Thread_e::ACQ_THREAD].pCBuffcv = &imgBuff2;

  /*---------------------------------------*/
  /* create synchronization mechanizisms */
  /*---------------------------------------*/
  sem_t semas[TOTAL_RT_THREADS];
  for(uint8_t ind = 0; ind < TOTAL_RT_THREADS; ++ind) {
    if (sem_init(&semas[ind], 0, 0)) {
      syslog(LOG_ERR, "couldn't create semaphore");
      return -1;
    }

    /* Set sequencer threadid */
    threadParams[Thread_e::WRITE_THREAD].pTidSeqThread = &threads[Thread_e::SEQ_THREAD];
  }

  pthread_mutex_t cb_mutex;
  pthread_mutex_init(&cb_mutex, NULL);
  threadParams[Thread_e::DIFF_THREAD].pMutex = &cb_mutex;
  threadParams[Thread_e::ACQ_THREAD].pMutex = &cb_mutex;

  /*----------------------------------------------*/
  /* set scheduling policy of main and threads */
  /*----------------------------------------------*/
  print_scheduler();
  set_main_policy(SCHED_FIFO, 0);
  print_scheduler();
  pthread_attr_t thread_attr;

  /*---------------------------------------*/
  /* Setup CPU Affinity for threads */
  /*---------------------------------------*/
  cpu_set_t threadCpu;

  /* Clear CPUs set */
  CPU_ZERO(&threadCpu);
  for (i = 0; i < NUM_CPU_CORES; i++) {
    CPU_SET(i, &threadCpu);
  }

  /*---------------------------------------*/
  /* create threads */
  /*---------------------------------------*/
  strcpy(threadParams[Thread_e::DIFF_THREAD].selectQueueName, selectQueueName);
  strcpy(threadParams[Thread_e::PROC_THREAD].selectQueueName, selectQueueName);
  strcpy(threadParams[Thread_e::PROC_THREAD].writeQueueName, writeQueueName);
  strcpy(threadParams[Thread_e::WRITE_THREAD].writeQueueName, writeQueueName);

  set_attr_policy(&thread_attr, &threadCpu, SCHED_FIFO, 2, 3);
  threadParams[Thread_e::ACQ_THREAD].pSema = &semas[Thread_e::ACQ_THREAD];
  if(pthread_create(&threads[Thread_e::ACQ_THREAD], &thread_attr, acquisitionTask, (void *)&threadParams[Thread_e::ACQ_THREAD]) != 0) {
    syslog(LOG_ERR, "couldn't create thread#%d", Thread_e::ACQ_THREAD);
  }

  set_attr_policy(&thread_attr, &threadCpu, SCHED_FIFO, 3, 2);
  threadParams[Thread_e::DIFF_THREAD].pSema = &semas[Thread_e::DIFF_THREAD];
  if(pthread_create(&threads[Thread_e::DIFF_THREAD], &thread_attr, differenceTask, (void *)&threadParams[Thread_e::DIFF_THREAD]) != 0) {
    syslog(LOG_ERR, "couldn't create thread#%d", Thread_e::DIFF_THREAD);
  }

  set_attr_policy(&thread_attr, &threadCpu, SCHED_FIFO, 4, 2);
  threadParams[Thread_e::PROC_THREAD].pSema = &semas[Thread_e::PROC_THREAD];
  if(pthread_create(&threads[Thread_e::PROC_THREAD], &thread_attr, processingTask, (void *)&threadParams[Thread_e::PROC_THREAD]) != 0) {
    syslog(LOG_ERR, "couldn't create thread#%d", Thread_e::PROC_THREAD);
  }

  set_attr_policy(&thread_attr, &threadCpu, SCHED_RR, 5, 0);
  threadParams[Thread_e::WRITE_THREAD].pSema = &semas[Thread_e::WRITE_THREAD];
  if(pthread_create(&threads[Thread_e::WRITE_THREAD], &thread_attr, writeTask, (void *)&threadParams[Thread_e::WRITE_THREAD]) != 0) {
    syslog(LOG_ERR, "couldn't create thread#%d", Thread_e::WRITE_THREAD);
  }

  set_attr_policy(&thread_attr, &threadCpu, SCHED_FIFO, 1, 4);
  seqThreadParams.pAcqSema       = &semas[Thread_e::ACQ_THREAD];
  seqThreadParams.pDiffSema      = &semas[Thread_e::DIFF_THREAD];
  seqThreadParams.pProcSema      = &semas[Thread_e::PROC_THREAD];
  seqThreadParams.pWriteSema     = &semas[Thread_e::WRITE_THREAD];
  seqThreadParams.tidAcqThread   = threads[Thread_e::ACQ_THREAD];
  seqThreadParams.tidDiffThread  = threads[Thread_e::DIFF_THREAD];
  seqThreadParams.tidProcThread  = threads[Thread_e::PROC_THREAD];
  seqThreadParams.tidWriteThread = threads[Thread_e::WRITE_THREAD];
  if(pthread_create(&threads[SEQ_THREAD], &thread_attr, sequencerTask, (void *)&seqThreadParams) != 0) {
    syslog(LOG_ERR, "couldn't create thread#%d", Thread_e::SEQ_THREAD);
  }

  /*----------------------------------------------*/
  /* exiting */
  /*----------------------------------------------*/
  syslog(LOG_INFO, "%s waiting on threads...", __func__);
  for(uint8_t ind = 0; ind < TOTAL_THREADS; ++ind) {
    pthread_join(threads[ind], NULL);
  }
syslog(LOG_INFO, "%s (tid = %lu) exiting at: %f", __func__, pthread_self(),  TIMESPEC_TO_MSEC(startTime));
  syslog(LOG_INFO, "...");
  syslog(LOG_INFO, "..");
  syslog(LOG_INFO, ".");
  closelog();

  for(uint8_t ind = 0; ind < TOTAL_RT_THREADS; ++ind) {
      sem_destroy(&semas[ind]);
  }
  pthread_attr_destroy(&thread_attr);
  mq_unlink(selectQueueName);
  mq_unlink(writeQueueName);
  mq_close(selectQueue);
  mq_close(writeQueue);
}

void usage(void) 
{
  cout  << "Usage: sudo ./project [hough_enable] [filter_enable] [save_type]\n"
        << "sudo ./project on on 0\n"
        << "sudo ./project off off 1\n";
}

void print_scheduler(void)
{
  switch (sched_getscheduler(getpid()))
  {
  case SCHED_FIFO:
    printf("Pthread Policy is SCHED_FIFO\n");
    syslog(LOG_INFO, "Pthread Policy is SCHED_FIFO");
    break;
  case SCHED_OTHER:
    printf("Pthread Policy is SCHED_OTHER\n");
    syslog(LOG_INFO, "Pthread Policy is SCHED_OTHER");
    break;
  case SCHED_RR:
    printf("Pthread Policy is SCHED_RR\n");
    syslog(LOG_INFO, "Pthread Policy is SCHED_RR");
    break;
  default:
    printf("Pthread Policy is UNKNOWN\n");
    syslog(LOG_INFO, "Pthread Policy is UNKNOWN");
  }
}

int set_attr_policy(pthread_attr_t *attr, cpu_set_t *cpuSet, int policy, uint8_t priorityOffset, int cpuCore)
{
  struct sched_param param;
  int rtnCode = 0;

  if(policy < 0) {
    printf("ERROR: invalid policy #: %d\n", policy);
    perror("setSchedPolicy");
    // SCHED_OTHER     --> 0
    // SCHED_FIFO      --> 1
    // SCHED_RR        --> 2
    // SCHED_BATCH     --> 3
    // SCHED_ISO       --> 4
    // SCHED_IDLE      --> 5
    // SCHED_DEADLINE  --> 6
    return -1;
  }
  else if(attr == NULL) {
    return -1;
  }

  /* set attribute structure */
  rtnCode |= pthread_attr_init(attr);
  rtnCode |= pthread_attr_setinheritsched(attr, PTHREAD_EXPLICIT_SCHED);
  rtnCode |= pthread_attr_setschedpolicy(attr, policy);

  param.sched_priority = sched_get_priority_max(policy) - priorityOffset;

  if(cpuCore > 0) {
    CPU_SET(cpuCore, cpuSet);
    rtnCode |= pthread_attr_setaffinity_np(attr, sizeof(cpu_set_t), cpuSet);
  }

  rtnCode |= pthread_attr_setschedparam(attr, &param);
  if (rtnCode) {
    printf("ERROR: set_attr_policy, errno: %s\n", strerror(errno));
    return -1;
  }
  return 0;
}

int set_main_policy(int policy, uint8_t priorityOffset)
{
  int rtnCode;
  struct sched_param param;

  if(policy < 0) {
    printf("ERROR: invalid policy #: %d\n", policy);
    perror("setSchedPolicy");
    return -1;
  }

  /* this sets the policy/priority for our process */
  rtnCode = sched_getparam(getpid(), &param);
  if (rtnCode) {
    printf("ERROR: sched_getparam (in set_main_policy)  rc is %d, errno: %s\n", rtnCode, strerror(errno));
    return -1;
  }

  /* update scheduler */
  param.sched_priority = sched_get_priority_max(policy) - priorityOffset;
  rtnCode = sched_setscheduler(getpid(), policy, &param);
  if (rtnCode) {
    printf("ERROR: sched_setscheduler (in set_main_policy) rc is %d, errno: %s\n", rtnCode, strerror(errno));
    return -1;
  }
  return 0;
}