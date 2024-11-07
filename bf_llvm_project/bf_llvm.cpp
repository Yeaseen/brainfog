#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Scalar.h" // This should include most Scalar passes
#include "llvm/Transforms/Scalar/GVN.h" // Verify if this is necessary
#include <iostream>
#include <stack>
#include <memory>
#include <string>
#include <fstream>

using namespace llvm;
using namespace std;

LLVMContext Context;
unique_ptr<Module> ModulePtr;
IRBuilder<> Builder(Context);

void generateLLVM(const string& code) {
    ModulePtr = make_unique<Module>("bf_module", Context);

    // Create the main function
    FunctionType *FuncType = FunctionType::get(Type::getInt32Ty(Context), false);
    Function *MainFunc = Function::Create(FuncType, Function::ExternalLinkage, "main", ModulePtr.get());

    BasicBlock *EntryBB = BasicBlock::Create(Context, "entry", MainFunc);
    Builder.SetInsertPoint(EntryBB);

    // Tape memory array (30000 cells of i8)
    ArrayType *TapeType = ArrayType::get(Type::getInt8Ty(Context), 30000);
    GlobalVariable *Tape = new GlobalVariable(*ModulePtr, TapeType, false, GlobalValue::PrivateLinkage, Constant::getNullValue(TapeType), "tape");

    // Pointer to the start of the tape
    Value *TapePtr = Builder.CreateGEP(TapeType, Tape, {Builder.getInt32(0), Builder.getInt32(0)}, "tape_ptr");

    // Index variable initialized to 0
    AllocaInst *Index = Builder.CreateAlloca(Type::getInt64Ty(Context), nullptr, "index");
    Builder.CreateStore(Builder.getInt64(0), Index);

    // Declare external functions for input and output
    FunctionCallee PutCharFunc = ModulePtr->getOrInsertFunction("putchar", FunctionType::get(Type::getInt32Ty(Context), {Type::getInt32Ty(Context)}, false));
    FunctionCallee GetCharFunc = ModulePtr->getOrInsertFunction("getchar", FunctionType::get(Type::getInt32Ty(Context), {}, false));

    // Stacks for loop handling
    stack<BasicBlock*> loopStartStack;
    stack<BasicBlock*> afterLoopStack;

    for (char command : code) {
        switch (command) {
            case '>':  // Increment index
                {
                    Value *CurrentIndex = Builder.CreateLoad(Type::getInt64Ty(Context), Index, "load_index");
                    CurrentIndex = Builder.CreateAdd(CurrentIndex, Builder.getInt64(1), "inc_index");
                    Builder.CreateStore(CurrentIndex, Index);
                }
                break;
            case '<':  // Decrement index
                {
                    Value *CurrentIndex = Builder.CreateLoad(Type::getInt64Ty(Context), Index, "load_index");
                    CurrentIndex = Builder.CreateSub(CurrentIndex, Builder.getInt64(1), "dec_index");
                    Builder.CreateStore(CurrentIndex, Index);
                }
                break;
            case '+':  // Increment byte at pointer
                {
                    Value *CurrentIndex = Builder.CreateLoad(Type::getInt64Ty(Context), Index, "load_index");
                    Value *Ptr = Builder.CreateGEP(TapeType, TapePtr, {Builder.getInt64(0), CurrentIndex}, "ptr_inc_val");
                    Value *Val = Builder.CreateLoad(Type::getInt8Ty(Context), Ptr, "load_val");
                    Val = Builder.CreateAdd(Val, Builder.getInt8(1), "inc_val");
                    Builder.CreateStore(Val, Ptr);
                }
                break;
            case '-':  // Decrement byte at pointer
                {
                    Value *CurrentIndex = Builder.CreateLoad(Type::getInt64Ty(Context), Index, "load_index");
                    Value *Ptr = Builder.CreateGEP(TapeType, TapePtr, {Builder.getInt64(0), CurrentIndex}, "ptr_dec_val");
                    Value *Val = Builder.CreateLoad(Type::getInt8Ty(Context), Ptr, "load_val");
                    Val = Builder.CreateSub(Val, Builder.getInt8(1), "dec_val");
                    Builder.CreateStore(Val, Ptr);
                }
                break;
            case '.':  // Output the byte at pointer
                {
                    Value *CurrentIndex = Builder.CreateLoad(Type::getInt64Ty(Context), Index, "load_index");
                    Value *Ptr = Builder.CreateGEP(TapeType, TapePtr, {Builder.getInt64(0), CurrentIndex}, "ptr_out");
                    Value *Val = Builder.CreateLoad(Type::getInt8Ty(Context), Ptr, "out_val");
                    Val = Builder.CreateSExt(Val, Type::getInt32Ty(Context), "sext_val");
                    Builder.CreateCall(PutCharFunc, Val);
                }
                break;
            case ',':  // Input a byte
                {
                    Value *CurrentIndex = Builder.CreateLoad(Type::getInt64Ty(Context), Index, "load_index");
                    Value *Ptr = Builder.CreateGEP(TapeType, TapePtr, {Builder.getInt64(0), CurrentIndex}, "ptr_in");
                    Value *Input = Builder.CreateCall(GetCharFunc);
                    Input = Builder.CreateTrunc(Input, Type::getInt8Ty(Context), "trunc_val");
                    Builder.CreateStore(Input, Ptr);
                }
                break;
            case '[':  // Start of loop
                {
                    BasicBlock *loopStart = BasicBlock::Create(Context, "loop_start", MainFunc);
                    BasicBlock *afterLoop = BasicBlock::Create(Context, "loop_end", MainFunc);

                    // Push loop start and end blocks onto stacks
                    loopStartStack.push(loopStart);
                    afterLoopStack.push(afterLoop);

                    Value *CurrentIndex = Builder.CreateLoad(Type::getInt64Ty(Context), Index, "load_index");
                    Value *Ptr = Builder.CreateGEP(TapeType, TapePtr, {Builder.getInt64(0), CurrentIndex}, "ptr_loop_start");
                    Value *valueAtPointer = Builder.CreateLoad(Type::getInt8Ty(Context), Ptr, "valueAtPointer");
                    Value *isZero = Builder.CreateICmpEQ(valueAtPointer, Builder.getInt8(0), "isZero");
                    Builder.CreateCondBr(isZero, afterLoop, loopStart);
                    Builder.SetInsertPoint(loopStart);
                }
                break;
            case ']':  // End of loop
                {
                    if (loopStartStack.empty() || afterLoopStack.empty()) {
                        cerr << "Error: unmatched ']' in Brainfuck code" << endl;
                        return;
                    }

                    BasicBlock *loopStart = loopStartStack.top();
                    BasicBlock *afterLoop = afterLoopStack.top();
                    loopStartStack.pop();
                    afterLoopStack.pop();

                    Value *CurrentIndex = Builder.CreateLoad(Type::getInt64Ty(Context), Index, "load_index");
                    Value *Ptr = Builder.CreateGEP(TapeType, TapePtr, {Builder.getInt64(0), CurrentIndex}, "ptr_loop_end");
                    Value *valueAtPointer = Builder.CreateLoad(Type::getInt8Ty(Context), Ptr, "valueAtPointer");
                    Value *isZero = Builder.CreateICmpEQ(valueAtPointer, Builder.getInt8(0), "isZero");
                    Builder.CreateCondBr(isZero, afterLoop, loopStart);
                    Builder.SetInsertPoint(afterLoop);
                }
                break;
            default:
                // Ignore any non-Brainfuck character
                break;
        }
    }

   
    Builder.CreateRet(ConstantInt::get(Type::getInt32Ty(Context), 0));

    // Setup optimization
    legacy::FunctionPassManager FPM(ModulePtr.get());
    legacy::PassManager PM;

    // Populate pass managers
    PassManagerBuilder PMBuilder;
    PMBuilder.OptLevel = 2; // Use 3 for -O3 level optimizations
    PMBuilder.populateFunctionPassManager(FPM);
    PMBuilder.populateModulePassManager(PM);

    // Add necessary passes
    FPM.doInitialization();
    for (Function &F : *ModulePtr)
        FPM.run(F);
    FPM.doFinalization();

    PM.run(*ModulePtr);

    // Verify and output the module
    std::error_code EC;
    raw_fd_ostream dest("output.ll", EC, sys::fs::OF_None);
    if (EC) {
        std::cerr << "Could not open file: " << EC.message() << std::endl;
        return;
    }

    if (verifyModule(*ModulePtr, &errs())) {
        std::cerr << "Error: Module verification failed." << std::endl;
        return;
    }

    ModulePtr->print(dest, nullptr);
    dest.close();
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <brainfuck_code_file>" << std::endl;
        return 1;
    }

    std::ifstream bf_file(argv[1]);
    if (!bf_file.is_open()) {
        std::cerr << "Error: Unable to open file " << argv[1] << std::endl;
        return 1;
    }

    std::string code((std::istreambuf_iterator<char>(bf_file)), std::istreambuf_iterator<char>());
    bf_file.close();

    generateLLVM(code);

    return 0;
}