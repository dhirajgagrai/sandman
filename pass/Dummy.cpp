#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Module.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include <fstream>
#include <unordered_set>

using namespace llvm;

namespace {
std::unordered_set<std::string> loadFunctionList() {
    std::unordered_set<std::string> fns;
    std::string line;

    std::ifstream dumpFile("build/pass/libc_functions.txt");

    if (dumpFile.is_open()) {
        while (getline(dumpFile, line)) {
            if (!line.empty()) {
                fns.insert(line);
            }
        }
        dumpFile.close();
    } else {
        llvm::errs() << "ERROR: Could not open dump file.\n";
        exit(1);
    }

    return fns;
}

struct DummyPass : public PassInfoMixin<DummyPass> {
    std::unordered_set<std::string> fnsList;
    DummyPass() {
        fnsList = loadFunctionList();
    }

    bool isLibFn(const std::string &nameToFind) {
        return fnsList.count(nameToFind) > 0;
    }

    PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
        LibFunc LF;

        for (Module::iterator FI = M.begin(), FE = M.end(); FI != FE; ++FI) {
            if (!isLibFn(FI->getName().str())) {
                errs() << "Entering " << FI->getName() << "\n";
                Function &F = *FI;
                for (BasicBlock &BB : F) {
                    errs() << " Basic Block: " << BB.getName() << "\n";
                    for (Instruction &I : BB) {
                        if (isa<CallInst>(I)) {
                            StringRef funcName = cast<CallInst>(I).getCalledFunction()->getName();
                            if (isLibFn(funcName.str())) {
                                errs() << "  Instrn: " << I << " ::::::::::: Library Call ::::::::::: " << funcName << "\n";
                            } else {
                                errs() << "  Instrn: " << I << " ::::::::::: Normal Call ::::::::::: " << funcName << "\n";
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
