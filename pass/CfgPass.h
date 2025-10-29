#ifndef CFG_PASS_H
#define CFG_PASS_H

#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include <map>
#include <unordered_set>

class CfgPassResult {
  public:
    std::map<llvm::CallInst *, int> FoundLibCalls;
};

struct CfgPass : public llvm::AnalysisInfoMixin<CfgPass> {
    friend llvm::AnalysisInfoMixin<CfgPass>;
    static llvm::AnalysisKey Key;
    using Result = CfgPassResult;

  private:
    std::unordered_set<std::string> FnsList;
    bool isLibFn(const std::string &nameToFind) const;

  public:
    CfgPass();

    Result run(llvm::Module &M, llvm::ModuleAnalysisManager &AM);
};

#endif
