#include "../detect/InterferenceDetector.h"
#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

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

bool operator==(const conflicting_addr &left, const conflicting_addr &right) {
  return (left.addr1 == right.addr1 && left.addr2 == right.addr2) ||
         (left.addr1 == right.addr2 && left.addr2 == right.addr1);
}

bool operator<(const global_var &left, const global_var &right) {
  return left.start_addr < right.start_addr;
}

// <name, accessOffsetInVar, accessSize>
memory_access addr_to_named_access(uint64_t addr,
                                   const std::vector<global_var> &global_vars) {
  auto search_val = global_var{"", addr, 0};
  auto it =
      std::lower_bound(global_vars.begin(), global_vars.end(), search_val);

  if (it == global_vars.end()) {
    assert(global_vars.size() > 0);
    it = global_vars.begin() + global_vars.size() - 1;
  } else if (it->start_addr == addr) {
    // no offset
  } else if (it != global_vars.begin()) {
    assert(it->start_addr > addr);
    it--;
  }

  if (addr >= it->start_addr &&
      addr < it->start_addr + it->size) { // TODO: Check overflow?
    return {it->name, addr - it->start_addr, 1};
  }
  return {"", 0, 0};

  // TODO: we always assume access of size 1 byte
  // return {it->name, addr - it->start_addr, 1};
}

int main(int argc, char **argv) {
  unordered_map<conflicting_addr, conflicting_access> priority_cache;
  std::vector<global_var> global_vars;
  std::string outfile("mapped_conflicts.out");
  ofstream out(outfile);

  if (argc != 4) {
    std::cerr << "Usage: " << argv[0]
              << " [path to mdcache.out] [path to *.interferences] [path to "
                 "fs_globals.txt]"
              << std::endl;
    exit(1);
  }

  ifstream realized_conflicting_addrs(argv[1]);
  ifstream potential_conflicting_addrs(argv[2]);
  ifstream global_addresses(argv[3]);
  std::string addr1, addr2;
  int64_t priority = 1;

  std::string global_addr;
  std::string global_name;
  size_t size;
  while (global_addresses >> global_name >> global_addr >> size) {
    auto addr = string_to_uint64(global_addr, 16);
    auto el = global_var{global_name, addr, size};
    global_vars.push_back(el);
  }
  std::sort(global_vars.begin(), global_vars.end());
  printf("done sorting\n");

  //   while (realized_conflicting_addrs >> addr1 >> addr2 >> priority) {
  // priority = some value
  //   }

  priority = 1;
  while (potential_conflicting_addrs >> addr1 >> addr2) {
    auto realaddr1 = string_to_uint64(addr1, 16);
    auto realaddr2 = string_to_uint64(addr2, 16);
    conflicting_addr addrs = {realaddr1, realaddr2};
    conflicting_access ca;
    ca.priority = priority;
    ca.var1 = addr_to_named_access(realaddr1, global_vars);
    ca.var2 = addr_to_named_access(realaddr2, global_vars);
    if (ca.var1.name.empty() || ca.var2.name.empty()) {
      continue;
    }
    if (priority_cache.count(addrs)) {
      priority_cache[addrs].priority += 1;
    } else {
      priority_cache[addrs] = ca;
    }
  }

  for (auto &ca : priority_cache) {
    auto &ma1 = ca.second.var1;
    auto &ma2 = ca.second.var2;
    out << std::hex << ma1.name << " " << ma1.accessOffset << " "
        << ma1.accessSize << " " << ma2.name << " " << ma2.accessOffset << " "
        << ma2.accessSize << " " << ca.second.priority << std::endl;
  }
}
