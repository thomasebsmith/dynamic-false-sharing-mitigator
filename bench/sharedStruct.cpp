#include <iostream>
#include <sys/times.h>
#include <thread>
#include <time.h>

struct FalseSharedStruct {
  volatile int thread1Data = 0;
  volatile int thread2Data = 0;
};

struct SharedStruct {
  alignas(64) volatile int thread1Data = 0;
  alignas(64) volatile int thread2Data = 0;
};

FalseSharedStruct false_shared_data;
SharedStruct shared_data;

const int NUM_LOOPS = 10;
const int NUM_RUNS = 1;

double compute(struct timespec start, struct timespec end) {
  double t;
  t = (end.tv_sec - start.tv_sec) * 1000;
  t += (end.tv_nsec - start.tv_nsec) * 0.000001;
  return t;
}

void runThread(volatile int *threadData) {
  for (int i = 0; i < NUM_LOOPS; ++i) {
    ++(*threadData);
  }
}

int main() {
  timespec tpBegin1, tpEnd1, tpBegin2, tpEnd2;
  clock_gettime(CLOCK_REALTIME, &tpBegin1);
  for (int i = 0; i < NUM_RUNS; i++) {
    std::thread thread1{runThread, &false_shared_data.thread1Data};
    std::thread thread2{runThread, &false_shared_data.thread2Data};
    thread1.join();
    thread2.join();
  }
  clock_gettime(CLOCK_REALTIME, &tpEnd1);

  clock_gettime(CLOCK_REALTIME, &tpBegin2);
  for (int i = 0; i < NUM_RUNS; i++) {
    std::thread thread1{runThread, &shared_data.thread1Data};
    std::thread thread2{runThread, &shared_data.thread2Data};
    thread1.join();
    thread2.join();
  }
  clock_gettime(CLOCK_REALTIME, &tpEnd2);

  auto time1 = compute(tpBegin1, tpEnd1);
  auto time2 = compute(tpBegin2, tpEnd2);

  printf("Average time taken over %d runs with false sharing      : %f ms\n",
         NUM_RUNS, time1 / NUM_RUNS);

  printf("Average time taken over %d runs without false sharing      : %f ms\n",
         NUM_RUNS, time2 / NUM_RUNS);

  return 0;
}
