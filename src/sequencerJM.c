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
 * @file sequencer.cs
.c
 * @brief 
 *
 ************************************************************************************
 */

/*---------------------------------------------------------------------------------*/
/* INCLUDES */
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

#include <iostream> // for standard I/O
#include <string>   // for strings
#include <iomanip>  // for controlling float print precision
#include <sstream>  // string to number conversion

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
void *sequencerTask(void *arg)
{
  /* get thread parameters */
  if(arg == NULL) {
    syslog(LOG_ERR, "invalid arg provided to %s", __func__);
    return NULL;
  }
  threadParams_t threadParams = *(threadParams_t *)arg;

  struct timespec startTime;
  clock_gettime(CLOCK_MONOTONIC, &startTime);
  syslog(LOG_INFO, "%s (id = %d) started at %f", __func__, threadParams.threadIdx,  TIMESPEC_TO_MSEC(startTime));
	while(1) {

		sleep(1);
	}
  clock_gettime(CLOCK_MONOTONIC, &startTime);
  syslog(LOG_INFO, "%s (id = %d) exiting at: %f", __func__, threadParams.threadIdx,  TIMESPEC_TO_MSEC(startTime));
  return NULL;
}