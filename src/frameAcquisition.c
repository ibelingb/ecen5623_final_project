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
 * @file frameAcquisition.c
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
#include <opencv2/highgui.hpp>  // OpenCV window I/O

#include <iostream>             // for standard I/O
#include <string>               // for strings
#include <iomanip>              // for controlling float print precision
#include <sstream>              // string to number conversion

using namespace cv;
using namespace std;

/* project headers */
#include "project.h"
#include "circular_buffer.h"

/*---------------------------------------------------------------------------------*/
/* MACROS / TYPES / CONST */

/*---------------------------------------------------------------------------------*/
/* PRIVATE FUNCTIONS */

/*---------------------------------------------------------------------------------*/
/* GLOBAL VARIABLES */

/*---------------------------------------------------------------------------------*/
void *acquisitionTask(void*arg)
{
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

  /* open camera stream */
  VideoCapture cam;
  if(!cam.open(threadParams.cameraIdx)) {
    syslog(LOG_ERR, "couldn't open camera");
    cout << "couldn't open camera" << endl;
    return NULL;
  } else {
    cam.set(CAP_PROP_FRAME_WIDTH, 640);
    cam.set(CAP_PROP_FRAME_HEIGHT, 480);
    cout  << "cam size (HxW): " << cam.get(CAP_PROP_FRAME_WIDTH)
          << " x " << cam.get(CAP_PROP_FRAME_HEIGHT) << endl;
  }

  Mat readImg;
  struct timespec readTime;
  clock_gettime(CLOCK_MONOTONIC, &readTime);
  syslog(LOG_INFO, "%s (tid = %lu) started at %f", __func__, pthread_self(),  TIMESPEC_TO_MSEC(readTime));
  while(1) {
    /* wait for semaphore */
    clock_gettime(CLOCK_REALTIME, &readTime);
    readTime.tv_nsec += ACQ_THREAD_SEMA_TIMEOUT;
    if(readTime.tv_nsec  > 1e9) {
      readTime.tv_sec += 1;
      readTime.tv_nsec -= 1e9;
    }
    if(sem_timedwait(threadParams.pSema, &readTime) < 0) {
      if(errno != ETIMEDOUT) {
        syslog(LOG_ERR, "%s error with sem_timedwait, errno: %d [%s]", __func__, errno, strerror(errno));
      } else {
        syslog(LOG_ERR, "%s semaphore timed out", __func__);
      }
    }

    /* read image from video */
    cam >> readImg;
    if(!readImg.empty()) {
      clock_gettime(CLOCK_MONOTONIC, &readTime);

      /* insert in circular buffer */
      threadParams.pCBuff->put(readImg);
      syslog(LOG_INFO, "frame acquired/inserted at: %.2f, dt since start: %.2f", TIMESPEC_TO_MSEC(readTime), CALC_DT_MSEC(readTime, threadParams.programStartTime));
      if(threadParams.pCBuff->full()) {
        syslog(LOG_WARNING, "circular buffer full");
      }
    }
  }
  clock_gettime(CLOCK_MONOTONIC, &readTime);
  syslog(LOG_INFO, "%s (tid = %lu) exiting at: %f", __func__, pthread_self(),  TIMESPEC_TO_MSEC(readTime));
  return NULL;
}