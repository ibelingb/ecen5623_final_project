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
  struct timespec timeNow;
#if defined(DT_SYSLOG_OUTPUT)
  struct timespec prevReadTime;
#endif

  clock_gettime(SYSLOG_CLOCK_TYPE, &timeNow);
  syslog(LOG_INFO, "%s (tid = %lu) started at %f", __func__, pthread_self(),  TIMESPEC_TO_MSEC(timeNow));
  unsigned int readCnt = 0;
  while(1) {
    /* wait for semaphore */
    clock_gettime(SEMA_CLOCK_TYPE, &timeNow);
    timeNow.tv_nsec += ACQ_THREAD_SEMA_TIMEOUT;
    if(timeNow.tv_nsec  > 1e9) {
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

    clock_gettime(SYSLOG_CLOCK_TYPE, &timeNow);
#if defined(TIMESTAMP_SYSLOG_OUTPUT)
    syslog(LOG_INFO, "%s frame received at:, %.2f, ms", __func__, TIMESPEC_TO_MSEC(timeNow));
#endif

    /* read image from video */
    cam >> readImg;

    /* verify we've skipped required frames at start */
    if((!readImg.empty()) && (++readCnt > FRAMES_TO_SKIP)) {
      readCnt = FRAMES_TO_SKIP;

      // sprintf(filename, "./f%d_filt%d_hough%d.ppm", dummy.diffFrameNum, threadParams.filter_enable, threadParams.hough_enable);
      // imwrite(filename, receivedImg);

      /* insert in circular buffer */
      threadParams.pCBuff->put(readImg);

#if defined(TIMESTAMP_SYSLOG_OUTPUT)
      clock_gettime(SYSLOG_CLOCK_TYPE, &timeNow);
      syslog(LOG_INFO, "%s frame inserted to CircBuffer at:, %.2f, ms", __func__, TIMESPEC_TO_MSEC(timeNow));
#endif
#if defined(DT_SYSLOG_OUTPUT)
      syslog(LOG_INFO, "%s frame inserted to CB, dt since start: %.2f ms, dt since last frame: %.2f ms", __func__,
             CALC_DT_MSEC(timeNow, threadParams.programStartTime), CALC_DT_MSEC(timeNow, prevReadTime));
      prevReadTime.tv_sec = timeNow.tv_sec;
      prevReadTime.tv_nsec = timeNow.tv_nsec;
#endif
      if(threadParams.pCBuff->full()) {
        syslog(LOG_WARNING, "%s CB is full!", __func__);
      }
    }
  }
  clock_gettime(SYSLOG_CLOCK_TYPE, &timeNow);
  syslog(LOG_INFO, "%s (tid = %lu) exiting at: %f", __func__, pthread_self(),  TIMESPEC_TO_MSEC(timeNow));
  return NULL;
}