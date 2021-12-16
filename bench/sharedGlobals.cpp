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
  timespec tpBegin1, tpEnd1;
  volatile int *thread1_data;
  volatile int *thread2_data;
  int data_choice1, data_choice2;
  std::cout << "Input thread data selection:" << std::endl;
  std::cin >> data_choice1 >> data_choice2;

  switch (data_choice1) {
  case 1:
    thread1_data = &thread_data1;
    break;
  case 2:
    thread1_data = &thread_data2;
    break;
  case 3:
    thread1_data = &thread_data3;
    break;
  case 4:
    thread1_data = &thread_data4;
    break;
  default:
    exit(1);
  }

  switch (data_choice2) {
  case 1:
    thread2_data = &thread_data1;
    break;
  case 2:
    thread2_data = &thread_data2;
    break;
  case 3:
    thread2_data = &thread_data3;
    break;
  case 4:
    thread2_data = &thread_data4;
    break;
  default:
    exit(1);
  }

  clock_gettime(CLOCK_REALTIME, &tpBegin1);
  for (int i = 0; i < NUM_RUNS; i++) {
    std::thread thread1{runThread, thread1_data};
    std::thread thread2{runThread, thread2_data};
    thread1.join();
    thread2.join();
  }
  clock_gettime(CLOCK_REALTIME, &tpEnd1);

  auto time1 = compute(tpBegin1, tpEnd1);

  printf("Average time taken over %d runs: %f ms\n", NUM_RUNS,
         time1 / NUM_RUNS);

  return 0;
}
