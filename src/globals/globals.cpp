//// LLVM pass to output address of all global variables ////
#include "llvm/ADT/SmallVector.h"
#include "llvm/Pass.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"
#include "llvm/IR/DataLayout.h"

using namespace llvm;

namespace {

struct Globals583 : public ModulePass {
  static char ID;
  Globals583() : ModulePass(ID) {}

  bool runOnModule(Module &M) override {
    auto &context = M.getContext();
    IRBuilder<> builder(context);

    // def __ctor583():
    auto *ctor = Function::Create(
      FunctionType::get(builder.getVoidTy(), false),
      Function::InternalLinkage,
      "__ctor583",
      M
    );

    auto *ctorBB = BasicBlock::Create(context, "initHandle", ctor);
    builder.SetInsertPoint(ctorBB);

    // FILE *fileHandle = fopen("./fs_globals.txt", "a");
    auto *fopenType = FunctionType::get(
      builder.getInt8PtrTy(),
      SmallVector<Type *>{builder.getInt8PtrTy(), builder.getInt8PtrTy()},
      false
    );
    auto fopenFunc = M.getOrInsertFunction("fopen", fopenType);
    auto *fileHandle = builder.CreateCall(fopenFunc, SmallVector<Value *>{
      builder.CreateGlobalStringPtr("./fs_globals.txt"),
      builder.CreateGlobalStringPtr("a")
    });
    
    // Printed lines will be in the format "name<tab>address<tab>size"
    auto *fprintfType = FunctionType::get(
      builder.getInt32Ty(),
      SmallVector<Type *>{builder.getInt8PtrTy()},
      true
    );
    auto fprintfFunc = M.getOrInsertFunction("fprintf", fprintfType);

    DataLayout dataLayout(&M);

    // We need to collect all globals before adding to them to avoid an infinite
    // loop.
    SmallVector<GlobalVariable *> globals;
    for (auto &global : M.globals()) {
      // Skip thread local variables
      if (!global.isThreadLocal() &&
          !global.getName().startswith("llvm.") && // used internally, e.g. llvm.global_ctors
          !global.getName().empty()) {
        globals.push_back(&global);
      }
    }

    for (auto *global : globals) {
      // fprintf("%s\t%p\t%lld\n", name, address, size);
      auto *name = builder.CreateGlobalStringPtr(global->getName());
      auto *size = ConstantInt::get(
        builder.getInt64Ty(),
        dataLayout.getTypeSizeInBits(global->getValueType()).getFixedSize() / 8
      );
      builder.CreateCall(fprintfFunc, SmallVector<Value *>{
        fileHandle,
        builder.CreateGlobalStringPtr("%s\t%p\t%lld\n"),
        name,
        global,
        size
      });
    }

    // fclose(fileHandle);
    auto *fcloseType = FunctionType::get(
      builder.getInt8PtrTy(),
      SmallVector<Type *>{builder.getInt8PtrTy()},
      false
    );
    auto fcloseFunc = M.getOrInsertFunction("fclose", fcloseType);
    builder.CreateCall(fcloseFunc, SmallVector<Value *>{fileHandle});

    builder.CreateRetVoid();
    appendToGlobalCtors(M, ctor, 0);

    return true;
  }
}; // end of struct Globals583

}  // end of anonymous namespace

char Globals583::ID = 0;
static RegisterPass<Globals583> X("false-sharing-globals",
                                  "Pass to output information about global variables",
                                   false /* Only looks at CFG */,
                                   false /* Analysis Pass */);
