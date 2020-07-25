/***********************************************************************************
 * @author Joshua Malburg
 * joshua.malburg@colordo.edu
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
#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>  // OpenCV window I/O

#include <iostream> // for standard I/O
#include <string>   // for strings
#include <iomanip>  // for controlling float print precision
#include <sstream>  // string to number conversion

using namespace cv;
using namespace std;

/*---------------------------------------------------------------------------------*/
/* MACROS / TYPES / CONST */

/*---------------------------------------------------------------------------------*/
/* PRIVATE FUNCTIONS */

/*---------------------------------------------------------------------------------*/
/* GLOBAL VARIABLES */

/*---------------------------------------------------------------------------------*/
void *acquisitionTask(void*arg)
{
  unsigned int cnt = 0;
  unsigned int prio = 30;
  Mat readImg;
  struct timespec expireTime;
  struct timespec startTime;

  /* get thread parameters */
  if(arg == NULL) {
    syslog(LOG_ERR, "invalid arg provided to %s", __func__);
    return NULL;
  }
  threadParams_t threadParams = *(threadParams_t *)arg;

  /* open handle to queue */
  mqd_t msgQueue = mq_open(threadParams.msgQueueName,O_WRONLY, 0666, NULL);
  if(msgQueue == -1) {
    syslog(LOG_ERR, "%s couldn't open queue", __func__);
    cout << __func__<< " couldn't open queue" << endl;
    return NULL;
  }

  /* open camera stream */
  VideoCapture cam;
  if(!cam.open(threadParams.cameraIdx)) {
    syslog(LOG_ERR, "couldn't open camera");
    cout << "couldn't open camera" << endl;
    return NULL;
  } else {
    cam.set(CAP_PROP_FRAME_WIDTH, 640 / threadParams.decimateFactor);
    cam.set(CAP_PROP_FRAME_HEIGHT, 480 / threadParams.decimateFactor);
    cout  << "cam size (HxW): " << cam.get(CAP_PROP_FRAME_WIDTH)
          << " x " << cam.get(CAP_PROP_FRAME_HEIGHT) << endl;
  }

  syslog(LOG_INFO, "%s started ...", __func__);
  clock_gettime(CLOCK_MONOTONIC, &startTime);
  while(1) {
    /* read image from video */
    cam >> readImg;

    /* try to insert image but don't block if full
     * so that we loop around and just get the newest */
    clock_gettime(CLOCK_MONOTONIC, &expireTime);
    if(mq_timedsend(msgQueue, (const char *)&readImg, MAX_MSG_SIZE, prio, &expireTime) != 0) {
      /* don't print if queue was empty */
      if(errno != ETIMEDOUT) {
        syslog(LOG_ERR, "%s error with mq_send, errno: %d [%s]", __func__, errno, strerror(errno));
      }
      cout << __func__ << " error with mq_send, errno: " << errno << " [" << strerror(errno) << "]" << endl;
    } else {
      ++cnt;
    }
  }
  gAbortTest = 1;
  syslog(LOG_INFO, "%s exiting", __func__);
  mq_close(msgQueue);
  return NULL;
}