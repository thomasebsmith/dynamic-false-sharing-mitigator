#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

const int NUM_RUNS = 10;
const int NUM_THREADS = 40;
const int NUM_LOOPS = 1000000;

namespace {
struct MyStruct {
    std::mutex m1;
    volatile unsigned char m1Data[NUM_THREADS];
};
}
MyStruct data{};

void run_thread(int thread_id) {
    while (!data.m1.try_lock()) {}
    for (int i = 0; i < NUM_LOOPS; ++i) {
        ++data.m1Data[thread_id];
    }
    data.m1.unlock();
}

int main() {
    for (int i = 0; i < NUM_RUNS; ++i) {
        std::vector<std::thread> threads;
        for (int i = 0; i < NUM_THREADS; ++i) {
            threads.emplace_back(run_thread, i);
        }
        for (auto &thread : threads) {
            thread.join();
        }
    }
    return data.m1Data[0] == 123 ? 1 : 0;
}