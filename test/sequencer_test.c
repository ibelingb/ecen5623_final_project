/* ECEN5623 - Real-Time Embedded Systems
 * Brian Ibeling
 * 7/25/2020
 * sequencer.c
 *
 * TODO
 *
 * References and Resources:
 *   - http://ecee.colorado.edu/~ecen5623/ecen/ex/Linux/sequencer_generic/seqgen3.c
 */
/*------------------------------------------------------------------------*/
// This is necessary for CPU affinity macros in Linux
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <semaphore.h>

#include <syslog.h>
#include <sys/time.h>
#include <sys/sysinfo.h>
#include <errno.h>

#include <signal.h>

/*------------------------------------------------------------------------*/
#define USEC_PER_MSEC (1000)
#define NANOSEC_PER_MSEC (1000000)
#define NANOSEC_PER_SEC (1000000000)
#define NUM_CPU_CORES (4)
#define TRUE (1)
#define FALSE (0)

#define CLOCK_TYPE CLOCK_MONOTONIC_RAW
#define SCHED_TYPE SCHED_FIFO

#define NUM_THREADS (1)
/*------------------------------------------------------------------------*/
typedef struct
{
    int threadIdx;
} threadParams_t;

/*------------------------------------------------------------------------*/
sem_t semS1, semS2, semS3, semS4, semS5;
static unsigned long long sequenceCount = 0;

cpu_set_t threadcpu;

double start_realtime;

static timer_t seqTimer;
static struct itimerspec itime = {{1, 0}, {1, 0}};
static struct itimerspec last_itime;

pthread_t threads[NUM_THREADS];
pthread_attr_t rt_sched_attr[NUM_THREADS];
threadParams_t threadParams[NUM_THREADS];


/*------------------------------------------------------------------------*/
/*** METHODS ***/
/*------------------------------------------------------------------------*/
double getTimeMsec(void) {
  struct timespec event_ts = {0, 0};

  clock_gettime(CLOCK_TYPE, &event_ts);
  return ((event_ts.tv_sec) * 1000.0) + ((event_ts.tv_nsec) / 1000000.0);
}

/*------------------------------------------------------------------------*/
double realtime(struct timespec *tsptr) {
  return ((double)(tsptr->tv_sec) + (((double)tsptr->tv_nsec) / 1000000000.0));
}

/*------------------------------------------------------------------------*/
void sequencer(void) {
  /* Increment sequence count */
  sequenceCount++;

  /* Post a semaphore for each service based on sub-rate of sequencer */

  // Servcie_1 = @ 1 Hz
  if((sequenceCount % 100) == 0) sem_post(&semS1);

  // TODO - Add other services

}
/*------------------------------------------------------------------------*/
void *testService(void *threadp) {
  struct timespec current_time_val;
  double current_realtime;
  unsigned long long S1Cnt = 0;
  threadParams_t *threadParams = (threadParams_t *)threadp;

  // Start up processing and resource initialization
  clock_gettime(CLOCK_TYPE, &current_time_val);
  current_realtime = realtime(&current_time_val);
  syslog(LOG_CRIT, "S1 thread @ sec=%6.9lf\n", current_realtime - start_realtime);
  printf("S1 thread @ sec=%6.9lf\n", current_realtime - start_realtime);

  while (1) // check for synchronous abort request
  {
    // wait for service request from the sequencer, a signal handler or ISR in kernel
    sem_wait(&semS1);

    // TODO - remove
    printf("testService Called\n");

    S1Cnt++;

    // DO WORK

    // on order of up to milliseconds of latency to get time
    clock_gettime(CLOCK_TYPE, &current_time_val);
    current_realtime = realtime(&current_time_val);
    syslog(LOG_CRIT, "S1 1 Hz on core %d for release %llu @ sec=%6.9lf\n", sched_getcpu(), S1Cnt, current_realtime - start_realtime);
  }

  // Resource shutdown here
  pthread_exit((void *)0);

}

