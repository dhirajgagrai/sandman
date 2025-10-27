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
    const string EP = "EP";
    const string ENTRY = "entry";
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

        llvm::outs() << "digraph NFA {\n";
        llvm::outs() << "  node [shape=doublebox];\n\n";

        const std::set<std::string> &specialStates = {"main-" + ENTRY, "main-" + EXIT};
        for (const std::string &state : specialStates) {
            llvm::outs() << "  \"" << state << "\" [shape=circle,width=1,fixedsize=true];\n";
        }
        llvm::outs() << "\n";

        map<string, map<string, string>> nfa;

        for (Module::iterator FI = M.begin(), FE = M.end(); FI != FE; ++FI) {
            if (!isLibFn(FI->getName().str())) {
                Function &F = *FI;
                BasicBlock &EntryBlock = F.getEntryBlock();

                for (Function::iterator BI = F.begin(), BE = F.end(); BI != BE; ++BI) {
                    BasicBlock *B = dyn_cast<BasicBlock>(&*BI);
                    string bbName = FI->getName().str() + "-" + B->getName().str();

                    if (B->getName().str() == EntryBlock.getName().str()) {
                        bbName = FI->getName().str() + "-" + ENTRY;
                    }

                    bool isItrmInserted = false;
                    int itrmCount = 1;
                    string uBbName = "";
                    string prevBb = bbName;

                    for (Instruction &I : *B) {
                        if (isa<CallInst>(I)) {
                            string funcName = cast<CallInst>(I).getCalledFunction()->getName().str();
                            if (isLibFn(funcName)) {
                                uBbName = bbName + "_i" + to_string(itrmCount);
                                nfa[prevBb][uBbName] = funcName + "()";

                                itrmCount++;
                            } else {
                                uBbName = funcName + "-" + ENTRY;
                                nfa[prevBb][uBbName] = EP;
                                uBbName = funcName + "-" + EXIT;
                            }

                            prevBb = uBbName;
                            isItrmInserted = true;
                        }
                    }

                    if (successors(B).empty()) {
                        string uExit = FI->getName().str() + "-" + EXIT;
                        if (isItrmInserted) {
                            // epsilon transition to exit state after intermediate lib call in a block
                            // uBbName -> last intermediate lib call
                            nfa[uBbName][uExit] = EP;
                        } else {
                            // epsilon transition to exit state from a block
                            nfa[bbName][uExit] = EP;
                        }
                    } else {
                        for (BasicBlock *Succ : successors(B)) {
                            string uSuccName = FI->getName().str() + "-" + Succ->getName().str();
                            if (isItrmInserted) {
                                // epsilon transition to successor blocks after intermediate lib call in a block
                                // uBbName -> last intermediate lib call
                                nfa[uBbName][uSuccName] = EP;
                            } else {
                                // epsilon transition to successor blocks from a block
                                nfa[bbName][uSuccName] = EP;
                            }
                        }
                    }

                } // end Function:iterator loop
            }
        } // end Module:iterator loop

        for (const auto &outerPair : nfa) {
            std::string currentState = outerPair.first;
            const auto &transitions = outerPair.second;

            for (const auto &innerPair : transitions) {
                std::string nextState = innerPair.first;
                std::string input = innerPair.second;

                llvm::outs() << "  \"" << currentState << "\""
                             << " -> "
                             << "\"" << nextState << "\""
                             << " [label=\"" << input << "\"];\n";
            }
        }

        llvm::outs() << "}\n";

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
