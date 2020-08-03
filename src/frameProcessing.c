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
  
  struct timespec timeNow, sendTime;
#if defined(DT_SYSLOG_OUTPUT)
  struct timespec prevSendTime;
#endif
  Mat readImg, procImg;

  unsigned int prio;
  unsigned int timeoutCnt = 0;
  clock_gettime(SYSLOG_CLOCK_TYPE, &timeNow);
  syslog(LOG_INFO, "%s (tid = %lu) started at %f", __func__, pthread_self(),  TIMESPEC_TO_MSEC(timeNow));
  while(1) {
    /* wait for semaphore */
    clock_gettime(SEMA_CLOCK_TYPE, &timeNow);
    timeNow.tv_nsec += PROC_THREAD_SEMA_TIMEOUT;
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

    /* read oldest, highest priority msg from the message queue */
    imgDef_t dummy;
    uint8_t emptyFlag = 0;
    do {
#if defined(TIMESTAMP_SYSLOG_OUTPUT)
      clock_gettime(SYSLOG_CLOCK_TYPE, &timeNow);
      syslog(LOG_INFO, "%s frame process start:,  %.2f, ms", __func__, TIMESPEC_TO_MSEC(timeNow));
#endif

      if(mq_receive(selectQueue, (char *)&dummy, SELECT_QUEUE_MSG_SIZE, &prio) < 0) {
        if(errno != EAGAIN) {
          syslog(LOG_ERR, "%s error with mq_receive, errno: %d [%s]", __func__, errno, strerror(errno));
        } else {
          emptyFlag = 1;
        }
      } else {
        if ((dummy.rows == 0) || (dummy.cols == 0)) {
          syslog(LOG_ERR, "%s received bad frame: rows = %d, cols = %d", __func__, dummy.rows, dummy.cols);
        } else {
          Mat readImg(Size(dummy.cols, dummy.rows), dummy.type, dummy.data);

          if(threadParams.save_type == SaveType_e::SAVE_COLOR_IMAGE) {
            cvtColor(readImg, procImg, COLOR_RGB2GRAY);
          } else {
            procImg = readImg.clone();
          }
          Mat gray = procImg.clone();

          /* add edge enhancement */
          if(threadParams.filter_enable) {
            int thres = 70;
            Canny(procImg, procImg, thres, thres*4, 3);
          }

          /* Draw the lines */
          if(threadParams.hough_enable) {
            /* find lines */
            vector<Vec4i> linesP;
            HoughLinesP(procImg,  // grayscale input image
                linesP,           // output vector of lines
                1,                // distance resolution
                CV_PI/180,        // angle resolution
                80,               // score threshold
                80,               // minimum length
                20);              // maximum allowed gap
            
            /* Draw the lines */
            if(threadParams.save_type != SaveType_e::SAVE_COLOR_IMAGE) {
              cvtColor(procImg, procImg, COLOR_GRAY2RGB);
            }
            
            for( size_t i = 0; i < linesP.size(); i++ )
            {
                Vec4i l = linesP[i];
                line(readImg, Point(l[0], l[1]), Point(l[2], l[3]), Scalar(0, 0, 255), 5, LINE_AA);
            }

            /* draw circles */
            medianBlur(gray, gray, 5);
            vector<Vec3f> circles;
            HoughCircles(gray, circles, 
                        HOUGH_GRADIENT, // method
                        1,              // dp inverse accumulator resolution
                        gray.rows/4,    // min distance between centers of detected circles
                        100,            // high threshold
                        30,             // low threshold
                        140,            // min. circle radius
                        250             // max. circle radius
            );

            for( size_t i = 0; i < circles.size(); i++ )
            {
                Vec3i c = circles[i];
                Point center = Point(c[0], c[1]);
                circle( readImg, center, 1, Scalar(255,0, 0), 3, LINE_AA);

                int radius = c[2];
                circle( readImg, center, radius, Scalar(255,0,0), 3, LINE_AA);
            }
          }

          imshow("readImg", readImg);
          waitKey(1);

          /* Send frame to frameWrite via writeQueue */
          clock_gettime(SEMA_CLOCK_TYPE, &timeNow);
          if(mq_timedsend(writeQueue, (char *)&dummy, SELECT_QUEUE_MSG_SIZE, prio, &timeNow) != 0) {
            if(errno == ETIMEDOUT) {
              cout << __func__ << " mq_timedsend(writeQueue, ...) TIMEOUT#" << timeoutCnt++ << endl;
            } 
            free(dummy.data);
            syslog(LOG_ERR, "%s error with mq_timedsend, errno: %d [%s]", __func__, errno, strerror(errno));
          } else {
            clock_gettime(SYSLOG_CLOCK_TYPE, &sendTime);
#if defined(TIMESTAMP_SYSLOG_OUTPUT)
            syslog(LOG_INFO, "%s frame #%d inserted to writeQueue at:,  %.2f, ms", __func__, dummy.diffFrameNum, TIMESPEC_TO_MSEC(sendTime));
#endif
#if defined(DT_SYSLOG_OUTPUT)
            syslog(LOG_INFO, "%s inserted frame#%d to writeQueue, dt since start: %.2f ms, dt since last frame sent: %.2f ms", __func__, dummy.diffFrameNum,
                   CALC_DT_MSEC(sendTime, threadParams.programStartTime), CALC_DT_MSEC(sendTime, prevSendTime));
            prevSendTime.tv_sec = sendTime.tv_sec;
            prevSendTime.tv_nsec = sendTime.tv_nsec;
#endif
            ++cnt;
          }
        }
      }
    } while(!emptyFlag);
  }

  mq_close(selectQueue);
  mq_close(writeQueue);
  clock_gettime(SYSLOG_CLOCK_TYPE, &timeNow);
  syslog(LOG_INFO, "%s (tid = %lu) exiting at: %f", __func__, pthread_self(),  TIMESPEC_TO_MSEC(timeNow));
  return NULL;
}