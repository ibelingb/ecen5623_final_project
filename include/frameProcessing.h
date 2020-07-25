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
 * @file frameProcessing.h
 * @brief 
 *
 ************************************************************************************
 */
#ifndef FRAME_PROCESSING_H
#define FRAME_PROCESSING_H

/*---------------------------------------------------------------------------------*/
/* INCLUDES */

/*---------------------------------------------------------------------------------*/
/* MACROS / TYPES / CONST */

#define FILTER_SIZE   (31)
#define FILTER_SIGMA  (2.0)

/*---------------------------------------------------------------------------------*/

/**
 * @brief
 * 
 * @param
 * @param
 * @return
 */
void *processingTask(void *arg);

#endif