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
  Mat inputImg;
  unsigned int prio;
  struct timespec prevTime, readTime, procTime;
  int cnt = 0;
  
  /* get thread parameters */
  if(arg == NULL) {
    syslog(LOG_ERR, "ERROR: invalid arg provided to %s", __func__);
    return NULL;
  }
  threadParams_t threadParams = *(threadParams_t *)arg;

  /* open handle to queue */
  mqd_t selectQueue = mq_open(threadParams.selectQueueName, O_RDONLY, 0666, NULL);
  if(selectQueue == -1) {
    syslog(LOG_ERR, "%s couldn't open queue", __func__);
    cout << __func__<< " couldn't open queue" << endl;
    return NULL;
  }

  /* open handle to queue */
  mqd_t writeQueue = mq_open(threadParams.writeQueueName, O_RDONLY, 0666, NULL);
  if(writeQueue == -1) {
    syslog(LOG_ERR, "%s couldn't open queue", __func__);
    cout << __func__<< " couldn't open queue" << endl;
    return NULL;
  }

  Mat kern1D = getGaussianKernel(FILTER_SIZE, FILTER_SIGMA, CV_32F);
  Mat kern2D = kern1D * kern1D.t();

  float cumTime = 0.0f;
  float cumProcTime = 0.0f;
  const float deadline_ms = 70.0f;
  float cumJitter_ms;
  
  struct timespec startTime;
  clock_gettime(CLOCK_MONOTONIC, &startTime);
  syslog(LOG_INFO, "%s (id = %d) started at %f", __func__, threadParams.threadIdx,  TIMESPEC_TO_MSEC(startTime));
  while(1) {
    /* read oldest, highest priority msg from the message queue */
    if(mq_receive(selectQueue, (char *)&inputImg, SELECT_QUEUE_MSG_SIZE, &prio) < 0) {
      /* don't print if queue was empty */
      if(errno != EAGAIN) {
        syslog(LOG_ERR, "%s error with mq_receive, errno: %d [%s]", __func__, errno, strerror(errno));
      }
    } else {
      /* process image */
      clock_gettime(CLOCK_MONOTONIC, &procTime);
      if(threadParams.filter_enable) {
        GaussianBlur(inputImg, inputImg, Size(FILTER_SIZE, FILTER_SIZE), FILTER_SIGMA);
        filter2D(inputImg, inputImg, CV_8U, kern2D);
        sepFilter2D(inputImg, inputImg, CV_8U, kern1D, kern1D);
      }

      clock_gettime(CLOCK_MONOTONIC, &readTime);
      if(cnt > 0) {
        cumProcTime += CALC_DT_MSEC(readTime, procTime);
        cumTime += CALC_DT_MSEC(readTime, prevTime);
        cumJitter_ms += deadline_ms - CALC_DT_MSEC(readTime, prevTime);
        if (CALC_DT_MSEC(readTime, prevTime) > deadline_ms) {
          syslog(LOG_ERR, "deadline missed: %f", CALC_DT_MSEC(readTime, prevTime));
        }
      }
      ++cnt;
      prevTime = readTime;
    }
  }
  /* ignore first frame */
  syslog(LOG_INFO, "avg frame time: %f msec",cumTime / (cnt - 1));
  syslog(LOG_INFO, "avg proc time: %f msec", cumProcTime / (cnt - 1));
  syslog(LOG_INFO, "avg jitter: %f msec", cumJitter_ms / (cnt - 1));

  mq_close(selectQueue);
  mq_close(writeQueue);
  clock_gettime(CLOCK_MONOTONIC, &startTime);
  syslog(LOG_INFO, "%s (id = %d) exiting at: %f", __func__, threadParams.threadIdx,  TIMESPEC_TO_MSEC(startTime));
  return NULL;
}