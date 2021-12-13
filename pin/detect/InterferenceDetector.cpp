#include "InterferenceDetector.h"

#include <iostream>

constexpr int HEX_BASE = 16;

// Convert a std::string to a uint64_t using stoull
uint64_t string_to_uint64(const std::string& str, int base = 10) {
    uint64_t result;
    try {
        result = std::stoull(str, nullptr, base);
    } catch (...) {
        throw std::runtime_error("Could not convert string to uint64_t: " + str);
    }
    return result;
}

// Convert a std::string to a boolean representing whether the 
// string represents a read or a write
bool string_to_rw(const std::string& rw) {
    if (rw == "W" || rw == "w") {
        return true;
    }
    if (rw == "R" || rw == "r") {
        return false;
    }
    throw std::runtime_error("Invalid rw value: " + rw);
}

InterferenceDetector::InterferenceDetector(uint64_t cacheline_size_in) :
    cacheline_size(cacheline_size_in) 
{}

void InterferenceDetector::recordAccess(const std::string& rw, const std::string& destAddr, 
    const std::string& accessSize, const std::string& threadId) 
{
    bool isWrite = string_to_rw(rw);
    uint64_t destAddrNum = string_to_uint64(destAddr, HEX_BASE);
    uint64_t accessSizeNum = string_to_uint64(accessSize);
    uint64_t threadIdNum = string_to_uint64(threadId);

    uint64_t cacheline_index = destAddrNum / cacheline_size;
    CacheLine& cacheline = cachelines[cacheline_index];
    for (const auto& access : cacheline.accesses) {
        // Check for interferences
        // If the accesses are from the same thread, it is not an interference
        if (access.threadId == threadIdNum) {
            continue;
        }
        // If the access ranges overlap, it is not false sharing
        if (access.destAddr < destAddrNum + accessSizeNum ||
                destAddrNum < access.destAddr + access.accessSize) {
            continue;
        }
        interferences.push_back({access.destAddr, destAddrNum});
    }
    cacheline.accesses.push_back(
        CacheLine::Access{isWrite, destAddrNum, accessSizeNum, threadIdNum}
    );
}

void InterferenceDetector::outputInterferences() 
{

}
