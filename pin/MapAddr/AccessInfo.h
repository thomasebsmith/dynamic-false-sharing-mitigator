#pragma once

#include <string>

struct global_var {
  std::string name;
  uint64_t start_addr;
  size_t size;
};

struct memory_access {
  std::string name;
  uint64_t accessOffset;
  uint64_t accessSize;
};

struct conflicting_access {
  memory_access var1;
  memory_access var2;
  uint64_t priority;
};

struct conflicting_addr {
  uint64_t addr1;
  uint64_t addr2;
};

template <> struct std::hash<conflicting_addr> {
  std::size_t operator()(conflicting_addr const &ca) const noexcept {
    std::size_t h1 = std::hash<uint64_t>{}(ca.addr1);
    std::size_t h2 = std::hash<uint64_t>{}(ca.addr2);
    return h1 ^ h2; // or use boost::hash_combine
  }
};

bool operator==(const conflicting_addr &left, const conflicting_addr &right);

bool operator<(const global_var &left, const global_var &right);

