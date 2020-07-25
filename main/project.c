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
#include <opencv2/imgproc.hpp>  // Gaussian Blur
#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>  // OpenCV window I/O

#include <iostream> // for standard I/O
#include <string>   // for strings
#include <iomanip>  // for controlling float print precision
#include <sstream>  // string to number conversion

using namespace cv;
using namespace std;

/* project headers */
#include "frameAcquisition.h"
#include "frameDifference.h"
#include "frameProcessing.h"
#include "frameWrite.h"
#include "sequencer.h"
#include "project.h"

/*---------------------------------------------------------------------------------*/
/* MACROS / TYPES / CONST */
#define NUM_THREADS         	        (2)

#define ERROR                         (-1)
#define READ_THEAD_NUM 			          (0)
#define PROC_THEAD_NUM 			          (READ_THEAD_NUM + 1)

/*---------------------------------------------------------------------------------*/
/* PRIVATE FUNCTIONS */

int set_attr_policy(pthread_attr_t *attr, int policy, uint8_t priorityOffset);
int set_main_policy(int policy, uint8_t priorityOffset);
void print_scheduler(void);

/*---------------------------------------------------------------------------------*/
/* GLOBAL VARIABLES */
int gAbortTest = 0;
const char *msgQueueName = "/image_mq";

/*---------------------------------------------------------------------------------*/

int main(int argc, char *argv[])
{
  /* starting logging; use cat /var/log/syslog | grep prob4
   * to view messages */
  openlog("prob5", LOG_PID | LOG_NDELAY | LOG_CONS, LOG_USER);
  syslog(LOG_INFO, ".");
  syslog(LOG_INFO, "..");
  syslog(LOG_INFO, "...");
  syslog(LOG_INFO, "logging started");

  if (argc < 3) {
    syslog(LOG_ERR, "incorrect number of arguments provided");
    cout  << "incorrect number of arguments provided\n\n"
          << "Usage: prob5 [decimation factor] [filter type]\n"
          << "decimation factor[0 = original size, 1 = half size, 2 = quarter size]\n"
          << "filter type [0 = gaussionBlur(), 1 = filter2D(), 2 = sepFilter2D()\n";
    return -1;
  }
  
  /*---------------------------------------*/
  /* setup common message queue */
  /*---------------------------------------*/

  /* ensure MQs properly cleaned up before starting */
  mq_unlink(msgQueueName);
  if(remove(msgQueueName) == -1 && errno != ENOENT) {
    syslog(LOG_ERR, "couldn't clean queue");
    return -1;
  }
	  
  struct mq_attr mq_attr;
  memset(&mq_attr, 0, sizeof(struct mq_attr));
  mq_attr.mq_maxmsg = 10;
  mq_attr.mq_msgsize = MAX_MSG_SIZE;
  mq_attr.mq_flags = 0;

  /* create queue here to allow main to do clean up */
  mqd_t mymq = mq_open(msgQueueName, O_CREAT, S_IRWXU, &mq_attr);
  if(mymq == (mqd_t)ERROR) {
    syslog(LOG_ERR, "couldn't create queue");
    return -1;
  }

  /*----------------------------------------------*/
  /* set scheduling policy of main and threads */
  /*----------------------------------------------*/
  print_scheduler();
  set_main_policy(SCHED_FIFO, 0);
  print_scheduler();
  pthread_attr_t thread_attr;
  set_attr_policy(&thread_attr, SCHED_FIFO, 0);

  /*---------------------------------------*/
  /* create threads */
  /*---------------------------------------*/
  pthread_t threads[NUM_THREADS];
  threadParams_t threadParams;
  strcpy(threadParams.msgQueueName, msgQueueName);
  if((threadParams.decimateFactor = atoi(argv[1])) > 2) {
    syslog(LOG_ERR, "invalid decimation factor provided");
    cout  << "invalid decimation factor provided\n\n"
          << "Usage: prob5 [decimation factor] [filter type]\n"
          << "decimation factor[0 = original size, 1 = half size, 2 = quarter size]\n"
          << "filter type [0 = gaussionBlur(), 1 = filter2D(), 2 = sepFilter2D()\n";
    return -1;
  } else {
    threadParams.decimateFactor = pow(2.0, threadParams.decimateFactor);
  }
  if((threadParams.filterMethod = (FilterType_e)atoi(argv[2])) > 2) {
    syslog(LOG_ERR, "invalid filter type provided");
    cout  << "invalid filter type provided\n\n"
          << "Usage: prob5 [decimation factor] [filter type]\n"
          << "decimation factor[0 = original size, 1 = half size, 2 = quarter size]\n"
          << "filter type [0 = gaussionBlur(), 1 = filter2D(), 2 = sepFilter2D()\n";
    return -1;
  }

  syslog(LOG_INFO, "decimation factor: %d", threadParams.decimateFactor);
  syslog(LOG_INFO, "filter type: %d", threadParams.filterMethod);

  threadParams.cameraIdx = 0;
  threadParams.threadIdx = READ_THEAD_NUM;
  if(pthread_create(&threads[READ_THEAD_NUM], &thread_attr, acquisitionTask, (void *)&threadParams) != 0) {
    syslog(LOG_ERR, "couldn't create thread#%d", READ_THEAD_NUM);
  }

  threadParams.threadIdx = PROC_THEAD_NUM;
  if(pthread_create(&threads[PROC_THEAD_NUM], &thread_attr, processingTask, (void *)&threadParams) != 0) {
    syslog(LOG_ERR, "couldn't create thread#%d", PROC_THEAD_NUM);
  }

  /*----------------------------------------------*/
  /* exiting */
  /*----------------------------------------------*/
  syslog(LOG_INFO, "%s waiting on threads...", __func__);
  for(uint8_t ind = 0; ind < NUM_THREADS; ++ind) {
    pthread_join(threads[ind], NULL);
  }
  syslog(LOG_INFO, "%s exiting, stopping log", __func__);
  syslog(LOG_INFO, "...");
  syslog(LOG_INFO, "..");
  syslog(LOG_INFO, ".");
  closelog();
  mq_unlink(msgQueueName);
  mq_close(mymq);
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

int set_attr_policy(pthread_attr_t *attr, int policy, uint8_t priorityOffset)
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