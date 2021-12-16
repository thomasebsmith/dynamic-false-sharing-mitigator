#include <condition_variable>
#include <iostream>
#include <mutex>
#include <sys/times.h>
#include <thread>
#include <time.h>
#include <vector>

std::mutex m1;
std::mutex m2;
std::condition_variable c1;
std::condition_variable c2;
std::vector<int> v1;
std::vector<int> v2;

const int NUM_LOOPS = 10000;
const int NUM_RUNS = 500;

double compute(struct timespec start, struct timespec end) {
  double t;
  t = (end.tv_sec - start.tv_sec) * 1000;
  t += (end.tv_nsec - start.tv_nsec) * 0.000001;
  return t;
}

void producer(const int threadID) {
  for (int i = 0; i < NUM_LOOPS; ++i) {
    switch (threadID) {
    case 1:
      m1.lock();
      v1.push_back(i);
      m1.unlock();
      c1.notify_one();
      break;
    case 2:
      m2.lock();
      v2.push_back(i);
      m2.unlock();
      c2.notify_one();
      break;
    }
  }
}

void consumer(int threadID) {
  int val = 0;
  auto lk1 = std::unique_lock<std::mutex>(m1, std::defer_lock);
  auto lk2 = std::unique_lock<std::mutex>(m2, std::defer_lock);
  for (int i = 0; i < NUM_LOOPS; ++i) {
    switch (threadID) {
    case 1:
      lk1.lock();
      while (!v1.size()) {
        c1.wait(lk1);
      }
      val = v1.back();
      v1.pop_back();
      lk1.unlock();
      break;
    case 2:
      lk2.lock();
      while (!v2.size()) {
        c2.wait(lk2);
      }
      val = v2.back();
      v2.pop_back();
      lk2.unlock();
      break;
    }
  }
}

int main() {
  timespec tpBegin1, tpEnd1;

  clock_gettime(CLOCK_REALTIME, &tpBegin1);
  for (int i = 0; i < NUM_RUNS; i++) {
    std::thread thread1{producer, 1};
    std::thread thread3{consumer, 1};

    std::thread thread2{producer, 2};
    std::thread thread4{consumer, 2};
    thread1.join();
    thread2.join();
    thread3.join();
    thread4.join();
  }
  clock_gettime(CLOCK_REALTIME, &tpEnd1);

  auto time1 = compute(tpBegin1, tpEnd1);

  printf("Average time taken over %d runs with false sharing      : %f ms\n",
         NUM_RUNS, time1 / NUM_RUNS);

  return 0;
}
