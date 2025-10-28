#include "DummyPass.h"
#include "CfgPass.h"

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Type.h"

using namespace llvm;
using namespace std;

PreservedAnalyses DummyPass::run(Module &M, ModuleAnalysisManager &AM) {
    const CfgPassResult &Result = AM.getResult<CfgPass>(M);

    if (!Result.FoundLibCalls.empty()) {
        LLVMContext &Ctx = M.getContext();

        Type *Int32Ty = Type::getInt32Ty(Ctx);
        Type *Int8PtrTy = PointerType::getUnqual(Ctx);
        FunctionType *PrintfType = FunctionType::get(
            Int32Ty,   // Return type
            Int8PtrTy, // First arg: char*
            true       // isVarArg
        );

        FunctionCallee PrintfFunc = M.getOrInsertFunction("printf", PrintfType);
        IRBuilder<> Builder(Ctx);

        GlobalVariable *GlobalStr = Builder.CreateGlobalString(
            "--- instrumented ---\n",
            ".hook_fmt_str",
            0, // Address Space
            &M // Module to add it to
        );

        Value *FormatString = Builder.CreateConstInBoundsGEP2_32(
            GlobalStr->getValueType(),
            GlobalStr,
            0, 0, // GEP indices (for array, then element)
            ".hook_fmt_str_ptr");

        for (llvm::CallInst *CI : Result.FoundLibCalls) {
            Builder.SetInsertPoint(CI);

            // --- 4. Insert the call to printf ---
            // We re-use the *same* FormatString constant for every call
            Builder.CreateCall(PrintfFunc, {FormatString});

            errs() << "  -> Instrumenting: ";
            CI->print(errs());
            errs() << "\n";
        }
    }

    return PreservedAnalyses::none();
}
