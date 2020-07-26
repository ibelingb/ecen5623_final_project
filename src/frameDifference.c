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
#include <opencv2/imgproc.hpp>
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

  if(threadParams.pCBuff == NULL) {
    syslog(LOG_ERR, "invalid circular buffer provided to %s", __func__);
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
  syslog(LOG_INFO, "%s (tid = %lu) started at %f", __func__, pthread_self(),  TIMESPEC_TO_MSEC(startTime));
  Mat prevFrame;
  Mat blank = Mat::zeros(Size(MAX_IMG_COLS, MAX_IMG_ROWS), CV_8UC1);
	while(1) {

    /* if this is the first time through, fill previous frame */
    if(prevFrame.empty() && threadParams.pCBuff->capacity() > 0) {
      prevFrame = threadParams.pCBuff->get();
      cvtColor(prevFrame, prevFrame, COLOR_RGB2GRAY);
    }

    /* continue as long as there's frames in buffer */
    while(threadParams.pCBuff->capacity() > 0)
    {
      Mat nextFrame = threadParams.pCBuff->get();
      cvtColor(nextFrame, nextFrame, COLOR_RGB2GRAY);

      /* find difference */
      Mat diffFrame = nextFrame - prevFrame;
      
      /* convert to binary */
      Mat bw;
      threshold(diffFrame, bw, 50, 255, THRESH_BINARY);

      /* if difference detected, send for processing */
      if(countNonZero(bw) > 10) {
        imshow("diff", diffFrame);
        waitKey();
        /* try to insert image but don't block if full
        * so that we loop around and just get the newest */
        clock_gettime(CLOCK_MONOTONIC, &expireTime);
        if(mq_timedsend(selectQueue, (const char *)&nextFrame, SELECT_QUEUE_MSG_SIZE, prio, &expireTime) != 0) {
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
      }

      /* store old frame */
      nextFrame.copyTo(prevFrame);
    }
		sleep(1);
	}
  mq_close(selectQueue);
  clock_gettime(CLOCK_MONOTONIC, &startTime);
  syslog(LOG_INFO, "%s (tid = %lu) exiting at: %f", __func__, pthread_self(),  TIMESPEC_TO_MSEC(startTime));
  return NULL;
}