#pragma once

#include <cstdint>
#include <string>

class InterferenceDetector {
public:
    InterferenceDetector(uint64_t cacheline_size_in);

    void recordAccess(const std::string& rw, const std::string& destAddr, 
        const std::string& accessSize, const std::string& threadId);

    void outputInterferences();

private:
    uint64_t cacheline_size;

};