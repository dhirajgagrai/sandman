#ifndef DUMMY_PASS_H
#define DUMMY_PASS_H

#include "llvm/IR/PassManager.h"

struct DummyPass : public llvm::PassInfoMixin<DummyPass> {
    llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &AM);
};

#endif
