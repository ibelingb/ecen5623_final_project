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

/*---------------------------------------------------------------------------------*/
/* PRIVATE FUNCTIONS */

/*---------------------------------------------------------------------------------*/
/* GLOBAL VARIABLES */

/*---------------------------------------------------------------------------------*/
void *writeTask(void *arg)
{
  char filename[80];
  unsigned int frameNum = 0;
  unsigned int prio;
  struct timespec readTime;
  imgDef_t queueData;

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

  clock_gettime(SYSLOG_CLOCK_TYPE, &readTime);
  syslog(LOG_INFO, "%s (tid = %lu) started at %f", __func__, pthread_self(), TIMESPEC_TO_MSEC(readTime));
	while(1) {
    /* wait for semaphore */
    clock_gettime(SEMA_CLOCK_TYPE, &readTime);
    readTime.tv_nsec += WRITE_THREAD_SEMA_TIMEOUT;
    if(readTime.tv_nsec > 1e9) {
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

    /* Read Frame from writeQueue */
    uint8_t emptyFlag = 0;
    do {
      if(mq_receive(writeQueue, (char *)&queueData, WRITE_QUEUE_MSG_SIZE, &prio) < 0) {
        if (errno == EAGAIN) {
          syslog(LOG_INFO, "%s - No frame available from writeQueue", __func__);
          emptyFlag = 1;
          continue;
        } else if(errno != EAGAIN) {
          syslog(LOG_ERR, "%s error with mq_receive, errno: %d [%s]", __func__, errno, strerror(errno));
        }
      } else {
        if ((queueData.rows == 0) || (queueData.cols == 0)) {
          syslog(LOG_ERR, "%s received bad frame: rows = %d, cols = %d", __func__, queueData.rows, queueData.cols);
        } else {
          /* convert received data into Mat object */
          Mat receivedImg(Size(queueData.cols, queueData.rows), queueData.type, queueData.data);

          /* Save frame to memory */
          sprintf(filename, "./f%d_filt%d_hough%d.jpg", frameNum, threadParams.filter_enable, threadParams.hough_enable);

          // TODO - do we need to write timestamp and platform info here? Or is that being added to image directly?

          // Write frame to output file
          imwrite(filename, receivedImg);

          if(threadParams.save_type != SaveType_e::SAVE_COLOR_IMAGE) {
            cvtColor(receivedImg, receivedImg, COLOR_GRAY2RGB);
          }
          writer.write(receivedImg);
          
          clock_gettime(SYSLOG_CLOCK_TYPE, &readTime);
          syslog(LOG_INFO, "%s image#%d saved at: %.2f", __func__, queueData.diffFrameNum, TIMESPEC_TO_MSEC(readTime));
          ++frameNum;
        }
        // I think it goes here, meaning if you get a frame clean up
        free(queueData.data);
      }
    } while(!emptyFlag);
	}

  /* Thread exit - cleanup */
  mq_close(writeQueue);
  clock_gettime(SYSLOG_CLOCK_TYPE, &readTime);
  syslog(LOG_INFO, "%s (tid = %lu) exiting at: %f", __func__, pthread_self(),  TIMESPEC_TO_MSEC(readTime));

  return NULL;
}