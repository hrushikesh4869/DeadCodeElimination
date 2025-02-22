#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Analysis/DominanceFrontier.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/InstIterator.h"

#include "deadCodeElimination.h"

using namespace llvm;

namespace {

struct deadCodeElimination : public PassInfoMixin<deadCodeElimination> {
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
        bool change = true;
        while(change) {
            DenseMap<Instruction *, std::set<Value *>> Use, Def, In, Out;
            calculateUseDef(F, Use, Def);
            livelinessAnalysis(F, Use, Def, In, Out);
            change = eliminateDeadCode(F, Def, Out);
            if(!change) {
                change = change || removeDeadBB(F);
                change = change || handleBranches(F);
                change = change || eliminateDeadAlloca(F);
            }
        }
        return PreservedAnalyses::all();
    }
    static bool isRequired() { return true; }
};

}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return {
        .APIVersion = LLVM_PLUGIN_API_VERSION,
        .PluginName = "Dead code elimination pass",
        .PluginVersion = "v0.1",
        .RegisterPassBuilderCallbacks = [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name,FunctionPassManager &FPM, ArrayRef<PassBuilder::PipelineElement>) {
                    FPM.addPass(deadCodeElimination());
		            return true;
                });
        }
    };
}
