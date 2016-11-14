#pragma once

#include <llvm/Analysis/Verifier.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <map>
#include <set>
#include <string>

using namespace llvm;

class GeneratorState {
public:
  IRBuilder<> *Builder;
  Module *MainModule;

  // Функции
  Function *Main;
  Function *BuiltinPrint;
  Function *BuiltinInput;

  std::map<std::string, AllocaInst *> Variables;

  GeneratorState() {
    Builder = new IRBuilder<>(getGlobalContext());
    MainModule = new Module("toycompiler", getGlobalContext());

    CreatePrototypes();
  }

  ~GeneratorState() {
    Main = nullptr;
    BuiltinPrint = nullptr;
    BuiltinInput = nullptr;
  }

  LLVMContext &GetContext() { return getGlobalContext(); }

  bool Verify() { return verifyModule(*MainModule); }
  
  void Optimize();

  IRBuilder<> *GetBuilder() const { return Builder; }

  Module *GetMainModule() const { return MainModule; }

  BasicBlock *GetMainEntryBlock() const { return &Main->getEntryBlock(); }

  AllocaInst *GetVar(const std::string &name) { return Variables[name]; }

  void AddVar(const std::string &name, AllocaInst *var) {
    Variables[name] = var;
  }
  void AddVariables(const std::set<std::string> &Variables);

private:
  void CreatePrototypes();
};
