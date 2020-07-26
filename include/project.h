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
 * @file project.h
 * @brief 
 *
 ************************************************************************************
 */
#ifndef PROJECT_TYPES_H
#define PROJECT_TYPES_H

/*---------------------------------------------------------------------------------*/
/* INCLUDES */
#include <semaphore.h>
#include <opencv2/core.hpp>     // Basic OpenCV structures (cv::Mat, Scalar)
#include "circular_buffer.h"

/*---------------------------------------------------------------------------------*/
/* MACROS / TYPES / CONST */

#define MAX_IMG_ROWS                  (480)
#define MAX_IMG_COLS                  (640)

#define TIMESPEC_TO_MSEC(time)	      ((((float)time.tv_sec) * 1.0e3) + (((float)time.tv_nsec) * 1.0e-6))
#define CALC_DT_MSEC(newest, oldest)  (TIMESPEC_TO_MSEC(newest) - TIMESPEC_TO_MSEC(oldest))

#define SELECT_QUEUE_MSG_SIZE         (sizeof(cv::Mat))
#define SELECT_QUEUE_LENGTH           (10)
#define WRITE_QUEUE_MSG_SIZE          (sizeof(cv::Mat))
#define WRITE_QUEUE_LENGTH            (10)
#define CIRCULAR_BUFF_LEN             (12)


typedef enum {
  USE_GAUSSIAN_BLUR,
  USE_FILTER_2D,
  USE_SEP_FILTER_2D
} FilterType_e;

typedef struct {
  int threadIdx;                      /* thread id */
  int cameraIdx;                      /* index of camera */
  sem_t *pSema;                       /* semaphore */
  char selectQueueName[64];           /* message queue */
  char writeQueueName[64];            /* message queue */
  pthread_mutex_t *pBuffMutex;        /* image circular buffer queue */
  circular_buffer<cv::Mat> *pCBuff;   /* circular buffer pointer */
  unsigned int hough_enable;          /* enable hough transformations */
  unsigned int filter_enable;         /* enable filtering */
} threadParams_t;

#endif