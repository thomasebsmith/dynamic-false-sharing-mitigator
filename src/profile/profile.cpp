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
#include <type_traits>
#include <utility>
#include <unordered_set>

using namespace llvm;

namespace {

struct Profile583 : public ModulePass {
  static char ID;
  Profile583() : ModulePass(ID) {}

  void runOnInstruction(Instruction *inst, Function *print583Func) {
    IRBuilder<> builder(inst);
    bool isStore = inst->getOpcode() == Instruction::Store;

    Value *pointerOperand;
    if (isStore) {
      pointerOperand = dyn_cast<StoreInst>(inst)->getPointerOperand();
    } else {
      pointerOperand = dyn_cast<LoadInst>(inst)->getPointerOperand(); 
    }
    auto *int8Ptr = builder.CreatePointerCast(pointerOperand, builder.getInt8PtrTy());

    builder.CreateCall(print583Func, SmallVector<Value *>{
      int8Ptr,
      ConstantInt::get(builder.getInt1Ty(), isStore)
    });
  }

  void runOnBasicBlock(BasicBlock &bb, Function *print583Func) {
    SmallVector<Instruction *> loadsAndStores;
    for (Instruction &inst : bb) {
      switch (inst.getOpcode()) {
        case Instruction::Load:
        case Instruction::Store:
          // TODO: Filter out stack variables
          loadsAndStores.push_back(&inst);
          break;
      }
    }

    for (auto *inst : loadsAndStores) {
      runOnInstruction(inst, print583Func);
    }
  }

  void runOnFunction(Function &F, Function *print583Func) {
    for (auto &bb: F) {
      runOnBasicBlock(bb, print583Func);
    }
  }

  // Adds a function of the form __print583(int8_t *addr, bool isStore)
  // Also adds a global file handle and global ctors/dtors for opening/closing it.
  std::pair<Function *, std::unordered_set<Function *>> addProfileFunction(Module &M) {
    auto &context = M.getContext();
    IRBuilder<> builder(context);

    std::unordered_set<Function *> allFuncs;

    auto *fileHandle = new GlobalVariable(
      M,
      builder.getInt8PtrTy(),
      false,
      GlobalValue::InternalLinkage,
      ConstantPointerNull::get(builder.getInt8PtrTy()),
      "__handle583"
    );
    auto *ctor = Function::Create(
      FunctionType::get(builder.getVoidTy(), false),
      Function::InternalLinkage,
      "__ctor583",
      M
    );
    allFuncs.insert(ctor);

    auto *ctorBB = BasicBlock::Create(context, "initHandle", ctor);
    builder.SetInsertPoint(ctorBB);
    auto *fopenType = FunctionType::get(
      builder.getInt8PtrTy(),
      SmallVector<Type *>{builder.getInt8PtrTy(), builder.getInt8PtrTy()},
      false
    );
    auto *fopenFunc = Function::Create(fopenType, Function::ExternalWeakLinkage, "fopen", M);
    auto *fopenResult = builder.CreateCall(fopenFunc, SmallVector<Value *>{
      builder.CreateGlobalStringPtr("./fs_profile.txt"),
      builder.CreateGlobalStringPtr("a")
    });
    builder.CreateStore(fopenResult, fileHandle);
    builder.CreateRetVoid();
    appendToGlobalCtors(M, ctor, 0);
    
    auto *dtor = Function::Create(
      FunctionType::get(builder.getVoidTy(), false),
      Function::InternalLinkage,
      "__dtor583",
      M
    );
    allFuncs.insert(dtor);
    auto *dtorBB = BasicBlock::Create(context, "closeHandle", dtor);
    builder.SetInsertPoint(dtorBB);
    auto *fcloseType = FunctionType::get(
      builder.getInt8PtrTy(),
      SmallVector<Type *>{builder.getInt8PtrTy()},
      false
    );
    auto *fcloseFunc = Function::Create(fcloseType, Function::ExternalWeakLinkage, "fclose", M);
    builder.CreateCall(fcloseFunc, SmallVector<Value *>{
      builder.CreateLoad(fileHandle)
    });
    builder.CreateRetVoid();
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
    allFuncs.insert(func);
    func->getArg(0)->setName("addr");
    func->getArg(1)->setName("isStore");

    auto *bb = BasicBlock::Create(context, "printToFile", func);
    builder.SetInsertPoint(bb);

    // Create stack variables to call getcpu for retrieving the cpu id
    AllocaInst* cpu = builder.CreateAlloca(builder.getInt32Ty());
    AllocaInst* numa = builder.CreateAlloca(builder.getInt32Ty());
    auto *int32PtrTy = PointerType::get(builder.getInt32Ty(), 0);
    auto *getcpuType = FunctionType::get(
      builder.getInt32Ty(),
      SmallVector<Type *>{int32PtrTy, int32PtrTy},
      false
    );
    //auto *getcpuFunc = Function::Create(getcpuType, Function::ExternalWeakLinkage, "getcpu", M);
    // builder.CreateCall(getcpuFunc, SmallVector<Value *>{
    //   cpu, numa
    // });

    // Printed lines will be in the format "address<tab>isStore<tab>cpu"
    auto *fprintfType = FunctionType::get(
      builder.getInt32Ty(),
      SmallVector<Type *>{builder.getInt8PtrTy()},
      true
    );
    auto *fprintfFunc = Function::Create(fprintfType, Function::ExternalWeakLinkage, "fprintf", M);
    builder.CreateCall(fprintfFunc, SmallVector<Value *>{
      builder.CreateLoad(fileHandle),
      builder.CreateGlobalStringPtr("%p\t%d\t%u\n"),
      func->getArg(0),
      func->getArg(1),
      cpu
    });
    builder.CreateRetVoid();

    errs() << "addProfileFunction returning without error\n";
    return {func, allFuncs};
  }

  bool runOnModule(Module &M) override {
    auto funcs = addProfileFunction(M);
    auto *print583Func = funcs.first;
    auto &ourFuncs = funcs.second;

    for (auto& F: M) {
      errs() << "Function: " << F << "\n";
      errs() << "Matches print583Func? " << (print583Func == &F) << "\n";
      if (ourFuncs.count(&F) == 0) {
        runOnFunction(F, print583Func);
      }
    }
    errs() << "runOnModule returning without error\n";
    return true;
  }
}; // end of struct Profile583

}  // end of anonymous namespace

char Profile583::ID = 0;
static RegisterPass<Profile583> X("false-sharing-profile",
                                  "Pass to profile false sharing",
                                   false /* Only looks at CFG */,
                                   false /* Analysis Pass */);
