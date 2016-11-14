#pragma once

#include "generator.h"
#include <llvm/IR/IRBuilder.h>
#include <string>
#include <set>

// Бинарные операторы
enum BinaryOp {
  ADD, // Сложение
  SUB  // Вычитание
};

// Операторы сравнения
enum CompareOp {
  NEGATIVE, // Значение меньше нуля
  ZERO,     // Значение равно нулю
  POSITIVE  // Значение больше нуля
};

// Выражение (абстрактный базовый класс).
class ExprNode {
public:
  virtual ~ExprNode() {}
  virtual llvm::Value *Generate(GeneratorState *) = 0;
  virtual std::set<std::string> GetVariables() = 0;
  virtual void Format(std::ostream &) = 0;
};

// Выражение с ошибкой.
// Инструкции для этого узла не генерируются (возвращается nullptr).
class ExprErrorNode : public ExprNode {
public:
  ExprErrorNode(const std::string &message) : Message(message) {}

  virtual llvm::Value *Generate(GeneratorState *) { return nullptr; }

  virtual std::set<std::string> GetVariables() {
    return std::set<std::string>();
  }

  virtual void Format(std::ostream &out);

  // Публично доступное сообщение об ошибке.
  std::string Message;
};

// Целочисленная константа.
class ConstNode : public ExprNode {
  int Val;

public:
  ConstNode(int val) : Val(val) {}
  virtual llvm::Value *Generate(GeneratorState *);
  virtual std::set<std::string> GetVariables();
  virtual void Format(std::ostream &os);
};

// Переменная
class VarNode : public ExprNode {
  std::string Name;

public:
  VarNode(const std::string &name) : Name(name) {}
  virtual llvm::Value *Generate(GeneratorState *);
  virtual std::set<std::string> GetVariables();
  virtual void Format(std::ostream &os);
};

// Применение бинарной операции
class BinaryNode : public ExprNode {
  BinaryOp Op;
  ExprNode *LHS;
  ExprNode *RHS;

public:
  BinaryNode(BinaryOp op, ExprNode *lhs, ExprNode *rhs)
      : Op(op), LHS(lhs), RHS(rhs) {}
  virtual llvm::Value *Generate(GeneratorState *);
  virtual std::set<std::string> GetVariables();
  virtual void Format(std::ostream &os);
};

// Оператор (абстрактный базовый класс)
class StmtNode {
public:
  virtual ~StmtNode() {}

  // Признак действительности узла. Узел, не являющийся ошибкой,
  // считается корректным, и с ним можно дальше работать.
  virtual bool IsValid() const { return true; }

  virtual void Generate(GeneratorState *) = 0;
  virtual std::set<std::string> GetVariables() = 0;
  virtual void Format(std::ostream &out, int indent) = 0;
};

// Оператор с ошибкой.
// Инструкции для этого узла не генерируются.
class StmtErrorNode : public StmtNode {
public:
  StmtErrorNode(const std::string &message) : Message(message) {}

  // В случае ошибки узел считается недействительным.
  virtual bool IsValid() const { return false; }

  virtual void Generate(GeneratorState *) {}

  virtual std::set<std::string> GetVariables() {
    return std::set<std::string>();
  }

  virtual void Format(std::ostream &out, int indent);

  // Публично доступное сообщение об ошибке.
  std::string Message;
};

// Последовательность операторов
class SeqNode : public StmtNode {
  std::vector<StmtNode *> Statements;

public:
  SeqNode() {}

  void Add(StmtNode *stmt) { Statements.push_back(stmt); }

  virtual void Generate(GeneratorState *);
  virtual std::set<std::string> GetVariables();
  virtual void Format(std::ostream &out, int indent);
};

// Оператор присваивания
class AssignNode : public StmtNode {
  std::string Name;
  ExprNode *RHS;

public:
  AssignNode(const std::string &name, ExprNode *rhs) : Name(name), RHS(rhs) {}
  virtual void Generate(GeneratorState *);
  virtual std::set<std::string> GetVariables();
  virtual void Format(std::ostream &out, int indent);
};

// Условный оператор
class IfNode : public StmtNode {
  CompareOp Op;
  ExprNode *Cond;
  StmtNode *Then;
  StmtNode *Else;

public:
  IfNode(CompareOp op, ExprNode *cond, StmtNode *thenBlock, StmtNode *elseBlock)
      : Op(op), Cond(cond), Then(thenBlock), Else(elseBlock) {}

  virtual void Generate(GeneratorState *);
  virtual std::set<std::string> GetVariables();
  virtual void Format(std::ostream &out, int indent);
};

// Оператор печати
class PrintNode : public StmtNode {
  ExprNode *RHS;

public:
  PrintNode(ExprNode *rhs) : RHS(rhs) {}
  virtual void Generate(GeneratorState *);
  virtual std::set<std::string> GetVariables();
  virtual void Format(std::ostream &out, int indent);
};

// Оператор ввода
class InputNode : public StmtNode {
  std::string Name;

public:
  InputNode(const std::string &name) : Name(name) {}
  virtual void Generate(GeneratorState *);
  virtual std::set<std::string> GetVariables();
  virtual void Format(std::ostream &out, int indent);
};
