#include "InterferenceDetector.h"

#include <iostream>

constexpr int HEX_BASE = 16;

InterferenceDetector::InterferenceDetector(uint64_t cacheline_size_in) :
    cacheline_size(cacheline_size_in) 
{}

void InterferenceDetector::recordAccess(int linenum, const std::string& rw, const std::string& destAddr, 
    const std::string& accessSize, const std::string& threadId) 
{
    /* 
    parse destAddr -> numerical addr
    map from numerical addr -> Cache Line
        CacheLine:
            tid, r/w, addresses accessed
    */
   uint64_t destAddrNum;
   try {
        destAddrNum = std::stoull(destAddr, nullptr, HEX_BASE);
   } catch (...) {
       std::cout << "Could not parse address " << destAddr << " (line " 
                 << linenum << ")" << std::endl;
       return;
   }
   std::cout << "Parsed " << destAddr << " as " << destAddrNum << std::endl;
}

void InterferenceDetector::outputInterferences() 
{

}
