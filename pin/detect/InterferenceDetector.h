#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <tuple>
#include <set>

class InterferenceDetector {
public:
    InterferenceDetector(uint64_t cacheline_size_in);

    void recordAccess(const std::string& rw, const std::string& destAddr, 
        const std::string& accessSize, const std::string& threadId);

    void outputInterferences();

private:
    uint64_t cacheline_size;

    struct CacheLine {
        struct Access {
            bool isWrite;
            // uint64_t destAddr;
            uint64_t accessSize;
            // uint64_t threadId;
        };
        // std::vector<Access> accesses;

        // thread id -> destAddr -> Access
        std::unordered_map<uint64_t, std::unordered_map<uint64_t, Access>> accesses;
    };
    std::unordered_map<uint64_t, CacheLine> cachelines;

    std::vector<std::tuple<uint64_t, uint64_t>> interferences;
};