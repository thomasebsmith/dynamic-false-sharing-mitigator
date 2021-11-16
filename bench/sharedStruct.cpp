#include <iostream>
#include <thread>

struct SharedData {
  volatile int thread1Data = 0;
  volatile int thread2Data = 0;
};

SharedData sharedStruct;

void runThread1() {
  for (int i = 0; i < 100000; ++i) {
    ++sharedStruct.thread1Data;
  }
}

void runThread2() {
  for (int i = 0; i < 50000; ++i) {
    std::cout << sharedStruct.thread2Data;
  }
}

int main() {
  std::thread thread1{runThread1};
  std::thread thread2{runThread2};
  thread1.join();
  thread2.join();
  return 0;
}
