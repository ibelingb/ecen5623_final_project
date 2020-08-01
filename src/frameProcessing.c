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
 * @file frameProcessing.c
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
#define FILTER_SIZE   (31)
#define FILTER_SIGMA  (2.0)

/*---------------------------------------------------------------------------------*/
/* PRIVATE FUNCTIONS */

/*---------------------------------------------------------------------------------*/
/* GLOBAL VARIABLES */

/*---------------------------------------------------------------------------------*/
void *processingTask(void *arg)
{  
  unsigned int cnt = 0;

  /* get thread parameters */
  if(arg == NULL) {
    syslog(LOG_ERR, "ERROR: invalid arg provided to %s", __func__);
    return NULL;
  }
  threadParams_t threadParams = *(threadParams_t *)arg;

  if(threadParams.pSema == NULL) {
    syslog(LOG_ERR, "invalid semaphore provided to %s", __func__);
    return NULL;
  }

  /* open handle to queue */
  mqd_t selectQueue = mq_open(threadParams.selectQueueName, O_RDONLY, 0666, NULL);
  if(selectQueue == -1) {
    syslog(LOG_ERR, "%s couldn't open queue", __func__);
    cout << __func__<< " couldn't open queue" << endl;
    return NULL;
  }

  /* open handle to queue */
  mqd_t writeQueue = mq_open(threadParams.writeQueueName, O_WRONLY, 0666, NULL);
  if(writeQueue == -1) {
    syslog(LOG_ERR, "%s couldn't open queue", __func__);
    cout << __func__<< " couldn't open queue" << endl;
    return NULL;
  }

  Mat kern1D = getGaussianKernel(FILTER_SIZE, FILTER_SIGMA, CV_32F);
  Mat kern2D = kern1D * kern1D.t();
  
  struct timespec startTime, sendTime, expireTime;
  Mat readImg, procImg;

  unsigned int prio;
  clock_gettime(CLOCK_REALTIME, &startTime);
  syslog(LOG_INFO, "%s (tid = %lu) started at %f", __func__, pthread_self(),  TIMESPEC_TO_MSEC(startTime));
  while(1) {
    /* wait for semaphore */
    clock_gettime(CLOCK_REALTIME, &expireTime);
    expireTime.tv_nsec += PROC_THREAD_SEMA_TIMEOUT;
    if(expireTime.tv_nsec > 1e9) {
      expireTime.tv_sec += 1;
      expireTime.tv_nsec -= 1e9;
    }
    if(sem_timedwait(threadParams.pSema, &expireTime) < 0) {
      if(errno != ETIMEDOUT) {
        syslog(LOG_ERR, "%s error with sem_timedwait, errno: %d [%s]", __func__, errno, strerror(errno));
      } else {
        syslog(LOG_ERR, "%s semaphore timed out", __func__);
      }
    }

    /* read oldest, highest priority msg from the message queue */
    imgDef_t dummy;
    if(mq_receive(selectQueue, (char *)&dummy, SELECT_QUEUE_MSG_SIZE, &prio) < 0) {
      /* don't print if queue was empty */
      if(errno != EAGAIN) {
        syslog(LOG_ERR, "%s error with mq_receive, errno: %d [%s]", __func__, errno, strerror(errno));
      }
    } else {
      if ((dummy.rows == 0) || (dummy.cols == 0)) {
        syslog(LOG_ERR, "%s received bad frame: rows = %d, cols = %d", __func__, dummy.rows, dummy.cols);
      } else {
        Mat readImg(Size(dummy.cols, dummy.rows), dummy.type, dummy.data);

        /* process image */
        syslog(LOG_INFO, "%s processing image", __func__);
        if(threadParams.filter_enable) {
          GaussianBlur(readImg, procImg, Size(FILTER_SIZE, FILTER_SIZE), FILTER_SIGMA);
          // filter2D(procImg, procImg, CV_8U, kern2D);
          // sepFilter2D(procImg, procImg, CV_8U, kern1D, kern1D);
          imshow("procImg", procImg);
        } else {
          imshow("procImg", readImg);
        }
        waitKey(1);
        free(dummy.data);

        /* Send frame to fraemWrite via writeQueue */
        clock_gettime(CLOCK_REALTIME, &expireTime);
        if(mq_timedsend(writeQueue, (char *)&procImg, SELECT_QUEUE_MSG_SIZE, prio, &expireTime) != 0) {
          /* don't print if queue was empty */
          if(errno != ETIMEDOUT) {
            syslog(LOG_ERR, "%s error with mq_timedsend, errno: %d [%s]", __func__, errno, strerror(errno));
          }
          cout << __func__ << " error with mq_timedsend, errno: " << errno << " [" << strerror(errno) << "]" << endl;
        } else {
          clock_gettime(CLOCK_REALTIME, &sendTime);
          syslog(LOG_INFO, "%s sent image#%d to writeQueue at: %f", __func__, cnt, TIMESPEC_TO_MSEC(sendTime));
          ++cnt;
        }
      }
    }
  }

  mq_close(selectQueue);
  mq_close(writeQueue);
  clock_gettime(CLOCK_REALTIME, &startTime);
  syslog(LOG_INFO, "%s (tid = %lu) exiting at: %f", __func__, pthread_self(),  TIMESPEC_TO_MSEC(startTime));
  return NULL;
}