#include "CfgPass.h"

#include "llvm/Passes/PassBuilder.h"

#include <fstream>
#include <map>
#include <queue>
#include <random>
#include <sstream>
#include <unordered_set>

using namespace llvm;
using namespace std;

AnalysisKey CfgPass::Key;

const string EP = "EP";
const string ENTRY = "entry";
const string EXIT = "exit";

using StateSet = set<string>;
using NfaTransitions = map<string, map<string, StateSet>>;

struct DfaStateInfo {
    StateSet nfaStates;
    bool isAcceptState = false;
};

using DfaTransitions = map<string, map<string, string>>;
using DfaStatesMap = map<string, DfaStateInfo>;

StateSet epsilonClosure(const StateSet &states, const NfaTransitions &nfa) {
    StateSet closure = states;
    queue<string> worklist;
    for (const auto &state : states) {
        worklist.push(state);
    }

    while (!worklist.empty()) {
        string currentState = worklist.front();
        worklist.pop();

        auto nfaStateIt = nfa.find(currentState);
        if (nfaStateIt != nfa.end()) {
            const auto &transitions = nfaStateIt->second;
            auto epsilonIt = transitions.find(EP);
            if (epsilonIt != transitions.end()) {
                for (const auto &nextState : epsilonIt->second) {
                    if (closure.insert(nextState).second) {
                        worklist.push(nextState);
                    }
                }
            }
        }
    }
    return closure;
}

StateSet move(const StateSet &states, const string &input, const NfaTransitions &nfa) {
    StateSet reachableStates;
    for (const auto &state : states) {
        auto nfaStateIt = nfa.find(state);
        if (nfaStateIt != nfa.end()) {
            const auto &transitions = nfaStateIt->second;
            auto inputIt = transitions.find(input);
            if (inputIt != transitions.end()) {
                reachableStates.insert(inputIt->second.begin(), inputIt->second.end());
            }
        }
    }
    return reachableStates;
}

string stateSetToString(const StateSet &states) {
    stringstream ss;
    vector<string> sortedStates(states.begin(), states.end());
    std::sort(sortedStates.begin(), sortedStates.end());
    ss << "{";
    bool first = true;
    for (const auto &state : sortedStates) {
        if (!first)
            ss << ",";
        ss << state;
        first = false;
    }
    ss << "}";
    return ss.str();
}

struct DfaResult {
    DfaTransitions transitions;
    DfaStatesMap states;
    string startStateName;
    set<string> acceptStateNames;
};

DfaResult convertNfaToDfa(
    const NfaTransitions &nfa,
    const string &nfaStartState,
    const StateSet &nfaAcceptStates) {
    DfaResult dfaResult;
    map<StateSet, string> stateSetToDfaName;
    queue<StateSet> worklist;
    set<string> alphabet;
    int dfaStateCounter = 0;

    for (const auto &pair : nfa) {
        for (const auto &transPair : pair.second) {
            if (transPair.first != EP) {
                alphabet.insert(transPair.first);
            }
        }
    }

    StateSet startSet = epsilonClosure({nfaStartState}, nfa);
    if (stateSetToDfaName.find(startSet) == stateSetToDfaName.end()) {
        dfaResult.startStateName = "D" + to_string(dfaStateCounter++);
        stateSetToDfaName[startSet] = dfaResult.startStateName;
        dfaResult.states[dfaResult.startStateName] = {startSet, false};
        worklist.push(startSet);
    } else {
        dfaResult.startStateName = stateSetToDfaName[startSet];
    }

    while (!worklist.empty()) {
        StateSet currentNfaStates = worklist.front();
        worklist.pop();
        string currentDfaName = stateSetToDfaName[currentNfaStates];

        for (const string &input : alphabet) {
            StateSet nextNfaStatesRaw = move(currentNfaStates, input, nfa);
            if (nextNfaStatesRaw.empty()) {
                continue;
            }
            StateSet nextNfaStatesClosure = epsilonClosure(nextNfaStatesRaw, nfa);

            string nextDfaName;
            if (stateSetToDfaName.find(nextNfaStatesClosure) == stateSetToDfaName.end()) {
                nextDfaName = "D" + to_string(dfaStateCounter++);
                stateSetToDfaName[nextNfaStatesClosure] = nextDfaName;
                dfaResult.states[nextDfaName] = {nextNfaStatesClosure, false};
                worklist.push(nextNfaStatesClosure);
            } else {
                nextDfaName = stateSetToDfaName[nextNfaStatesClosure];
            }
            dfaResult.transitions[currentDfaName][input] = nextDfaName;
        }
    }

    for (auto &pair : dfaResult.states) {
        const string &dfaName = pair.first;
        DfaStateInfo &dfaInfo = pair.second;
        for (const string &nfaAcceptState : nfaAcceptStates) {
            if (dfaInfo.nfaStates.count(nfaAcceptState)) {
                dfaInfo.isAcceptState = true;
                dfaResult.acceptStateNames.insert(dfaName);
                break;
            }
        }
    }

    return dfaResult;
}

