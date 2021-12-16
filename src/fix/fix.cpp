///// LLVM analysis pass to mitigate false sharing based on profiling data /////
#include "llvm/ADT/Optional.h"
#include "llvm/IR/Constants.h"
#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <fstream>
#include <string>
#include <unordered_map>
#include <set>
#include <vector>

using namespace llvm;

// Whether to enable struct padding. Struct padding is potentially unstable.
static const bool enableStructPadding = true;

// This must be a power of 2 for other code to work.
static const size_t cacheLineSize = 64; // in bytes

namespace {
struct CacheLineEntry {
  std::string variableName;
  size_t accessOffsetInVariable; // The offset of the access within this variable, in bytes.
  size_t accessSize;             // The size of the read/write, in bytes.
};
}

namespace {
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
struct PaddedStruct {
  StructType *type;
  std::unordered_map<unsigned int, unsigned int> newElementByOldElement;

  PaddedStruct(
    Module &M,
    const StructType *oldType,
    const StructLayout *oldLayout,
    const std::set<unsigned int> &conflictingElements
  ) {

    SmallVector<Type *> newTypes;
    size_t paddingBytesSoFar = 0;
    auto *int8Ty = Type::getInt8Ty(M.getContext());
    for (unsigned int i = 0; i < oldType->getNumElements(); ++i) {
      size_t alignSoFar = oldLayout->getElementOffset(i) + paddingBytesSoFar;
      if (conflictingElements.count(i) > 0 && alignSoFar % cacheLineSize != 0) {
        // We need to add padding to align this to a cache line boundary.
        size_t paddingBytes = (alignSoFar / cacheLineSize + 1) * cacheLineSize - alignSoFar;
        newTypes.push_back(ArrayType::get(int8Ty, paddingBytes));
        alignSoFar += paddingBytes;
        paddingBytesSoFar += paddingBytes;
        assert(alignSoFar % cacheLineSize == 0);
      }
      newTypes.push_back(oldType->getElementType(i));
      newElementByOldElement.emplace(i, static_cast<unsigned int>(newTypes.size() - 1));
    }

    if (oldType->hasName()) {
      type = StructType::create(newTypes, oldType->getName());
    } else {
      type = StructType::create(newTypes);
    }
  }
};
}

