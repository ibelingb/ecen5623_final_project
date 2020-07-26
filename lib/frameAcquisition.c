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

/* project headers */
#include "project.h"

/*---------------------------------------------------------------------------------*/
/* MACROS / TYPES / CONST */

/*---------------------------------------------------------------------------------*/
/* PRIVATE FUNCTIONS */

/*---------------------------------------------------------------------------------*/
/* GLOBAL VARIABLES */

/*---------------------------------------------------------------------------------*/
void *acquisitionTask(void*arg)
{
  Mat readImg;
  struct timespec readTime;

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

  struct timespec startTime;
  clock_gettime(CLOCK_MONOTONIC, &startTime);
  syslog(LOG_INFO, "%s (id = %d) started at %f", __func__, threadParams.threadIdx,  TIMESPEC_TO_MSEC(startTime));
  while(1) {
    /* read image from video */
    cam >> readImg;
    clock_gettime(CLOCK_MONOTONIC, &readTime);

    /* todo: insert in circular buffer */
  }
  clock_gettime(CLOCK_MONOTONIC, &startTime);
  syslog(LOG_INFO, "%s (id = %d) exiting at: %f", __func__, threadParams.threadIdx,  TIMESPEC_TO_MSEC(startTime));
  return NULL;
}