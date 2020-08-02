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

  if(threadParams.pSema == NULL) {
    syslog(LOG_ERR, "invalid semaphore provided to %s", __func__);
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
  
  struct timespec timeNow, prevSendTime, sendTime;
  clock_gettime(CLOCK_MONOTONIC, &timeNow);
  syslog(LOG_INFO, "%s (tid = %lu) started at %f", __func__, pthread_self(),  TIMESPEC_TO_MSEC(timeNow));
  Mat prevFrame;
  Mat blank = Mat::zeros(Size(MAX_IMG_COLS, MAX_IMG_ROWS), CV_8UC1);
  unsigned int timeoutCnt = 0;
	while(1) {
    /* wait for semaphore */
    clock_gettime(CLOCK_REALTIME, &timeNow);
    timeNow.tv_nsec += DIFF_THREAD_SEMA_TIMEOUT;
    if(timeNow.tv_nsec > 1e9) {
      timeNow.tv_sec += 1;
      timeNow.tv_nsec -= 1e9;
    }
    if(sem_timedwait(threadParams.pSema, &timeNow) < 0) {
      if(errno != ETIMEDOUT) {
        syslog(LOG_ERR, "%s error with sem_timedwait, errno: %d [%s]", __func__, errno, strerror(errno));
      } else {
        syslog(LOG_ERR, "%s semaphore timed out", __func__);
      }
    }

    /* if this is the first time through, fill previous frame */
    if(prevFrame.empty() && !threadParams.pCBuff->empty()) {
      prevFrame = threadParams.pCBuff->get();
      if(prevFrame.empty()) {
        cout << "ERROR: prevFrame empty still!" << endl;
        continue;
      }
      cvtColor(prevFrame, prevFrame, COLOR_RGB2GRAY);
    }

    /* continue as long as there's frames in buffer */
    while(!threadParams.pCBuff->empty())
    {
      Mat nextFrame = threadParams.pCBuff->get();
      if(nextFrame.empty()) {
        cout << "ERROR: nextFrame empty still!" << endl;
        continue;
      }
      cvtColor(nextFrame, nextFrame, COLOR_RGB2GRAY);

      /* find difference */
      Mat diffFrame = nextFrame - prevFrame;
      
      /* convert to binary */
      Mat bw;
      threshold(diffFrame, bw, 50, 255, THRESH_BINARY);

      /* if difference detected, send for processing */
      if(countNonZero(bw) > 10) {
        clock_gettime(CLOCK_MONOTONIC, &timeNow);
        std::string label = format("Frame time: %.2f ms", CALC_DT_MSEC(timeNow, threadParams.programStartTime));
        putText(nextFrame, label, Point(0, 15), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0, 255, 0));
        int len = nextFrame.rows * nextFrame.cols * nextFrame.elemSize();
        uint8_t *pixelData = (uint8_t *)malloc(len);
        memcpy(pixelData, nextFrame.data, len);

        /* this is a really ugly way to do this but I didn't 
         * know how to get around the ref count / auto-memory
         * managment of C++; thought about using STL container instead
         * but MQs is what we learned in class */
        imgDef_t dummy = {  .data = pixelData, 
                            .type = nextFrame.type(), 
                            .rows = nextFrame.rows, 
                            .cols = nextFrame.cols, 
                            .elem_size = nextFrame.elemSize() };

        /* try to insert image but don't block if full
        * so that we loop around and just get the newest */
        clock_gettime(CLOCK_REALTIME, &timeNow);
        if(mq_timedsend(selectQueue, (char *)&dummy, SELECT_QUEUE_MSG_SIZE, prio, &timeNow) != 0) {
            if(errno == ETIMEDOUT) {
              cout << __func__ << " mq_timedsend(writeQueue, ...) TIMEOUT#" << timeoutCnt++ << endl;
            }
            free(dummy.data);
            syslog(LOG_ERR, "%s error with mq_timedsend, errno: %d [%s]", __func__, errno, strerror(errno));
        } else {
          clock_gettime(CLOCK_MONOTONIC, &sendTime);
          syslog(LOG_INFO, "%s sent/inserted frame#%d to writeQueue, dt since start: %.2f ms, dt since last frame sent: %.2f ms", __func__, cnt,
            CALC_DT_MSEC(sendTime, threadParams.programStartTime), CALC_DT_MSEC(sendTime, prevSendTime));
          ++cnt;
        }
      }
      /* store old frame */
      nextFrame.copyTo(prevFrame);
      prevSendTime.tv_sec = sendTime.tv_sec;
      prevSendTime.tv_nsec = prevSendTime.tv_nsec;
    }
	}
  mq_close(selectQueue);
  clock_gettime(CLOCK_REALTIME, &timeNow);
  syslog(LOG_INFO, "%s (tid = %lu) exiting at: %f", __func__, pthread_self(),  TIMESPEC_TO_MSEC(timeNow));
  return NULL;
}