///// LLVM analysis pass to mitigate false sharing based on profiling data /////
#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/Support/raw_ostream.h"
#include <cassert>
#include <cstdint>
#include <fstream>
#include <string>
#include <unordered_map>
#include <set>
#include <vector>

using namespace llvm;

// TODO: Is this accurate?
// Also, make sure this is a power of 2.
static const size_t cacheLineSize = 64; // in bytes

// TODO: This is a hack
static bool isLocalToModule(StructType *type) {
  return type->hasName() && type->getName().contains("(anonymous namespace)");
}

namespace {
// TODO: Do we want a priority here?
// Right now, the pass realigns all falsely shared data.
// That might add a lot of padding.
struct CacheLineEntry {
  std::string variableName;
  size_t accessOffsetInVariable; // The offset of the access within this variable, in bytes.
  size_t accessSize;             // The size of the read/write, in bytes.
};
}

namespace{
// Represents a pair of memory locations that were accessed by different CPUs
// but resided on the same cache line during profiling.
struct Conflict {
  CacheLineEntry entry1;
  CacheLineEntry entry2;
  uint64_t priority;
};
}

std::istream &operator>>(std::istream &in, CacheLineEntry &entry) {
  return in >> entry.variableName >> entry.accessOffsetInVariable >> entry.accessSize;
}
std::istream &operator>>(std::istream &in, Conflict &conflict) {
  return in >> conflict.entry1 >> conflict.entry2 >> conflict.priority;
}

namespace {
struct PaddedAggregate {
  Type *type;
  std::unordered_map<unsigned int, unsigned int> newElementByOldElement;
};
}

static PaddedAggregate createPaddedStructType(
  Module &M,
  const StructType *oldType,
  const StructLayout *oldLayout,
  const std::set<unsigned int> &conflictingElements
) {
  assert(oldType->hasName());
  SmallVector<Type *> newTypes;
  PaddedAggregate padded{};
  size_t paddingBytesSoFar = 0;
  auto *int8Ty = Type::getInt8Ty(M.getContext());
  for (unsigned int i = 0; i < oldType->getNumElements(); ++i) {
    size_t alignSoFar = oldLayout->getElementOffset(i) + paddingBytesSoFar;
    if (conflictingElements.count(i) > 0 && alignSoFar % cacheLineSize != 0) {
      // We need to add padding to align this to a cache line boundary.
      size_t paddingBytes = (alignSoFar / cacheLineSize + 1) * cacheLineSize - alignSoFar;
      newTypes.push_back(ArrayType::get(int8Ty, paddingBytes));
      alignSoFar += paddingBytes;
      assert(alignSoFar % cacheLineSize == 0);
    }
    newTypes.push_back(oldType->getElementType(i));
    padded.newElementByOldElement.emplace(i, static_cast<unsigned int>(newTypes.size() - 1));
  }

  auto newName = oldType->getName().str() + ".(583padded)";
  padded.type = StructType::create(newTypes, newName);
  return padded;
}

static void replaceTypes(
  Module &M,
  std::unordered_map<std::string, PaddedAggregate> &replacements
) {
  // Rewrite all uses of key types to use value types instead.
  // Need to fix the following instruction types (at least):
  //  - extractvalue
  //  - insertvalue
  //  - alloca
  //  - getelementptr
  // Also, need to fix:
  //  - Pointers to this type (?)
  //  - Other structs/arrays that contain this struct type
  //  - Function arguments
  //  - Global variables

  /*
  Algorithm:
   (1) Create map (Type) -> (Types that contain Type (pointers, structs, arrays, ...)).
   (2) Use map from (1) to create derived types that use newTypes instead of oldTypes.
   (3) Create map (Type) -> (Insts that use Type).
   (4) Use map from (3) to replace oldTypes with newTypes in instructions.
   (5) Go through all functions and fix argument types/return types to use newTypes.
   (6) Change all global variables to use newTypes instead of oldTypes.
  */
}

namespace{
struct Fix583 : public ModulePass {
  static char ID;

  // TODO: Come up with a better way of taking input.
  // Maybe consider doing something similar to profiling passes.
  static const std::string inputFile;


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
    std::unordered_map<std::string, std::set<size_t>> structAccesses;
    for (auto &conflict : conflicts) {
      auto *global1 = M.getGlobalVariable(conflict.entry1.variableName, true);
      auto *global2 = M.getGlobalVariable(conflict.entry2.variableName, true);
      if (!global1 || !global2) {
        continue;
      }
      if (conflict.entry1.variableName == conflict.entry2.variableName) {
        if (auto *type = dyn_cast<StructType>(global1->getValueType())) {
          if (isLocalToModule(type) && type->hasName() && !type->isPacked()) {
            auto &set = structAccesses[std::string(type->getName())];
            set.insert(conflict.entry1.accessOffsetInVariable);
            set.insert(conflict.entry2.accessOffsetInVariable);
          }
        }
      } else {
        // TODO: Is there a way to know which global comes later?
        // If so, we could just realign that one.
        global1->setAlignment(Align(cacheLineSize));
        global2->setAlignment(Align(cacheLineSize));
        changed = true;
      }
    }

    std::unordered_map<std::string, PaddedAggregate> replacements;
    auto &dataLayout = M.getDataLayout();
    for (auto &pair : structAccesses) {
      auto *type = StructType::getTypeByName(M.getContext(), pair.first);
      assert(type != nullptr);
      auto *layout = dataLayout.getStructLayout(type);
      std::set<unsigned int> conflictingElements;
      for (size_t offset : pair.second) {
        conflictingElements.insert(layout->getElementContainingOffset(offset));
      }
      if (conflictingElements.size() > 1) {
        replacements.emplace(type->getName(), createPaddedStructType(M, type, layout, conflictingElements));
      }
    }
    if (replacements.size() > 0) {
      replaceTypes(M, replacements);
      changed = true;
    }
    return changed;
  }
}; // end of struct Fix583
}  // end of anonymous namespace

char Fix583::ID = 0;
const std::string Fix583::inputFile = "mapped_conflicts.txt";
static RegisterPass<Fix583> X("false-sharing-fix", "Pass to fix false sharing",
                              false /* Only looks at CFG */,
                              false /* Analysis Pass */);