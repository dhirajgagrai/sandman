#include "CfgPass.h"

#include "llvm/Passes/PassBuilder.h"

#include <fstream>
#include <map>
#include <unordered_set>

using namespace llvm;
using namespace std;

llvm::AnalysisKey CfgPass::Key;

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

CfgPass::CfgPass() {
    FnsList = loadFunctionList();
}

bool CfgPass::isLibFn(const string &nameToFind) const {
    return FnsList.count(nameToFind) > 0;
}

const string EP = "EP";
const string ENTRY = "entry";
const string EXIT = "exit";

unordered_set<string> fnsList;

CfgPass::Result CfgPass::run(Module &M, ModuleAnalysisManager &AM) {
    Result R;

    llvm::outs() << "digraph NFA {\n";
    llvm::outs() << "  node [shape=doublebox];\n\n";

    const std::set<std::string> &specialStates = {"main-" + ENTRY, "main-" + EXIT};
    for (const std::string &state : specialStates) {
        llvm::outs() << "  \"" << state << "\" [shape=circle,width=1,fixedsize=true];\n";
    }
    llvm::outs() << "\n";

    map<string, map<string, string>> nfa;

    for (Function &F : M) {
        if (F.isDeclaration()) {
            continue;
        }

        BasicBlock &EntryBlock = F.getEntryBlock();

        for (Function::iterator BI = F.begin(), BE = F.end(); BI != BE; ++BI) {
            BasicBlock *B = dyn_cast<BasicBlock>(&*BI);
            string bbName = F.getName().str() + "-" + B->getName().str();

            if (B->getName().str() == EntryBlock.getName().str()) {
                bbName = F.getName().str() + "-" + ENTRY;
            }

            bool isItrmInserted = false;
            int itrmCount = 1;
            string uBbName = "";
            string prevBb = bbName;

            for (Instruction &I : *B) {
                if (isa<CallInst>(I)) {
                    Function *CalledF = cast<CallInst>(I).getCalledFunction();
                    string funcName = "";

                    if (CalledF->isIntrinsic()) {
                        Intrinsic::ID id = CalledF->getIntrinsicID();
                        StringRef baseName = Intrinsic::getBaseName(id);
                        if (baseName.starts_with("llvm.")) {
                            funcName = baseName.drop_front(5).str();
                        } else {
                            funcName = baseName.str();
                        }

                        if (!isLibFn(funcName)) {
                            continue;
                        }
                    } else {
                        funcName = CalledF->getName().str();
                    }

                    if (isLibFn(funcName)) {
                        uBbName = bbName + "_i" + to_string(itrmCount);
                        nfa[prevBb][uBbName] = funcName + "()";

                        R.FoundLibCalls.insert(llvm::dyn_cast<llvm::CallInst>(&I));

                        itrmCount++;
                    } else {
                        string funcEntry = funcName + "-" + ENTRY;
                        nfa[prevBb][funcEntry] = EP;
                        string funcExit = funcName + "-" + EXIT;
                        nfa[funcEntry][funcExit] = EP;
                        uBbName = funcExit;
                    }

                    prevBb = uBbName;
                    isItrmInserted = true;
                }
            }

            if (successors(B).empty()) {
                string uExit = F.getName().str() + "-" + EXIT;
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
                    string uSuccName = F.getName().str() + "-" + Succ->getName().str();
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

    return R;
};
