/***********************************************************************************
 * @author Joshua Malburg
 * joshua.malburg@colorado.edu
 * 
 * Real-time Embedded Systems
 * ECEN5623 - Sam Siewert
 * @date 25Jul2020
 * Ubuntu 18.04 LTS and RPi 3B+
 ************************************************************************************
 *
 * @file frameDifference.c
 * @brief 
 *
 ************************************************************************************
 */

/*---------------------------------------------------------------------------------*/
/* INCLUDES */
#include <stdio.h>
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
#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>  // OpenCV window I/O

#include <iostream> // for standard I/O
#include <string>   // for strings
#include <iomanip>  // for controlling float print precision
#include <sstream>  // string to number conversion

using namespace cv;
using namespace std;

/* project headers */
#include "project.h"

/*---------------------------------------------------------------------------------*/
/* MACROS / TYPES / CONST */

/*---------------------------------------------------------------------------------*/
/* PRIVATE FUNCTIONS */

/*---------------------------------------------------------------------------------*/
/* GLOBAL VARIABLES */

/*---------------------------------------------------------------------------------*/
void *differenceTask(void *arg)
{
  unsigned int cnt = 0;
  unsigned int prio = 30;

  /* get thread parameters */
  if(arg == NULL) {
    syslog(LOG_ERR, "invalid arg provided to %s", __func__);
    return NULL;
  }
  threadParams_t threadParams = *(threadParams_t *)arg;

  if(threadParams.pBuffMutex == NULL) {
    syslog(LOG_ERR, "invalid mutex provided to %s", __func__);
    return NULL;
  }

  /* open handle to queue */
  mqd_t selectQueue = mq_open(threadParams.selectQueueName,O_WRONLY, 0666, NULL);
  if(selectQueue == -1) {
    syslog(LOG_ERR, "%s couldn't open queue", __func__);
    cout << __func__<< " couldn't open queue" << endl;
    return NULL;
  }

  
  struct timespec startTime, sendTime, expireTime;
  clock_gettime(CLOCK_MONOTONIC, &startTime);
  syslog(LOG_INFO, "%s (id = %d) started at %f", __func__, threadParams.threadIdx,  TIMESPEC_TO_MSEC(startTime));
  Mat img = Mat::zeros(Size(MAX_IMG_COLS, MAX_IMG_ROWS), CV_8UC3);
	while(1) {
    /*todo: cycle through circular buffer */

    /*todo: find difference */

    /* try to insert image but don't block if full
     * so that we loop around and just get the newest */
    clock_gettime(CLOCK_MONOTONIC, &expireTime);
    if(mq_timedsend(selectQueue, (const char *)&img, SELECT_QUEUE_MSG_SIZE, prio, &expireTime) != 0) {
      /* don't print if queue was empty */
      if(errno != ETIMEDOUT) {
        syslog(LOG_ERR, "%s error with mq_send, errno: %d [%s]", __func__, errno, strerror(errno));
      }
      cout << __func__ << " error with mq_send, errno: " << errno << " [" << strerror(errno) << "]" << endl;
    } else {
      clock_gettime(CLOCK_MONOTONIC, &sendTime);
      syslog(LOG_INFO, "%s sent image#%d at: %f", __func__, cnt, TIMESPEC_TO_MSEC(sendTime));
      ++cnt;
    }

		sleep(1);
	}
  mq_close(selectQueue);
  clock_gettime(CLOCK_MONOTONIC, &startTime);
  syslog(LOG_INFO, "%s (id = %d) exiting at: %f", __func__, threadParams.threadIdx,  TIMESPEC_TO_MSEC(startTime));
  return NULL;
}