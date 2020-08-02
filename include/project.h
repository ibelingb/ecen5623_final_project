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
#define MAX_FRAME_COUNT               (1800)

#define TIMESPEC_TO_MSEC(time)	      ((((float)time.tv_sec) * 1.0e3) + (((float)time.tv_nsec) * 1.0e-6))
#define CALC_DT_MSEC(newest, oldest)  (TIMESPEC_TO_MSEC(newest) - TIMESPEC_TO_MSEC(oldest))

/* for queues */
typedef struct {
  uint8_t *data;
  int type;
  int rows;
  int cols;
  size_t elem_size;
  unsigned int diffFrameNum;
} imgDef_t;

#define SELECT_QUEUE_MSG_SIZE         (sizeof(imgDef_t))
#define SELECT_QUEUE_LENGTH           (13)
#define WRITE_QUEUE_MSG_SIZE          (sizeof(cv::Mat))
#define WRITE_QUEUE_LENGTH            (50)
#define CIRCULAR_BUFF_LEN             (13)

/* for synchronization */
#define ACQ_THREAD_SEMA_TIMEOUT       (50e6)
#define DIFF_THREAD_SEMA_TIMEOUT      (500e6)
#define PROC_THREAD_SEMA_TIMEOUT      (1000e6)
#define WRITE_THREAD_SEMA_TIMEOUT     (1000e6)

/* Clock Types */
#define SEMA_CLOCK_TYPE (CLOCK_REALTIME)
#define SYSLOG_CLOCK_TYPE (CLOCK_MONOTONIC)

typedef enum {
  ACQ_THREAD = 0,
  DIFF_THREAD,
  PROC_THREAD,
  WRITE_THREAD,
  SEQ_THREAD,
  TOTAL_THREADS
} Thread_e;
#define TOTAL_RT_THREADS  (TOTAL_THREADS - 1)


typedef enum {
  SAVE_COLOR_IMAGE = 0,
  SAVE_BW_IMAGE,
  SAVE_DIFF_IMAGE,
  SAVE_THRES_IMAGE,
  SAVE_TYPE_END
} SaveType_e;

typedef struct {
  int cameraIdx;                              /* index of camera */
  sem_t *pSema;                               /* semaphore */
  char selectQueueName[64];                   /* message queue */
  char writeQueueName[64];                    /* message queue */
  circular_buffer<cv::Mat> *pCBuff;           /* circular buffer pointer */
  unsigned int hough_enable;                  /* enable hough transformations */
  unsigned int filter_enable;                 /* enable filtering */
  SaveType_e save_type;                       /* type of frame to pass through the pipeline */
  struct timespec programStartTime;           /* start time to make times more reasonable */
} threadParams_t;

typedef struct {
  sem_t *pAcqSema;                            /* Acquire Frame semaphore */
  sem_t *pDiffSema;                           /* Frame Difference semaphore */
  sem_t *pProcSema;                           /* Frame Processing semaphore */
  sem_t *pWriteSema;                          /* Frame Write semaphore */
  sem_t *pSeqSema;                            /* Sequencer semaphore */
} seqThreadParams_t;

#endif