void printDfaDot(const DfaResult &dfa) {
    outs() << "digraph DFA {\n";
    outs() << "  node [shape = point]; start_node;\n";
    outs() << "  node [shape = circle];\n";

    for (const auto &pair : dfa.states) {
        const string &dfaName = pair.first;
        const DfaStateInfo &dfaInfo = pair.second;
        outs() << "  \"" << dfaName << "\" [label=\"" << dfaName
               << "\""
               << (dfaInfo.isAcceptState ? ", shape=doublecircle" : "")
               << "];\n";
    }

    outs() << "  start_node -> \"" << dfa.startStateName << "\";\n";

    for (const auto &outerPair : dfa.transitions) {
        const string &currentState = outerPair.first;
        const auto &transitions = outerPair.second;
        for (const auto &innerPair : transitions) {
            const string &input = innerPair.first;
            const string &nextState = innerPair.second;
            outs() << "  \"" << currentState << "\""
                   << " -> "
                   << "\"" << nextState << "\""
                   << " [label=\"" << input << "\"];\n";
        }
    }

    outs() << "}\n";
}

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
        errs() << "ERROR: Could not open dump file.\n";
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

CfgPassResult CfgPass::run(Module &M, ModuleAnalysisManager &AM) {
    Result R;

    NfaTransitions nfa;

    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<int> distrib(100, 999);

    int uid = distrib(gen);

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
                        string transition = funcName + "(): " + to_string(uid);
                        uBbName = bbName + "_i" + to_string(itrmCount);
                        nfa[prevBb][transition].insert(uBbName);

                        R.FoundLibCalls[dyn_cast<CallInst>(&I)] = uid;

                        uid++;
                        itrmCount++;
                    } else {
                        string funcEntry = funcName + "-" + ENTRY;
                        nfa[prevBb][EP].insert(funcEntry);
                        string funcExit = funcName + "-" + EXIT;
                        uBbName = funcExit;
                        if (CalledF->isIntrinsic()) {
                            nfa[prevBb][EP].insert(funcExit);
                        }
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
                    nfa[uBbName][EP].insert(uExit);
                } else {
                    // epsilon transition to exit state from a block
                    nfa[bbName][EP].insert(uExit);
                }
            } else {
                for (BasicBlock *Succ : successors(B)) {
                    string uSuccName = F.getName().str() + "-" + Succ->getName().str();
                    if (isItrmInserted) {
                        // epsilon transition to successor blocks after intermediate lib call in a block
                        // uBbName -> last intermediate lib call
                        nfa[uBbName][EP].insert(uSuccName);
                    } else {
                        // epsilon transition to successor blocks from a block
                        nfa[bbName][EP].insert(uSuccName);
                    }
                }
            }

        } // end Function:iterator loop
    }

    DfaResult dfa = convertNfaToDfa(nfa, "main-" + ENTRY, {"main-" + EXIT});
    printDfaDot(dfa);

    return R;
};
