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
#define FILTER_SIZE   (15)
#define FILTER_SIGMA  (2.0)

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
  if(threadParams.pMutex == NULL) {
    syslog(LOG_ERR, "invalid MUTEX provided to %s", __func__);
    return NULL;
  }

  /* open handle to queue */
  mqd_t selectQueue = mq_open(threadParams.selectQueueName,O_WRONLY, 0666, NULL);
  if(selectQueue == -1) {
    syslog(LOG_ERR, "%s couldn't open queue", __func__);
    cout << __func__<< " couldn't open queue" << endl;
    return NULL;
  }

  /* create filter kernel */
  Mat kern1D = getGaussianKernel(FILTER_SIZE, FILTER_SIGMA, CV_32F);
  
  struct timespec timeNow, sendTime;
  #if defined(DT_SYSLOG_OUTPUT)
  struct timespec prevSendTime;
  #endif

  clock_gettime(SYSLOG_CLOCK_TYPE, &timeNow);
  syslog(LOG_INFO, "%s (tid = %lu) started at %f", __func__, pthread_self(),  TIMESPEC_TO_MSEC(timeNow));
  Mat prevFrame;
  Mat blank = Mat::zeros(Size(MAX_IMG_COLS, MAX_IMG_ROWS), CV_8UC1);
  Mat newTimeFrame;
  unsigned int timeoutCnt = 0;
  uint8_t skipNextCnt = 0;
	while(1) {
    /* wait for semaphore */
    clock_gettime(SEMA_CLOCK_TYPE, &timeNow);
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
      pthread_mutex_lock(threadParams.pMutex);
      prevFrame = threadParams.pCBuff->get();
      pthread_mutex_unlock(threadParams.pMutex);
      if(prevFrame.empty()) {
        cout << "ERROR: prevFrame empty still!" << endl;
        continue;
      }
      cvtColor(prevFrame, prevFrame, COLOR_RGB2GRAY);
    }

    /* continue as long as there's frames in buffer */
    while(!threadParams.pCBuff->empty())
    {
#if defined(TIMESTAMP_SYSLOG_OUTPUT)
      clock_gettime(SYSLOG_CLOCK_TYPE, &timeNow);
      syslog(LOG_INFO, "%s frame process start:, %.2f, ms", __func__, TIMESPEC_TO_MSEC(timeNow));
#endif
      pthread_mutex_lock(threadParams.pMutex);
      Mat readFrame = threadParams.pCBuff->get();
      pthread_mutex_unlock(threadParams.pMutex);
      if(readFrame.empty()) {
        cout << "ERROR: nextFrame empty still!" << endl;
        continue;
      }

      Mat nextFrame;
      cvtColor(readFrame, nextFrame, COLOR_RGB2GRAY);

      /* find difference */
      Mat diffFrame = nextFrame - prevFrame;
  
      /* convert to binary */
      Mat bw;
      threshold(diffFrame, bw, 20, 255, THRESH_BINARY);

      /* if a difference was found, take the next
       * frame to ensure the hands are stationary */
      clock_gettime(SYSLOG_CLOCK_TYPE, &timeNow);
      if(countNonZero(bw) > 100) {
        skipNextCnt = 5;

        while(skipNextCnt != 0) {
          if(threadParams.pCBuff->empty()) {
            cout << "not enough frames in CB to fulfill skip request, using last in CB" << endl;
            break;
          } else {
            // cout << " skip# " << (int)skipNextCnt << endl;
            // char filename[80];
            // sprintf(filename, "./Diff_acquiredFrame%d_skip#%d.ppm", cnt, skipNextCnt);
            // imwrite(filename, readFrame);
            pthread_mutex_lock(threadParams.pMutex);
            readFrame = threadParams.pCBuff->get();
            pthread_mutex_unlock(threadParams.pMutex);
            cvtColor(readFrame, nextFrame, COLOR_RGB2GRAY);
            --skipNextCnt;
          }
        }

        if(threadParams.save_type == SaveType_e::SAVE_COLOR_IMAGE) {
          readFrame.copyTo(newTimeFrame);
        } else if (threadParams.save_type == SaveType_e::SAVE_DIFF_IMAGE) {
          diffFrame.copyTo(newTimeFrame);
        } else if (threadParams.save_type == SaveType_e::SAVE_THRES_IMAGE) {
          bw.copyTo(newTimeFrame);
        } else {
          nextFrame.copyTo(newTimeFrame);
        }
        clock_gettime(SYSLOG_CLOCK_TYPE, &timeNow);
        int len = newTimeFrame.rows * newTimeFrame.cols * newTimeFrame.elemSize();
        uint8_t *pixelData = (uint8_t *)malloc(len);
        memcpy(pixelData, newTimeFrame.data, len);

        /* this is a really ugly way to do this but I didn't 
         * know how to get around the ref count / auto-memory
         * managment of C++; thought about using STL container instead
         * but MQs is what we learned in class */
        imgDef_t dummy = {  .data = pixelData, 
                            .type = newTimeFrame.type(), 
                            .rows = newTimeFrame.rows, 
                            .cols = newTimeFrame.cols, 
                            .elem_size = newTimeFrame.elemSize(),
                            .diffFrameNum = cnt,
                            .diffFrameTime = CALC_DT_MSEC(timeNow, threadParams.programStartTime),
                            .isColor = (threadParams.save_type == SaveType_e::SAVE_COLOR_IMAGE)};

        /* try to insert image but don't block if full
        * so that we loop around and just get the newest */
        clock_gettime(SEMA_CLOCK_TYPE, &timeNow);
        if(mq_timedsend(selectQueue, (char *)&dummy, SELECT_QUEUE_MSG_SIZE, prio, &timeNow) != 0) {
            if(errno == ETIMEDOUT) {
              cout << __func__ << " mq_timedsend(writeQueue, ...) TIMEOUT#" << timeoutCnt++ << endl;
            }
            free(dummy.data);
            syslog(LOG_ERR, "%s error with mq_timedsend, errno: %d [%s]", __func__, errno, strerror(errno));
        } else {

          clock_gettime(SYSLOG_CLOCK_TYPE, &sendTime);
#if defined(TIMESTAMP_SYSLOG_OUTPUT)
          syslog(LOG_INFO, "%s frame #%d inserted to selectQueue at:, %.2f, ms", __func__, cnt, TIMESPEC_TO_MSEC(sendTime));
#endif
#if defined(DT_SYSLOG_OUTPUT)
          syslog(LOG_INFO, "%s inserted frame#%d to selectQueue, dt since start: %.2f ms, dt since last frame sent: %.2f ms", __func__, cnt,
                 CALC_DT_MSEC(sendTime, threadParams.programStartTime), CALC_DT_MSEC(sendTime, prevSendTime));
          prevSendTime.tv_sec = sendTime.tv_sec;
          prevSendTime.tv_nsec = sendTime.tv_nsec;
#endif
          ++cnt;
        }
      }
      /* store old frame */
      nextFrame.copyTo(prevFrame);
    }
	}
  mq_close(selectQueue);
  clock_gettime(SYSLOG_CLOCK_TYPE, &timeNow);
  syslog(LOG_INFO, "%s (tid = %lu) exiting at: %f", __func__, pthread_self(),  TIMESPEC_TO_MSEC(timeNow));
  return NULL;
}