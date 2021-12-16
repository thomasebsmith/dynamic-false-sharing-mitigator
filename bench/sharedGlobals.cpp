#include <iostream>
#include <sys/times.h>
#include <thread>
#include <time.h>

namespace {
volatile int thread_data1 = 0;
volatile int thread_data2 = 0;
volatile int thread_data3 = 0;
volatile int thread_data4 = 0;
} // namespace

const int NUM_LOOPS = 100000;
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
  timespec tpBegin1, tpEnd1;

  clock_gettime(CLOCK_REALTIME, &tpBegin1);
  for (int i = 0; i < NUM_RUNS; i++) {
    std::thread thread1{runThread, &thread_data1};
    std::thread thread2{runThread, &thread_data3};
    thread1.join();
    thread2.join();
  }
  clock_gettime(CLOCK_REALTIME, &tpEnd1);

  auto time1 = compute(tpBegin1, tpEnd1);

  printf("Average time taken over %d runs: %f ms\n", NUM_RUNS,
         time1 / NUM_RUNS);

  return 0;
}
