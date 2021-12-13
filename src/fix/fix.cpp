///// LLVM analysis pass to mitigate false sharing based on profiling data /////
#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

using namespace llvm;

namespace {
// TODO: Do we want a priority here?
// Right now, the pass realigns all falsely shared data.
// That might add a lot of padding.
struct CacheLineEntry {
  std::string variableName;
  size_t accessOffsetInVariable; // The offset of the access within this variable, in bytes.
  size_t accessSize;             // The size of the read/write, in bytes.
};

// Represents a pair of memory locations that were accessed by different CPUs
// but resided on the same cache line during profiling.
struct Conflict {
  CacheLineEntry entry1;
  CacheLineEntry entry2;
  uint64_t priority;
};
std::istream &operator>>(std::istream &in, CacheLineEntry &entry) {
  return in >> entry.variableName >> entry.accessOffsetInVariable >> entry.accessSize;
}
std::istream &operator>>(std::istream &in, Conflict &conflict) {
  return in >> conflict.entry1 >> conflict.entry2 >> conflict.priority;
}

struct Fix583 : public ModulePass {
  static char ID;

  // TODO: Come up with a better way of taking input.
  // Maybe consider doing something similar to profiling passes.
  static const std::string inputFile;

  // TODO: Is this accurate?
  // Also, make sure this is a power of 2.
  static const size_t cacheLineSize = 64; // in bytes

  std::vector<Conflict> getPotentialFS() {
    std::ifstream in(inputFile);

    std::vector<Conflict> conflicts;
    Conflict conflict;
    while (in >> conflict) {
      conflicts.push_back(std::move(conflict));
    }

    return conflicts;
  }

  Fix583() : ModulePass(ID) {}

  bool runOnModule(Module &M) override {
    bool changed = false;
    auto conflicts = getPotentialFS();
    for (auto &conflict : conflicts) {
      auto *global1 = M.getGlobalVariable(conflict.entry1.variableName, true);
      auto *global2 = M.getGlobalVariable(conflict.entry2.variableName, true);
      if (!global1 || !global2) {
        continue;
      }
      if (conflict.entry1.variableName == conflict.entry2.variableName) {
        // TODO: Check if this is a struct that we can pad.
        // Otherwise, don't do anything.
        // If this is a struct:
        //  - Find out if accesses are to different members
        //  - If they are, align each member to its own cache line
        auto *type = global1->getValueType();
        if (type->isStructTy()) {
          // ...
          changed = true;
        }
      } else {
        // TODO: Can this be more efficient?
        global1->setAlignment(Align(cacheLineSize));
        global2->setAlignment(Align(cacheLineSize));
        changed = true;
      }
    }
    return changed;
  }
}; // end of struct Hello
}  // end of anonymous namespace

char Fix583::ID = 0;
const std::string Fix583::inputFile = "fs_conflicts.txt";
static RegisterPass<Fix583> X("false-sharing-fix", "Pass to fix false sharing",
                              false /* Only looks at CFG */,
                              false /* Analysis Pass */);