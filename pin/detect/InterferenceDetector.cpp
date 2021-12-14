#include "InterferenceDetector.h"

#include <iostream>

constexpr int HEX_BASE = 16;

// Convert a std::string to a uint64_t using stoull
uint64_t string_to_uint64(const std::string &str, int base) {
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
bool string_to_rw(const std::string &rw) {
  if (rw == "W" || rw == "w") {
    return true;
  }
  if (rw == "R" || rw == "r") {
    return false;
  }
  throw std::runtime_error("Invalid rw value: " + rw);
}

InterferenceDetector::InterferenceDetector(uint64_t cacheline_size_in)
    : cacheline_size(cacheline_size_in) {}

void InterferenceDetector::recordAccess(const std::string &rw,
                                        const std::string &destAddr,
                                        const std::string &accessSize,
                                        const std::string &threadId) {
  bool isWrite = string_to_rw(rw);
  uint64_t destAddrNum = string_to_uint64(destAddr, HEX_BASE);
  uint64_t accessSizeNum = string_to_uint64(accessSize);
  uint64_t threadIdNum = string_to_uint64(threadId);

  uint64_t cacheline_index = destAddrNum / cacheline_size;
  CacheLine &cacheline = cachelines[cacheline_index];
  cacheline.accesses[threadIdNum];
  for (auto &threadAccesses : cacheline.accesses) {
    if (threadAccesses.first == threadIdNum) {
      auto access_it = threadAccesses.second.emplace(
          destAddrNum, CacheLine::Access{isWrite, accessSizeNum});
      if (!access_it.second) {
        // Mark as write if it wasn't before. TODO: Might react to this.
        access_it.first->second.isWrite =
            (access_it.first->second.isWrite) || isWrite;
        // Access already recorded
        if (access_it.first->second.accessSize >= accessSizeNum) {
          return;
        } else {
          access_it.first->second.accessSize = accessSizeNum;
        }
      }
      // std::cout << std::hex << "Recorded access to " << destAddrNum << " of
      // size " << accessSizeNum
      //           << " for thread " << threadIdNum << " in cacheline " <<
      //           cacheline_index << std::endl;
      continue;
    }
    for (const auto &access : threadAccesses.second) {
      // If the access ranges overlap, it is not false sharing
      if ((access.first >= destAddrNum &&
           access.first < destAddrNum + accessSizeNum) ||
          (destAddrNum >= access.first &&
           destAddrNum < access.first + access.second.accessSize)) {
        continue;
      }
      if (!isWrite && !access.second.isWrite) {
        continue; // don't mark as interference is both accesses are reads
      }
      interferences.push_back({access.first, destAddrNum});
    }
  }
}

void InterferenceDetector::outputInterferences(std::ostream &out) {
  std::cout << "Number of interferences: " << interferences.size() << std::endl;
  for (const auto &interference : interferences) {
    out << std::hex << std::get<0>(interference) << "\t"
        << std::get<1>(interference) << std::endl;
  }
}
