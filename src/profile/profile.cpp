//// LLVM analysis pass to output information about potential false sharing ////
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

struct Profile583 : public ModulePass {
  static char ID;
  Profile583() : ModulePass(ID) {}

  void runOnInstruction(const Instruction &inst) {
    // TODO: Add call to __print583 right after inst 
  }

  void runOnBasicBlock(const BasicBlock &bb) {
    SmallVector<const Instruction *> loadsAndStores;
    for (const auto &inst : bb) {
      switch (inst.getOpcode()) {
        case Instruction::Load:
        case Instruction::Store:
          // TODO: Filter out stack variables
          loadsAndStores.push_back(&inst);
          break;
      }
    }

    for (const auto *inst : loadsAndStores) {
      runOnInstruction(*inst);
    }
  }

  void runOnFunction(Function &F) {
    for (const auto &bb: F) {
      runOnBasicBlock(bb);
    }
  }

  // Adds a function of the form __print583(int8_t *addr, bool isStore)
  // Also adds a global file handle and global ctors/dtors for opening/closing it.
  Function *addProfileFunction(Module &M) {
    auto &context = M.getContext();
    IRBuilder<> builder(context);

    auto *fileHandle = new GlobalVariable(
      builder.getInt8PtrTy(),
      false,
      GlobalValue::InternalLinkage,
      nullptr,
      "__handle583"
    );
    auto *ctor = Function::Create(
      FunctionType::get(builder.getVoidTy(), false),
      Function::InternalLinkage,
      "__ctor583",
      M
    );
    auto *ctorBB = BasicBlock::Create(context, "initHandle", ctor);
    builder.SetInsertPoint(ctorBB);
    auto *fopenType = FunctionType::get(
      builder.getInt8PtrTy(),
      SmallVector<Type *>{builder.getInt8PtrTy(), builder.getInt8PtrTy()},
      false
    );
    auto *fopenFunc = Function::Create(fopenType, Function::ExternalLinkage, "__fopen583", M);
    auto *fopenResult = builder.CreateCall(fopenFunc, SmallVector<Value *>{
      builder.CreateGlobalStringPtr("./fs_profile.txt"),
      builder.CreateGlobalStringPtr("a")
    });
    builder.CreateStore(fopenResult, fileHandle);
    appendToGlobalCtors(M, ctor, 0);
    
    auto *dtor = Function::Create(
      FunctionType::get(builder.getVoidTy(), false),
      Function::InternalLinkage,
      "__dtor583",
      M
    );
    auto *dtorBB = BasicBlock::Create(context, "closeHandle", dtor);
    builder.SetInsertPoint(dtorBB);
    auto *fcloseType = FunctionType::get(
      builder.getInt8PtrTy(),
      SmallVector<Type *>{builder.getInt8PtrTy()},
      false
    );
    auto *fcloseFunc = Function::Create(fcloseType, Function::ExternalLinkage, "__fclose583", M);
    builder.CreateCall(fcloseFunc, SmallVector<Value *>{
      builder.CreateLoad(fileHandle)
    });
    appendToGlobalDtors(M, dtor, 0);

    auto *type = FunctionType::get(
      builder.getVoidTy(),      // return type
      SmallVector<Type *>{
        builder.getInt8PtrTy(), // arg 0
        builder.getInt1Ty()     // arg 1
      },
      false                     // var args?
    );

    auto *func = Function::Create(type, Function::InternalLinkage, "__print583", M);
    func->getArg(0)->setName("addr");
    func->getArg(1)->setName("isStore");

    auto *bb = BasicBlock::Create(context, "printToFile", func);
    builder.SetInsertPoint(bb);

    // Create stack variables to call getcpu for retrieving the cpu id
    AllocaInst* cpu = builder.CreateAlloca(builder.getInt32Ty());
    AllocaInst* numa = builder.CreateAlloca(builder.getInt32Ty());
    DataLayout dataLayout = DataLayout(&M);
    auto *getcpuType = FunctionType::get(
      builder.getInt32Ty(),
      SmallVector<Type *>{builder.getIntPtrTy(dataLayout), builder.getIntPtrTy(dataLayout)},
      false
    );
    auto *getcpuFunc = Function::Create(getcpuType, Function::ExternalLinkage, "__getcpu583", M);
    builder.CreateCall(getcpuType, SmallVector<Value *>{
      cpu, numa
    });

    // Printed lines will be in the format "address<tab>isStore<tab>cpu"
    auto *fprintfType = FunctionType::get(
      builder.getInt32Ty(),
      SmallVector<Type *>{builder.getInt8PtrTy()},
      true
    );
    auto *fprintfFunc = Function::Create(fprintfType, Function::ExternalLinkage, M);
    builder.CreateCall(fprintfFunc, SmallVector<Value *>{
      builder.CreateLoad(fileHandle),
      builder.CreateGlobalStringPtr("%p\t%d\t%u\n"),
      func->getArg(0),
      func->getArg(1),
      cpu
    });

    return func;
  }

  bool runOnModule(Module &M) override {
    auto *print583Func = addProfileFunction(M);
    // TODO: call runOnFunction for each function
    return true;
  }
}; // end of struct Profile583

}  // end of anonymous namespace

char Profile583::ID = 0;
static RegisterPass<Profile583> X("false-sharing-profile",
                                  "Pass to profile false sharing",
                                   false /* Only looks at CFG */,
                                   false /* Analysis Pass */);
