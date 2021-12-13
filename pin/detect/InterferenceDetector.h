#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <tuple>
#include <set>
#include <ostream>

class InterferenceDetector {
public:
    InterferenceDetector(uint64_t cacheline_size_in);

    void recordAccess(const std::string& rw, const std::string& destAddr, 
        const std::string& accessSize, const std::string& threadId);

    void outputInterferences(std::ostream& out);

private:
    uint64_t cacheline_size;

    struct CacheLine {
        struct Access {
            bool isWrite;
            uint64_t accessSize;
        };
        // thread id -> destAddr -> Access
        std::unordered_map<uint64_t, std::unordered_map<uint64_t, Access>> accesses;
    };
    std::unordered_map<uint64_t, CacheLine> cachelines;

    std::vector<std::tuple<uint64_t, uint64_t>> interferences;
};