/*------------------------------------------------------------------------*/
void *acquireFrames(void *threadp) {

}

/*------------------------------------------------------------------------*/

/*------------------------------------------------------------------------*/

/*------------------------------------------------------------------------*/

/*------------------------------------------------------------------------*/

/*------------------------------------------------------------------------*/
void main(void) {
  int status;
  int i;
  int rtMaxPriority;
  int rtMinPriority;
  int flags = 0;
  pid_t mainpid;
  cpu_set_t cpuSet;
  struct sched_param rt_param[NUM_THREADS];
  struct sched_param mainSchedParam;


  /* Initialize Sequencer semaphores */
  if (sem_init(&semS1, 0, 0)) {
    printf("Failed to initialize S1 semaphore\n");
    exit(-1);
  }
  if (sem_init(&semS2, 0, 0)) {
    printf("Failed to initialize S2 semaphore\n");
    exit(-1);
  }
  if (sem_init(&semS3, 0, 0)) {
    printf("Failed to initialize S3 semaphore\n");
    exit(-1);
  }
  if (sem_init(&semS4, 0, 0)) {
    printf("Failed to initialize S4 semaphore\n");
    exit(-1);
  }
  if (sem_init(&semS5, 0, 0)) {
    printf("Failed to initialize S5 semaphore\n");
    exit(-1);
  }


  /* Set services to run on specific system cores */
  CPU_ZERO(&cpuSet);
  for (i = 0; i < NUM_CPU_CORES; i++) {
    CPU_SET(i, &cpuSet);
  }


  /* Set system scheduler */
  mainpid = getpid();
  rtMaxPriority = sched_get_priority_max(SCHED_TYPE);
  rtMinPriority = sched_get_priority_min(SCHED_TYPE);

  status = sched_getparam(mainpid, &mainSchedParam);
  mainSchedParam.sched_priority = rtMaxPriority;
  status = sched_setscheduler(getpid(), SCHED_TYPE, &mainSchedParam);
  if (status < 0) {
    perror("mainSchedParam");
  }

  /* Set thread scheduling policy and parameters */
  for (i = 0; i < NUM_THREADS; i++) {
    pthread_attr_init(&rt_sched_attr[i]);
    pthread_attr_setinheritsched(&rt_sched_attr[i], PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&rt_sched_attr[i], SCHED_TYPE);
  }

  /* Create Service Threads running on Core 2 */
  CPU_ZERO(&threadcpu);
  CPU_SET(2, &threadcpu);

  /* Create test service thread */
  rt_param[0].sched_priority = rtMaxPriority - 1;
  pthread_attr_setschedparam(&rt_sched_attr[0], &rt_param[0]);
  pthread_attr_setaffinity_np(&rt_sched_attr[0], sizeof(cpu_set_t), &threadcpu);
  status = pthread_create(&threads[0], &rt_sched_attr[0], testService, (void *)&(threadParams[0]));
  if (status < 0)
    perror("pthread_create for testService");
  else
    printf("pthread_create successful for testService\n");


  /* Create Service Threads running on Core 3 */
  CPU_ZERO(&threadcpu);
  CPU_SET(3, &threadcpu);
  // TODO


  /* Create Sequencer */
  timer_create(CLOCK_REALTIME, NULL, &seqTimer);
  signal(SIGALRM, (void (*)())sequencer);
  itime.it_interval.tv_sec = 0;
  itime.it_interval.tv_nsec = 10000000;
  itime.it_value.tv_sec = 0;
  itime.it_value.tv_nsec = 10000000;
  timer_settime(seqTimer, flags, &itime, &last_itime);

  /* Join threads */
  for (i=0; i<NUM_THREADS; i++) {
    if (status = pthread_join(threads[i], NULL) < 0)
      perror("main pthread_join");
    else
      printf("joined thread %d\n", i);
  }
  /* */

  /* */
}