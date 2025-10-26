#include "llvm/IR/CFG.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Module.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

#include <fstream>
#include <map>
#include <unordered_set>

using namespace llvm;
using namespace std;

namespace {

unordered_set<string> loadFunctionList() {
    unordered_set<string> fns;
    string line;

    ifstream dumpFile("build/pass/libc_functions.txt");

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

struct CfgPass : public PassInfoMixin<CfgPass> {
    const string EP = "epsilon";
    const string EXIT = "exit";

    unordered_set<string> fnsList;
    CfgPass() {
        fnsList = loadFunctionList();
    }

    bool isLibFn(const string &nameToFind) {
        return fnsList.count(nameToFind) > 0;
    }

    PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
        LibFunc LF;

        for (Module::iterator FI = M.begin(), FE = M.end(); FI != FE; ++FI) {
            map<string, map<string, string>> nfa;
            if (!isLibFn(FI->getName().str())) {
                Function &F = *FI;

                for (Function::iterator BI = F.begin(), BE = F.end(); BI != BE; ++BI) {
                    BasicBlock *B = dyn_cast<BasicBlock>(&*BI);
                    string bbName = FI->getName().str() + "-" + B->getName().str();

                    int itrmCount = 0;
                    string uBbName = "";
                    string prev = bbName;
                    for (Instruction &I : *B) {
                        if (isa<CallInst>(I)) {
                            string funcName = cast<CallInst>(I).getCalledFunction()->getName().str();
                            if (isLibFn(funcName)) {
                                itrmCount++;

                                uBbName = bbName + "_i" + to_string(itrmCount);
                                nfa[prev][uBbName] = funcName;
                                prev = uBbName;
                            }
                        }
                    }

                    if (successors(B).empty()) {
                        string uExit = FI->getName().str() + "-" + EXIT;
                        if (itrmCount == 0) {
                            // epsilon transition to exit state from a block
                            nfa[bbName][uExit] = EP;
                        } else {
                            // epsilon transition to exit state after intermediate lib call in a block
                            // uBbName -> last intermediate lib call
                            nfa[uBbName][uExit] = EP;
                        }
                    } else {
                        for (BasicBlock *Succ : successors(B)) {
                            string uSuccName = FI->getName().str() + "-" + Succ->getName().str();
                            if (itrmCount == 0) {
                                // epsilon transition to successor blocks from a block
                                nfa[bbName][uSuccName] = EP;
                            } else {
                                // epsilon transition to successor blocks after intermediate lib call in a block
                                // uBbName -> last intermediate lib call
                                nfa[uBbName][uSuccName] = EP;
                            }
                        }
                    }

                } // end Function:iterator loop
            }

            for (const auto &outerPair : nfa) {
                string currentState = outerPair.first;
                const auto &transitions = outerPair.second;

                errs() << "\nState: " << currentState << "\n";

                if (transitions.empty()) {
                    errs() << "  (No outgoing transitions)" << "\n";
                    continue;
                }

                for (const auto &innerPair : transitions) {
                    string nextState = innerPair.first;
                    string input = innerPair.second;

                    errs() << "  On Input: " << input << " -> Go To: " << nextState << "\n";
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
            .PluginName = "Cfg Pass",
            .PluginVersion = "v0.1",
            .RegisterPassBuilderCallbacks = [](PassBuilder &PB) {
                PB.registerPipelineStartEPCallback(
                    [](ModulePassManager &MPM, OptimizationLevel Level) {
                        MPM.addPass(CfgPass());
                    });
            }};
}
