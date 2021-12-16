#pragma once

#include "../MapAddr/AccessInfo.h"

#include <cstdint>
#include <ostream>
#include <set>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

uint64_t string_to_uint64(const std::string &str, int base = 10);

class InterferenceDetector {
public:
  InterferenceDetector(uint64_t cacheline_size_in);

  void recordAccess(const std::string &rw, const std::string &destAddr,
                    const std::string &accessSize, const std::string &threadId);

  void outputInterferences(std::ostream &out);

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

  // interference -> count
  std::unordered_map<conflicting_addr, uint64_t> interferences;
};