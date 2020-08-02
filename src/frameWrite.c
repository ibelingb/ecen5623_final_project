/***********************************************************************************
 * @author Brian Ibeling
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
 * References/Resources Used:
 *  - https://stackoverflow.com/questions/3596310/c-how-to-use-the-function-uname
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
#include <sys/utsname.h>

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

/*---------------------------------------------------------------------------------*/
/* PRIVATE FUNCTIONS */

/*---------------------------------------------------------------------------------*/
/* GLOBAL VARIABLES */

/*---------------------------------------------------------------------------------*/
void *writeTask(void *arg)
{
  char filename[80];
  uint8_t emptyFlag = 0;
  unsigned int prio;
  imgDef_t dummy;
  std::string timestamp;
  std::string procName;
  struct utsname unameData;
  struct timespec timeNow, saveTime;
#if defined(DT_SYSLOG_OUTPUT)
  struct timespec prevSaveTime;
#endif 

  /* get thread parameters */
  if(arg == NULL) {
    syslog(LOG_ERR, "invalid arg provided to %s", __func__);
    return NULL;
  }
  threadParams_t threadParams = *(threadParams_t *)arg;

  /* Verify semaphore is valid */
  if(threadParams.pSema == NULL) {
    syslog(LOG_ERR, "invalid semaphore provided to %s", __func__);
    return NULL;
  }

  /* open non-blocking handle to queue */
  mqd_t writeQueue = mq_open(threadParams.writeQueueName, O_RDONLY | O_NONBLOCK, 0666, NULL);
  if(writeQueue == -1) {
    syslog(LOG_ERR, "%s couldn't open queue", __func__);
    cout << __func__<< " couldn't open queue" << endl;
    return NULL;
  }

  /* create video writer */
  // int codec = VideoWriter::fourcc('M',''P','G','4'');
  // string videoFilename = "video.mp4";
  int codec = VideoWriter::fourcc('M','J','P','G');
  string videoFilename = "video.avi";
  VideoWriter writer;
  int isColor = 1;
  double fps = 10.0;
  writer.open(videoFilename, codec, fps, Size(MAX_IMG_COLS, MAX_IMG_ROWS), isColor);
  if(!writer.isOpened()) {
    cout << "failed to open video writer" << std::endl;
  }

  clock_gettime(SYSLOG_CLOCK_TYPE, &timeNow);
  syslog(LOG_INFO, "%s (tid = %lu) started at %f", __func__, pthread_self(), TIMESPEC_TO_MSEC(timeNow));
	while(1) {
    /* wait for semaphore */
    clock_gettime(SEMA_CLOCK_TYPE, &timeNow);
    timeNow.tv_nsec += WRITE_THREAD_SEMA_TIMEOUT;
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

#if defined(TIMESTAMP_SYSLOG_OUTPUT)
    clock_gettime(SYSLOG_CLOCK_TYPE, &timeNow);
    syslog(LOG_INFO, "%s frame process start:, %.2f, ms", __func__, TIMESPEC_TO_MSEC(timeNow));
#endif

    /* Read Frame from writeQueue */
    emptyFlag = 0;
    do {
      if(mq_receive(writeQueue, (char *)&dummy, WRITE_QUEUE_MSG_SIZE, &prio) < 0) {
        if (errno == EAGAIN) {
          //syslog(LOG_INFO, "%s - No frame available from writeQueue", __func__);
          emptyFlag = 1;
          continue;
        } else if(errno != EAGAIN) {
          syslog(LOG_ERR, "%s error with mq_receive, errno: %d [%s]", __func__, errno, strerror(errno));
        }
      } else {
        if ((dummy.rows == 0) || (dummy.cols == 0)) {
          syslog(LOG_ERR, "%s received bad frame: rows = %d, cols = %d", __func__, dummy.rows, dummy.cols);
        } else {
          /* Convert received data into Mat object */
          Mat receivedImg(Size(dummy.cols, dummy.rows), dummy.type, dummy.data);

          /* Save frame to memory */
          sprintf(filename, "./f%d_filt%d_hough%d.ppm", dummy.diffFrameNum, threadParams.filter_enable, threadParams.hough_enable);

          /* Add timestamp and uname to frame and write frame to output file */
          uname(&unameData);
          timestamp = format("Frame time: %.2f ms", dummy.diffFrameTime);
          procName = format("uname: %s", unameData.sysname);
          putText(receivedImg, timestamp, Point(0, 15), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(255, 255, 255));
          putText(receivedImg, procName, Point(0, 30), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(255, 255, 255));
          imwrite(filename, receivedImg);

          if(threadParams.save_type != SaveType_e::SAVE_COLOR_IMAGE) {
            cvtColor(receivedImg, receivedImg, COLOR_GRAY2RGB);
          }
          writer.write(receivedImg);
          
          clock_gettime(SYSLOG_CLOCK_TYPE, &saveTime);
#if defined(TIMESTAMP_SYSLOG_OUTPUT)
          syslog(LOG_INFO, "%s frame #%d saved at:, %.2f, ms", __func__, dummy.diffFrameNum, TIMESPEC_TO_MSEC(saveTime));
#endif
#if defined(DT_SYSLOG_OUTPUT)
          syslog(LOG_INFO, "%s saved frame#%d, dt since start: %.2f ms, dt since last frame saved: %.2f ms", __func__, dummy.diffFrameNum,
                 CALC_DT_MSEC(saveTime, threadParams.programStartTime), CALC_DT_MSEC(saveTime, prevSaveTime));
          prevSaveTime.tv_sec = saveTime.tv_sec;
          prevSaveTime.tv_nsec = saveTime.tv_nsec;
#endif
        }
        /* free malloced data */
        free(dummy.data);
      }
    } while(!emptyFlag);
	}

  /* Thread exit - cleanup */
  mq_close(writeQueue);
  clock_gettime(SYSLOG_CLOCK_TYPE, &timeNow);
  syslog(LOG_INFO, "%s (tid = %lu) exiting at: %f", __func__, pthread_self(),  TIMESPEC_TO_MSEC(timeNow));

  return NULL;
}