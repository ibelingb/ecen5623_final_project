/***********************************************************************************
 * @author Brian Ebeling
 * ibelingb@colorado.edu
 * 
 * Real-time Embedded Systems
 * ECEN5623 - Sam Siewert
 * @date 25Jul2020
 * Ubuntu 18.04 LTS and RPi 3B+
 ************************************************************************************
 *
 * @file frameWrite.c
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
void *writeTask(void *arg)
{
  /* get thread parameters */
  if(arg == NULL) {
    syslog(LOG_ERR, "invalid arg provided to %s", __func__);
    return NULL;
  }
  threadParams_t threadParams = *(threadParams_t *)arg;

  /* open handle to queue */
  mqd_t writeQueue = mq_open(threadParams.writeQueueName,O_RDONLY, 0666, NULL);
  if(writeQueue == -1) {
    syslog(LOG_ERR, "%s couldn't open queue", __func__);
    cout << __func__<< " couldn't open queue" << endl;
    return NULL;
  }

  char filename[80];
  Mat img = Mat::zeros(Size(MAX_IMG_COLS, MAX_IMG_ROWS), CV_8UC3);

  struct timespec startTime;
  clock_gettime(CLOCK_REALTIME, &startTime);
  syslog(LOG_INFO, "%s (tid = %lu) started at %f", __func__, pthread_self(),  TIMESPEC_TO_MSEC(startTime));
	while(1) {
    sprintf(filename,"filt%d_hough%d.jpg",threadParams.filter_enable, threadParams.hough_enable);
    imwrite(filename, img);
    sleep(1);
	}
  mq_close(writeQueue);
  clock_gettime(CLOCK_REALTIME, &startTime);
  syslog(LOG_INFO, "%s (tid = %lu) exiting at: %f", __func__, pthread_self(),  TIMESPEC_TO_MSEC(startTime));
  return NULL;
}