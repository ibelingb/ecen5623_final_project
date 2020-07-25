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

/*---------------------------------------------------------------------------------*/
/* MACROS / TYPES / CONST */

#define MAX_MSG_SIZE                  (sizeof(cv::Mat))
#define TIMESPEC_TO_MSEC(time)	      ((((float)time.tv_sec) * 1.0e3) + (((float)time.tv_nsec) * 1.0e-6))
#define CALC_DT_MSEC(newest, oldest)  (TIMESPEC_TO_MSEC(newest) - TIMESPEC_TO_MSEC(oldest))

typedef enum {
  USE_GAUSSIAN_BLUR,
  USE_FILTER_2D,
  USE_SEP_FILTER_2D
} FilterType_e;

typedef struct {
  int threadIdx;              /* thread id */
  int cameraIdx;              /* index of camera */
  char msgQueueName[64];      /* message queue */
  unsigned int decimateFactor;
  FilterType_e filterMethod;
} threadParams_t;

#endif