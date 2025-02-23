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

#include<vector>
#include<set>

using namespace llvm;

void calculateUseDef(Function &F, DenseMap<Instruction *, std::set<Value *>> &Use, DenseMap<Instruction *, std::set<Value *>> &Def) {
    for(auto &BB : F) {
        for(auto &Inst : BB) {
            Use.insert({&Inst, {}});
            Def.insert({&Inst, {}});

            if(isa<StoreInst>(&Inst)) {
                Value *op1 = Inst.getOperand(0);
                Value *op2 = Inst.getOperand(1);
                if(isa<Instruction>(op1) || isa<Argument>(op1))
                    Use[&Inst].insert(op1);
                
                if(isa<Instruction>(op2) || isa<Argument>(op2))
                    Def[&Inst].insert(op2);
                
                continue;
            }

            for(int i = 0; i < Inst.getNumOperands(); i++) {
                Value *op = Inst.getOperand(i);
                if(isa<Instruction>(op) || isa<Argument>(op)) {
                    if(Def[&Inst].find(op) == Def[&Inst].end())
                        Use[&Inst].insert(op);
                }
            }
            if (!Inst.getType()->isVoidTy()) {
                Def[&Inst].insert(&Inst);
            }
        }
    }
}

void livelinessAnalysis(Function &F, DenseMap<Instruction *, std::set<Value *>> &Use, DenseMap<Instruction *, std::set<Value *>> &Def,
    DenseMap<Instruction *, std::set<Value *>> &In, DenseMap<Instruction *, std::set<Value *>> &Out) {
    bool change = true;
    for(inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
        Instruction *Inst = &(*I);
        In[Inst] = {};
        Out[Inst] = {};
    }
    while(change) {
        change = false;
        for(inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {

            std :: set<Value *> Diff;
            Instruction *Inst = &(*I);

            // Calculate OUT set.
            // OUT = Union of In sets of all the successors.
            Instruction *Succ = Inst->getNextNode();
            if(Succ) {
                for(auto i : In[Succ])
                    Out[Inst].insert(i);
            }
            else{
                BasicBlock *BB = Inst->getParent();
                for (auto succ = succ_begin(BB); succ != succ_end(BB); ++succ) {
                    if (!(*succ)->empty()) {
                        Instruction &firstInst = (*succ)->front();
                        for(auto i : In[&firstInst]) {
                            Out[Inst].insert(i);
                        }
                    }
                }
            }

            // Calculate In set
            // In = USE U (OUT - DEF)
            for(auto i : Out[Inst]) {
                if(Def[Inst].find(i) == Def[Inst].end())
                    Diff.insert(i);
            }

            for(auto i : Use[Inst]) {
                if(In[Inst].find(i) == In[Inst].end()) {
                    In[Inst].insert(i);
                    change = true;
                }
            }

            for(auto i : Diff) {
                if(In[Inst].find(i) == In[Inst].end()) {
                    In[Inst].insert(i);
                    change = true;
                }
            }
        }
    }
}

bool eliminateDeadCode(Function &F, DenseMap<Instruction *, std::set<Value *>> &Def, DenseMap<Instruction *, std::set<Value *>> &Out) {

    bool change = false;
    std :: vector<Instruction *> deadCode;

    for(inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
        Instruction *Inst = &(*I);

        if(Inst && isa<StoreInst>(Inst)) {
            Value *op1 = Inst->getOperand(0);
            Value *op2 = Inst->getOperand(1);

            if((Def[Inst].find(op2) != Def[Inst].end()) && (Out[Inst].find(dyn_cast<Instruction>(op2)) == Out[Inst].end())) {
                deadCode.push_back(Inst);
                change  = true;
            }
            continue;
        }

        if(Inst && (isa<AllocaInst>(Inst) || isa<CallInst>(Inst)))
            continue;

        if((Def[Inst].find(Inst) != Def[Inst].end()) && (Out[Inst].find(Inst) == Out[Inst].end())) {
            deadCode.push_back(Inst);
            change = true;
        }
    }

    for(int i = 0; i < deadCode.size(); i++)
        deadCode[i]->eraseFromParent();

    return change;
}

bool removeDeadBB(Function &F) {
    bool change = false;
    std :: vector<BasicBlock *> deadBB;

    for(auto &BB : F) {
        if (BB.empty() || (BB.size() == 1 && BB.getTerminator())) {
            deadBB.push_back(&BB);
        }
    }

    for(int i = 0; i < deadBB.size(); i++) {
        BasicBlock *BB = deadBB[i];
        BranchInst *BI = dyn_cast<BranchInst>(BB->getTerminator());
        if(!BI)
            continue;

        BasicBlock *Succ = BI->getSuccessor(0);
        Succ->removePredecessor(BB);
        BB->replaceAllUsesWith(Succ);
        BB->eraseFromParent();
        change = true;
    }
    return change;
}

bool handleBranches(Function &F) {
    bool change = false;
    for(auto &BB : F) {
        Instruction *Inst = BB.getTerminator();
        if(Inst && isa<BranchInst>(Inst)) {
            BranchInst *BI = dyn_cast<BranchInst>(Inst);
            if(BI->isConditional()) {
                Value *op1, *op2, *op3;
                
                op1 = Inst->getOperand(0);
                op2 = Inst->getOperand(1);
                op3 = Inst->getOperand(2);

                if((op2 == op3) && !isa<Constant>(op1)) {
                    Constant *CI = ConstantInt::get(op1->getType(), 1);
                    Inst->setOperand(0, CI);
                    change = true;
                }
            }
        }
    }
    return change;
}

bool eliminateDeadAlloca(Function &F) {
    std :: set<Value *> AllocaStatus;
    bool change = false;

    for(inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
        Instruction *Inst = &(*I);
        if(isa<AllocaInst>(Inst))
            AllocaStatus.insert(Inst);
    }
    for(inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
        Instruction *Inst = &(*I);
        //Inst->dump();
        //errs() <<"Operands : \n";
        for(int i = 0; i < Inst->getNumOperands(); i++) {
            Value *op = Inst->getOperand(i);
            //op->dump();
            if(AllocaStatus.find(op) != AllocaStatus.end())
                AllocaStatus.erase(op);
        }
    }
    for(auto i : AllocaStatus) {
        Instruction *Inst = dyn_cast<Instruction>(i);
        Inst->eraseFromParent();
        change = true;
    }

    return change;
}
