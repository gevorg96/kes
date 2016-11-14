#include "ast.h"
#include "generator.h"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/Module.h>
#include <llvm/PassManager.h>
#include <llvm/Analysis/Passes.h>
#include <llvm/Transforms/Scalar.h>
#include <algorithm>
#include <iostream>

using namespace llvm;

// Генерация кода
// ===========================================

Value *ConstNode::Generate(GeneratorState *) {
  return ConstantInt::get(getGlobalContext(), APInt(32, Val));
}

Value *VarNode::Generate(GeneratorState *gen) {
  Value *V = gen->GetVar(Name);
  // Так как мы заранее создаем все переменные, переменная
  // обязательно должна быть, так что V != nullptr.
  return gen->GetBuilder()->CreateLoad(V, Name.c_str());
}

Value *BinaryNode::Generate(GeneratorState *gen) {
  Value *L = LHS->Generate(gen);
  Value *R = RHS->Generate(gen);
  if (L == nullptr || R == nullptr) {
    return nullptr;
  }

  Value *V = nullptr;
  switch (Op) {
  case ADD:
    V = gen->GetBuilder()->CreateAdd(L, R);
    break;
  case SUB:
    V = gen->GetBuilder()->CreateSub(L, R);
    break;
  }

  return V;
}

// Генерация кода для операторов
// -------------------------------------------------------------
void SeqNode::Generate(GeneratorState *gen) {
  for (auto stmt : Statements) {
    stmt->Generate(gen);
  }
}

void AssignNode::Generate(GeneratorState *gen) {
  AllocaInst *lhs = gen->GetVar(Name);
  Value *rhs = RHS->Generate(gen);
  gen->GetBuilder()->CreateStore(rhs, lhs);
}

void IfNode::Generate(GeneratorState *gen) {
  Value *cond;
  Value *arg = this->Cond->Generate(gen);
  Value *zero = ConstantInt::get(gen->GetContext(), APInt(32, 0));

  if (Op == NEGATIVE) {
    cond = gen->Builder->CreateICmpSLT(arg, zero);
  } else if (Op == ZERO) {
    cond = gen->Builder->CreateICmpEQ(arg, zero);
  } else {
    cond = gen->Builder->CreateICmpSGT(arg, zero);
  }

  BasicBlock *t = BasicBlock::Create(gen->GetContext(), "then", gen->Main);
  BasicBlock *e = BasicBlock::Create(gen->GetContext(), "else", gen->Main);
  BasicBlock *m = BasicBlock::Create(gen->GetContext(), "merge", gen->Main);

  gen->Builder->CreateCondBr(cond, t, e);

  // Блок then
  gen->Builder->SetInsertPoint(t);
  Then->Generate(gen);
  // Then = gen->Builder->GetInsertBlock();
  gen->Builder->CreateBr(m);

  // Блок else
  gen->Builder->SetInsertPoint(e);
  if (Else != nullptr) {
    Else->Generate(gen);
  }
  // Else = gen->Builder->GetInsertBlock();
  gen->Builder->CreateBr(m);

  // Слияние ветвей
  gen->Builder->SetInsertPoint(m);
}

void PrintNode::Generate(GeneratorState *gen) {
  Value *rhs = RHS->Generate(gen);
  gen->Builder->CreateCall(gen->BuiltinPrint, rhs);
}

void InputNode::Generate(GeneratorState *gen) {
  CallInst *val = gen->Builder->CreateCall(gen->BuiltinInput);
  gen->Builder->CreateStore(val, gen->GetVar(Name));
}

// Получение списка переменных
// ------------------------------------------------------------------
std::set<std::string> ConstNode::GetVariables() {
  return std::set<std::string>();
}

std::set<std::string> VarNode::GetVariables() {
  std::set<std::string> Vars;
  Vars.insert(Name);
  return Vars;
}

std::set<std::string> BinaryNode::GetVariables() {
  auto Vars = LHS->GetVariables();
  auto RHSVars = RHS->GetVariables();
  Vars.insert(RHSVars.begin(), RHSVars.end());
  return Vars;
}

std::set<std::string> SeqNode::GetVariables() {
  std::set<std::string> Vars;
  for (auto stmt : Statements) {
    auto ThisStmtVars = stmt->GetVariables();
    Vars.insert(ThisStmtVars.begin(), ThisStmtVars.end());
  }

  return Vars;
}

std::set<std::string> AssignNode::GetVariables() {
  auto Vars = RHS->GetVariables();
  Vars.insert(Name);
  return Vars;
}

std::set<std::string> IfNode::GetVariables() {
  auto Vars = Cond->GetVariables();
  auto ThenVars = Then->GetVariables();
  Vars.insert(ThenVars.begin(), ThenVars.end());

  if (Else != nullptr) {
    auto ElseVars = Else->GetVariables();
    Vars.insert(ElseVars.begin(), ElseVars.end());
  }

  return Vars;
}

std::set<std::string> PrintNode::GetVariables() { return RHS->GetVariables(); }

std::set<std::string> InputNode::GetVariables() {
  std::set<std::string> Vars;
  Vars.insert(Name);
  return Vars;
}

// Состояние генератора
void GeneratorState::CreatePrototypes() {
  BuiltinPrint = Function::Create(
      FunctionType::get(
          Type::getVoidTy(getGlobalContext()),
          std::vector<Type *>(1, Type::getInt32Ty(getGlobalContext())), false),
      Function::ExternalLinkage, "builtin_print", MainModule);

  BuiltinInput =
      Function::Create(FunctionType::get(Type::getInt32Ty(getGlobalContext()),
                                         std::vector<Type *>(), false),
                       Function::ExternalLinkage, "builtin_input", MainModule);
  Main = Function::Create(FunctionType::get(Type::getVoidTy(getGlobalContext()),
                                            std::vector<Type *>(), false),
                          Function::ExternalLinkage, "main", MainModule);
  BasicBlock *BB = BasicBlock::Create(getGlobalContext(), "entry", Main);
  Builder->SetInsertPoint(BB);
}

void GeneratorState::AddVariables(const std::set<std::string> &Variables) {
  BasicBlock *Root = GetMainEntryBlock();
  IRBuilder<> VarBuilder(Root, Root->begin());

  for (auto var : Variables) {
    AddVar(var, VarBuilder.CreateAlloca(Type::getInt32Ty(getGlobalContext()),
                                        0, var));
  }
}

void GeneratorState::Optimize()
{
  FunctionPassManager fpm(MainModule);
  fpm.add(createBasicAliasAnalysisPass());
  fpm.add(createPromoteMemoryToRegisterPass());
  fpm.add(createInstructionCombiningPass());
  fpm.add(createReassociatePass());
  fpm.add(createGVNPass());
  fpm.add(createCFGSimplificationPass());
  fpm.doInitialization();
  fpm.run(*Main);
}

// Реализация генератора
Module *Generate(StmtNode *Prog) {
  GeneratorState Gen;

  Gen.AddVariables(Prog->GetVariables());
  Prog->Generate(&Gen);
  Gen.Builder->CreateRetVoid();
  Gen.Optimize();

  return Gen.GetMainModule();
}

