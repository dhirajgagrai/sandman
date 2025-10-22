#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Module.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

using namespace llvm;

namespace {

struct DummyPass : public PassInfoMixin<DummyPass> {
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
        const TargetLibraryInfo *TLI;
        LibFunc LF;

        for (Module::iterator FI = M.begin(), FE = M.end(); FI != FE; ++FI) {
            if (!TLI->getLibFunc(FI->getName(), LF)) {
                errs() << "Entering " << FI->getName() << "\n";
                Function &F = *FI;
                for (BasicBlock &BB : F) {
                    errs() << " Basic Block: " << BB.getName() << "\n";
                    for (Instruction &I : BB) {
                        if (isa<CallInst>(I)) {
                            StringRef funcName = cast<CallInst>(I).getCalledFunction()->getName();
                            if (TLI->getLibFunc(funcName, LF)) {
                                errs() << "  Lib Call :::: " << funcName << "\n";
                            } else {
                                errs() << "  Nor Call :::: " << funcName << "\n";
                            }
                        } else {
                            errs() << "  Instrn: " << I << "\n";
                        }
                    }
                }
            }
        }

        return PreservedAnalyses::all();
    };
};

} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return {.APIVersion = LLVM_PLUGIN_API_VERSION,
            .PluginName = "Dummy Pass",
            .PluginVersion = "v0.1",
            .RegisterPassBuilderCallbacks = [](PassBuilder &PB) {
                PB.registerPipelineStartEPCallback(
                    [](ModulePassManager &MPM, OptimizationLevel Level) {
                        MPM.addPass(DummyPass());
                    });
            }};
}