static bool fixGlobalStruct(Module &M, GlobalVariable *globalVar, PaddedStruct &padded) {
  for (auto *user : globalVar->users()) {
    if (auto *gepInst = dyn_cast<GetElementPtrInst>(user)) {
      if (gepInst->getNumIndices() < 2) {
        errs() << "Unable to pad struct - used in array-style GetElementPtr instruction\n";
        return false;
      }
      if (gepInst->getPointerOperand() != globalVar) {
        errs() << "Unable to pad struct - used in unfixable GetElementPtr instruction\n";
        return false;
      }
      auto *index = gepInst->getOperand(2); // operand 2 = GEP index 1 = the struct member
      if (!isa<ConstantInt>(index)) {
        errs() << "Unable to pad struct - non-constant GetElementPtr index\n";
        return false;
      }
    } else if (auto *constExpr = dyn_cast<ConstantExpr>(user)) {
      switch (constExpr->getOpcode()) {
        case Instruction::GetElementPtr: {
          if (constExpr->getNumOperands() < 3) {
            errs() << "Unable to pad struct - used in array-style GetElementPtr constant expression\n";
            return false;
          }
          if (constExpr->getOperand(0) != globalVar) {
            errs() << "Unable to pad struct - used in unfixable GetElementPtr constant expression\n";
            return false;
          }
          auto *index = constExpr->getOperand(2); // operand 2 = GEP index 1 = the struct member
          if (!isa<ConstantInt>(index)) {
            errs() << "Unable to pad struct - non-constant GetElementPtr index\n";
            return false;
          }
          break;
        }

        default:
          errs() << "Unable to pad struct - used in unfixable constant expression\n";
          return false;
      }
    } else {
      errs() << "Unable to pad struct - used in unfixable instruction\n";
      return false;
    }
  }

  auto *int8Ty = Type::getInt8Ty(globalVar->getContext());

  Constant *initializer = nullptr;
  if (auto *structInit = dyn_cast<ConstantStruct>(globalVar->getInitializer())) {
    SmallVector<Constant *> fields;
    fields.resize(padded.type->getNumElements(), nullptr);
    for (auto &pair : padded.newElementByOldElement) {
      unsigned int oldElement = pair.first;
      unsigned int newElement = pair.second;
      fields[newElement] = structInit->getOperand(oldElement);
    }
    for (unsigned int i = 0; i < fields.size(); ++i) {
      if (!fields[i]) {
        fields[i] = UndefValue::get(padded.type->getElementType(i));
      }
    }
    initializer = ConstantStruct::get(padded.type, fields);
  } else if (auto *zeroInit = dyn_cast<ConstantAggregateZero>(globalVar->getInitializer())) {
    initializer = ConstantAggregateZero::get(padded.type);
  } else if (auto *poisonInit = dyn_cast<PoisonValue>(globalVar->getInitializer())) {
    initializer = PoisonValue::get(padded.type);
  } else if (auto *undefInit = dyn_cast<UndefValue>(globalVar->getInitializer())) {
    initializer = UndefValue::get(padded.type);
  } else if (globalVar->getInitializer() != nullptr) {
    errs() << "Unable to pad struct - unknown initializer format\n";
    return false;
  }
  
  errs() << "Replacing " << globalVar->getName() << " with new, padded global\n";

  auto *newGlobalVar = new GlobalVariable(
    M,
    padded.type,
    globalVar->isConstant(),
    globalVar->getLinkage(),
    initializer,
    globalVar->getName(),
    nullptr,
    globalVar->getThreadLocalMode(),
    globalVar->getAddressSpace(),
    globalVar->isExternallyInitialized());
  newGlobalVar->setAlignment(Align(cacheLineSize));

  auto *int32Ty = Type::getInt32Ty(globalVar->getContext());
  SmallVector<Instruction *> toErase;
  for (auto *user : globalVar->users()) {
    if (auto *gepInst = dyn_cast<GetElementPtrInst>(user)) {
      SmallVector<Value *> newIndices;
      size_t i = 0;
      for (auto &indexUse : gepInst->indices()) {
        if (i == 1) { // The index that might need to change
          unsigned int oldElement = static_cast<unsigned int>(
            cast<ConstantInt>(indexUse)->getZExtValue());
          unsigned int newElement = padded.newElementByOldElement[oldElement];
          auto *newConst = ConstantInt::get(int32Ty, newElement);
          newIndices.push_back(newConst);
        } else {
          newIndices.push_back(indexUse);
        }
        ++i;
      }
      auto *newInst = GetElementPtrInst::Create(padded.type, newGlobalVar, newIndices);
      newInst->setIsInBounds(gepInst->isInBounds());
      gepInst->replaceAllUsesWith(newInst);
      toErase.push_back(gepInst);
    } else if (auto *constExpr = dyn_cast<ConstantExpr>(user)) {
      SmallVector<Constant *> operands;
      for (auto &use : constExpr->operands()) {
        operands.push_back(cast<Constant>(use));
      }
      assert(operands.size() >= 3);
      operands[0] = newGlobalVar;
      unsigned int oldElement = static_cast<unsigned int>(
        cast<ConstantInt>(operands[2])->getZExtValue());
      unsigned int newElement = padded.newElementByOldElement[oldElement];
      auto *newConst = ConstantInt::get(int32Ty, newElement);
      operands[2] = newConst;

      auto *newConstExpr = constExpr->getWithOperands(operands, padded.type, false, padded.type);
      constExpr->replaceAllUsesWith(newConstExpr);
    } else {
      assert(false && "Unexpected user");
    }
  }
  for (auto *inst : toErase) {
    inst->eraseFromParent();
  }
  globalVar->eraseFromParent();

  return true;
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
    std::sort(conflicts.begin(), conflicts.end(), [](auto &c1, auto &c2) {
      return c1.priority > c2.priority;
    });

    auto &dataLayout = M.getDataLayout();

    std::unordered_map<StructType *, std::unordered_map<GlobalVariable *, std::set<size_t>>> structAccesses;

    Optional<uint64_t> priorityThreshold;

    for (auto &conflict : conflicts) {
      if (!priorityThreshold) {
        priorityThreshold = conflict.priority / 1000;
      }
      if (conflict.priority < *priorityThreshold) {
        break;
      }
      auto *global1 = M.getGlobalVariable(conflict.entry1.variableName, true);
      auto *global2 = M.getGlobalVariable(conflict.entry2.variableName, true);
      if (!global1) {
        errs() << "Did not find global with name " << conflict.entry1.variableName << '\n';
        continue;
      }
      if (!global2) {
        errs() << "Did not find global with name " << conflict.entry2.variableName << '\n';
        continue;
      }
      if (conflict.entry1.variableName == conflict.entry2.variableName) {
        if (auto *type = dyn_cast<StructType>(global1->getValueType())) {
          if (enableStructPadding && GlobalValue::isLocalLinkage(global1->getLinkage())) {
            auto &set = structAccesses[type][global1];
            set.insert(conflict.entry1.accessOffsetInVariable);
            set.insert(conflict.entry2.accessOffsetInVariable);
          }
        }
      } else {
        errs() << "Aligning " << conflict.entry1.variableName << " to cache boundary\n";
        global1->setAlignment(Align(cacheLineSize));
        errs() << "Aligning " << conflict.entry2.variableName << " to cache boundary\n";
        global2->setAlignment(Align(cacheLineSize));
        changed = true;
      }
    }

    std::unordered_map<std::string, PaddedStruct> replacements;
    for (auto &pair : structAccesses) {
      auto *type = pair.first;
      for (auto &pair2 : pair.second) {
        auto *globalVar = pair2.first;
        auto *layout = dataLayout.getStructLayout(type);
        std::set<unsigned int> conflictingElements;
        for (size_t offset : pair2.second) {
          conflictingElements.insert(layout->getElementContainingOffset(offset));
        }
        if (conflictingElements.size() > 1) {
          errs() << "Found struct " << globalVar->getName() << " with false sharing in elements ";
          for (auto idx : conflictingElements) {
            errs() << idx << ' ';
          }
          errs() << '\n';
          PaddedStruct padded(M, type, layout, conflictingElements);
          changed = fixGlobalStruct(M, globalVar, padded) || changed;
        }
      }
    }
    return changed;
  }
}; // end of struct Fix583
}  // end of anonymous namespace

char Fix583::ID = 0;
const std::string Fix583::inputFile = "mapped_conflicts.out";
static RegisterPass<Fix583> X("false-sharing-fix", "Pass to fix false sharing",
                              false /* Only looks at CFG */,
                              false /* Analysis Pass */);
