// https://github.com/MJjainam/falseSharing/blob/master/parallelComputing.c

#include <pthread.h>
#include <stdio.h>
#include <sys/times.h>
#include <time.h>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sched.h>

struct timespec tpBegin1, tpEnd1, tpBegin2, tpEnd2, tpBegin3,
    tpEnd3; // These are inbuilt structures to store the time related activities

void print_cpu(const char *label) {
  unsigned cpu, numa;
  getcpu(&cpu, &numa);
  printf("[%s] cpu: %u, numa: %u\n", label, cpu, numa);
}

double
compute(struct timespec start,
        struct timespec end) // computes time in milliseconds given
                             // endTime and startTime timespec structures.
{
  print_cpu("compute begin");
  double t;
  t = (end.tv_sec - start.tv_sec) * 1000;
  t += (end.tv_nsec - start.tv_nsec) * 0.000001;
  print_cpu("compute end");
  return t;
}

int array[100];

void *expensive_function(void *param) {
  print_cpu("expensive_function begin");
  int index = *((int *)param);
  int i;
  for (i = 0; i < 1000000; i++)
    array[index] += 1;
  print_cpu("expensive_function end");
  return nullptr;
}

int main(int argc, char *argv[]) {
  int first_elem = 0;
  int bad_elem = 1;
  int good_elem = 99;
  double time1;
  double time2;
  double time3;
  pthread_t thread_1;
  pthread_t thread_2;

  const int NUM_RUNS = 100;

  print_cpu("main");

  //-------------START--------Serial Computation-------------------------------

  printf("\nDoing serial computation of expensive function\n");
  clock_gettime(CLOCK_REALTIME, &tpBegin1);
  for (int i = 0; i < NUM_RUNS; i++) {
    expensive_function((void *)&first_elem);
    expensive_function((void *)&bad_elem);
  }
  clock_gettime(CLOCK_REALTIME, &tpEnd1);

  //-------------END----------Serial Computation-------------------------------

  //-------------START--------parallel computation with False Sharing----------

  printf("\nDoing parallel computation with false sharing (two expensive "
         "functions)\n");
  clock_gettime(CLOCK_REALTIME, &tpBegin2);
  for (int i = 0; i < NUM_RUNS; i++) {
    pthread_create(&thread_1, NULL, expensive_function, (void *)&first_elem);
    pthread_create(&thread_2, NULL, expensive_function, (void *)&bad_elem);
    pthread_join(thread_1, NULL);
    pthread_join(thread_2, NULL);
  }
  clock_gettime(CLOCK_REALTIME, &tpEnd2);

  //-------------END----------parallel computation with False Sharing----------

  //-------------START--------parallel computation without False Sharing-------

  printf("\nDoing parallel computation with NO false sharing (two expensive "
         "functions)\n");
  clock_gettime(CLOCK_REALTIME, &tpBegin3);
  for (int i = 0; i < NUM_RUNS; i++) {
    pthread_create(&thread_1, NULL, expensive_function, (void *)&first_elem);
    pthread_create(&thread_2, NULL, expensive_function, (void *)&good_elem);
    pthread_join(thread_1, NULL);
    pthread_join(thread_2, NULL);
  }
  clock_gettime(CLOCK_REALTIME, &tpEnd3);

  //-------------END--------parallel computation without False Sharing---------

  //------------START------------------OUTPUT STATS----------------------------
  printf("\nStats:\n");
  printf("array[first_element]: %d\t\t "
         "array[bad_element]: %d\t\t "
         "array[good_element]: %d\n\n",
         array[first_elem], array[bad_elem], array[good_elem]);

  time1 = compute(tpBegin1, tpEnd1);
  time2 = compute(tpBegin2, tpEnd2);
  time3 = compute(tpBegin3, tpEnd3);
  printf("Average time taken over %d runs with false sharing      : %f ms\n",
         NUM_RUNS, time2 / NUM_RUNS);
  printf("Average time taken over %d runs without false sharing  : %f ms\n",
         NUM_RUNS, time3 / NUM_RUNS);
  printf("Average time taken over %d runs in Sequential computing : %f ms\n",
         NUM_RUNS, time1 / NUM_RUNS);

  //------------END------------------OUTPUT STATS------------------------------

  return 0;
}
