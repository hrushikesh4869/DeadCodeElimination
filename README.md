
# Dead Code Elimination Pass.

### Overview
The Dead Code Elimination (DCE) is an optimization technique used in compilers to remove code that does not affect the program's observable behavior. This LLVM pass implements DCE optimization using Liveliness analysis. It eliminates dead code and simplifies control flow by eliminating empty basic blocks.

### How it works
Pass uses Liveliness analysis information to identify and eliminate dead instructions. This information is calculated for each instruction. Following sections explain how liveliness analysis information is calculated and used to eliminate dead code.
#### Liveliness analysis
Here are some definitions before going into algorithm.

USE set : All variables which are used in current basic block prior to any assignment to them.

DEF set : All variables which are defined in current basic block.

IN set : IN set of basic block is set of all variables which are alive at the start of that basic block.

OUT set : OUT set of basic block is set of all variables which are alive at the end of that basic block.

*Algorithm :*

1. **Initialize**:
   - For each Instruction `Inst`, initialize `In[Inst]` and `Out[Inst]` to empty sets.
2. **Iterate**:
   - Repeat until `In` and `Out` sets stabilize (i.e., no changes occur):
     - For each Instruction `Inst` in `Function` :
       1. `Out[Inst]` = Union of `In` sets of all successors of `Inst`.
       2. `LiveIn[Inst]` = `Use[Inst]` ∪ (`Out[Inst]` - `Def[Inst]`).

#### Eliminating dead instructions
An instruction is considered dead if it is in its DEF set but not present in its OUT set.

#### Eliminating empty basic blocks
After removing dead instructions there might be some empty basic blocks in CFG. To remove such basic blocks pass simply traverses the CFG and looks for blocks with size one and containing branch instruction as a terminator instruction. To delete the block, pass simply redirects the control flow and replaces all the uses of current basic block with its successor.

#### Handling branches after deleting empty basic blocks
After eliminating empty blocks there might arise some conditional branch instructions where both the target blocks in that branch are same. As shown in following example : 

```llvm
define i32 @main(i32 %argc, ptr %argv) #0 {
  %add = add nsw i32 %argc, 42
  %mul = mul nsw i32 %add, 2
  %cmp = icmp sgt i32 %mul, 0
  br i1 %cmp, label %cond.end, label %cond.end

cond.end:                                         
  %cond = phi i32 [ %mul, %cond.true ], [ %argc, %cond.false ]
  %call = call i32 (ptr, ...) @printf(ptr noundef @.str)
  ret i32 0
}
```

In such cases branch instruction's condition variable is redundant. Simply setting that variable to a constant offers more chances to eliminate dead code. Following is a resulting code after setting condition variable in branch to constant and eliminating dead code.

```llvm
define i32 @main(i32 %argc, ptr %argv) {
  br i1 true, label %cond.end, label %cond.end

cond.end:                                         
  %call = call i32 (ptr, ...) @printf(ptr noundef @.str)
  ret i32 0
}
```

Finally dead alloca slots are eliminated for those variables which are not used anywhere in the CFG.
### Requirements to build and run pass

Operating system :  Ubuntu version "24.04.2 LTS (Noble Numbat)".

LLVM version used : Ubuntu LLVM version 18.1.3

CMake version : cmake version 3.28.3


Installing LLVM and CMake on Ubuntu : 
```bash
sudo apt install llvm
sudo apt install cmake
```

### Building and running the pass

**Build**
```bash
git clone https://github.com/hrushikesh4869/DeadCodeElimination.git
cd DeadCodeElimination
mkdir build
cd build
cmake ..
cmake --build .
```

**Run**
```bash
# Run this command from root dir of project
opt -S -load-pass-plugin=build/deadCodeElimination/deadCodeElimination.so -passes=deadCodeEliminationPass path/to/IR/file
```