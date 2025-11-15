#include "DummyPass.h"
#include "CfgPass.h"

#include "llvm/IR/IRBuilder.h"

using namespace llvm;
using namespace std;

const int64_t DUMMY_ID = 462;

PreservedAnalyses DummyPass::run(Module &M, ModuleAnalysisManager &AM) {
    const CfgPassResult &Result = AM.getResult<CfgPass>(M);

    if (Result.FoundLibCalls.empty()) {
        return PreservedAnalyses::all();
    }

    LLVMContext &Ctx = M.getContext();

    Type *Int64Ty = Type::getInt64Ty(Ctx);
    Type *Int32Ty = Type::getInt32Ty(Ctx);

    FunctionType *SyscallFuncType = FunctionType::get(Int64Ty, {Int64Ty}, true);

    FunctionCallee SyscallFunc = M.getOrInsertFunction("syscall", SyscallFuncType);

    IRBuilder<> Builder(Ctx);

    for (auto const &[CI, id] : Result.FoundLibCalls) {
        Builder.SetInsertPoint(CI);
        Value *SyscallNum = ConstantInt::get(Int64Ty, DUMMY_ID);
        Value *idVal = ConstantInt::get(Int32Ty, id);
        Value *idVal64 = Builder.CreateZExt(idVal, Int64Ty);
        Builder.CreateCall(SyscallFunc, {SyscallNum, idVal64});
    }

    return PreservedAnalyses::none();
}
