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

struct MinNfaStateInfo {
    StateSet nfaStates;
    bool isAcceptState = false;
};

using MinNfaTransitions = map<string, map<string, string>>;
using MinNfaStatesMap = map<string, MinNfaStateInfo>;

StateSet epsilonClosure(const StateSet &states, const NfaTransitions &nfa) {
    StateSet closure = states;
    queue<string> q;
    for (const auto &state : states) {
        q.push(state);
    }

    while (!q.empty()) {
        string currentState = q.front();
        q.pop();

        auto nfaStateIt = nfa.find(currentState);
        if (nfaStateIt != nfa.end()) {
            const auto &transitions = nfaStateIt->second;
            auto epsilonIt = transitions.find(EP);
            if (epsilonIt != transitions.end()) {
                for (const auto &nextState : epsilonIt->second) {
                    if (closure.insert(nextState).second) {
                        q.push(nextState);
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

struct MinNfaResult {
    MinNfaTransitions transitions;
    MinNfaStatesMap states;
    string startStateName;
    set<string> acceptStateNames;
};

MinNfaResult convertNfaToMinNfa(
    const NfaTransitions &nfa,
    const string &nfaStartState,
    const StateSet &nfaAcceptStates) {
    MinNfaResult minNfaResult;
    map<StateSet, string> stateSetToNfaName;
    queue<StateSet> q;
    set<string> alphabet;
    int nfaStateCounter = 0;

    for (const auto &pair : nfa) {
        for (const auto &transPair : pair.second) {
            if (transPair.first != EP) {
                alphabet.insert(transPair.first);
            }
        }
    }

    StateSet startSet = epsilonClosure({nfaStartState}, nfa);
    if (stateSetToNfaName.find(startSet) == stateSetToNfaName.end()) {
        minNfaResult.startStateName = "S" + to_string(nfaStateCounter++);
        stateSetToNfaName[startSet] = minNfaResult.startStateName;
        minNfaResult.states[minNfaResult.startStateName] = {startSet, false};
        q.push(startSet);
    } else {
        minNfaResult.startStateName = stateSetToNfaName[startSet];
    }

    while (!q.empty()) {
        StateSet currentNfaStates = q.front();
        q.pop();
        string currentNfaName = stateSetToNfaName[currentNfaStates];

        for (const string &input : alphabet) {
            StateSet nextNfaStatesRaw = move(currentNfaStates, input, nfa);
            if (nextNfaStatesRaw.empty()) {
                continue;
            }
            StateSet nextNfaStatesClosure = epsilonClosure(nextNfaStatesRaw, nfa);

            string nextNfaName;
            if (stateSetToNfaName.find(nextNfaStatesClosure) == stateSetToNfaName.end()) {
                nextNfaName = "S" + to_string(nfaStateCounter++);
                stateSetToNfaName[nextNfaStatesClosure] = nextNfaName;
                minNfaResult.states[nextNfaName] = {nextNfaStatesClosure, false};
                q.push(nextNfaStatesClosure);
            } else {
                nextNfaName = stateSetToNfaName[nextNfaStatesClosure];
            }
            minNfaResult.transitions[currentNfaName][input] = nextNfaName;
        }
    }

    for (auto &pair : minNfaResult.states) {
        const string &nfaName = pair.first;
        MinNfaStateInfo &nfaInfo = pair.second;
        for (const string &nfaAcceptState : nfaAcceptStates) {
            if (nfaInfo.nfaStates.count(nfaAcceptState)) {
                nfaInfo.isAcceptState = true;
                minNfaResult.acceptStateNames.insert(nfaName);
                break;
            }
        }
    }

    return minNfaResult;
}

void printNfaDot(const MinNfaResult &nfa) {
    std::error_code EC;
    raw_fd_ostream DotFile("nfa.dot", EC);

    if (EC) {
        errs() << "Error opening nfa.dot: " << EC.message() << "\n";
    } else {
        DotFile << "digraph NFA {\n";
        DotFile << "  node [shape = point]; start_node;\n";
        DotFile << "  node [shape = circle];\n";

        for (const auto &pair : nfa.states) {
            const string &nfaName = pair.first;
            const MinNfaStateInfo &nfaInfo = pair.second;
            DotFile << "  \"" << nfaName << "\" [label=\"" << nfaName
                    << "\""
                    << (nfaInfo.isAcceptState ? ", shape=doublecircle" : "")
                    << "];\n";
        }

        DotFile << "  start_node -> \"" << nfa.startStateName << "\";\n";

        for (const auto &outerPair : nfa.transitions) {
            const string &currentState = outerPair.first;
            const auto &transitions = outerPair.second;
            for (const auto &innerPair : transitions) {
                const string &input = innerPair.first;
                const string &nextState = innerPair.second;
                DotFile << "  \"" << currentState << "\""
                        << " -> "
                        << "\"" << nextState << "\""
                        << " [label=\"" << input << "\"];\n";
            }
        }

        DotFile << "}\n";
    }
}

void generateDatFiles(const MinNfaResult &nfa, const std::map<std::string, int> &inputSymbolToId) {
    std::error_code EC;
    raw_fd_ostream DatFile("nfa.dat", EC);

    if (EC) {
        errs() << "Error opening nfa.dat: " << EC.message() << "\n";
    } else {
        int stateIdCounter = 0;
        int errorStateId = -1;
        std::map<std::string, int> stateNameToId;

        stateNameToId[nfa.startStateName] = stateIdCounter++;
        stateNameToId["STATE_ERROR"] = errorStateId;

        for (const auto &statePair : nfa.states) {
            const std::string &stateName = statePair.first;
            if (stateNameToId.find(stateName) == stateNameToId.end()) {
                stateNameToId[stateName] = stateIdCounter++;
            }
        }

        for (const auto &outerPair : nfa.transitions) {
            std::string currentStateName = outerPair.first;
            for (const auto &innerPair : outerPair.second) {
                std::string inputSymbol = innerPair.first;
                std::string nextStateName = innerPair.second;

                int is_final = nfa.acceptStateNames.count(nextStateName);

                if (inputSymbolToId.count(inputSymbol) &&
                    stateNameToId.count(currentStateName) &&
                    stateNameToId.count(nextStateName)) {
                    int currentStateId = stateNameToId.at(currentStateName);
                    int inputId = inputSymbolToId.at(inputSymbol);
                    int nextStateId = stateNameToId.at(nextStateName);

                    DatFile << currentStateId << " " << inputId << " " << nextStateId << " " << is_final << "\n";
                }
            }
        }

        DatFile.close();
    }
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
    std::map<std::string, int> funcId;
    std::set<std::string> nfaAcceptStates;

    nfaAcceptStates.insert("main-" + EXIT);

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
                        // Handle transition for lib calls
                        string transition = funcName + "(): " + to_string(uid);
                        uBbName = bbName + "_i" + to_string(itrmCount);
                        nfa[prevBb][transition].insert(uBbName);

                        R.FoundLibCalls[dyn_cast<CallInst>(&I)] = uid;

                        funcId[transition] = uid;

                        uid++;
                        itrmCount++;
                    } else {
                        // Placeholder for non-lib functions
                        string funcEntry = funcName + "-" + ENTRY;
                        nfa[prevBb][EP].insert(funcEntry);
                        string funcExit = funcName + "-" + EXIT;
                        uBbName = funcExit;

                        // Intrinsic functions can be bypassed
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
                    // epsilon transition to exit state from intermediate lib call in a block
                    // uBbName -> last intermediate lib call
                    nfa[uBbName][EP].insert(uExit);
                } else {
                    // epsilon transition to exit state from a block
                    nfa[bbName][EP].insert(uExit);
                }

                Instruction *Terminator = B->getTerminator();
                if (isa<UnreachableInst>(Terminator)) {
                    nfaAcceptStates.insert(uExit);
                }
            } else {
                for (BasicBlock *Succ : successors(B)) {
                    string uSuccName = F.getName().str() + "-" + Succ->getName().str();
                    if (isItrmInserted) {
                        // epsilon transition to successor blocks after intermediate lib call in a block
                        // uBbName -> last intermediate lib call
                        nfa[uBbName][EP].insert(uSuccName);
                    } else {
                        // epsilon transition to successor blocks from the block entry
                        nfa[bbName][EP].insert(uSuccName);
                    }
                }
            }

        } // end Function:iterator loop
    }

    // Post processing to remove transition from accepting states
    for (const std::string &acceptState : nfaAcceptStates) {
        if (nfa.count(acceptState)) {
            nfa.erase(acceptState);
        }
    }

    MinNfaResult minNfa = convertNfaToMinNfa(nfa, "main-" + ENTRY, nfaAcceptStates);
    printNfaDot(minNfa);
    generateDatFiles(minNfa, funcId);

    return R;
};
