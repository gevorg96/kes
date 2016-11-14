#pragma once

#include "ast.h"
#include <string>
#include <istream>
#include <iostream>

enum Token {
  // Конец файла
  tok_eof = -1,

  // Перевод строки
  tok_newline = -2,

  // Константа
  tok_const = -3,

  // Идентификатор
  tok_identifier = -4,

  // Лексемы условного оператора
  tok_if = -5,
  tok_else = -6,
  tok_end = -7,

  // Операторы сравнения
  tok_is = -8,
  tok_negative = -9,
  tok_zero = -10,
  tok_positive = -11,

  // Операторы ввода-вывода
  tok_input = -12,
  tok_print = -13
};

// Лексический анализатор.
class Lexer {
  std::istream &Input;
  int LastChar;

public:
  Lexer(std::istream &input)
      : Input(input), LastChar(' '), IdentifierName(""), ConstValue(0) {}

  ~Lexer() {}

  // Получение очередной лексемы из потока.
  int GetToken();

  // Публично доступные атрибуты лексемы.
  std::string IdentifierName;
  int ConstValue;
};

// Синтаксический анализатор.
// =====================================================================

/* Грамматика языка:
 *
 * Seq  -> Stmt*
 * Stmt -> Var '=' Expr
 *       | 'if' Expr 'is' ('negative' | 'zero' | 'positive')
 *         'then' Seq ['else' Seq] 'end'
 *       | 'input' Var
 *       | 'print' Var
 * Expr -> (const | ident) (('+' | '-') Expr)*
 */

class Parser {
  std::istream &Input;
  Lexer *Lex;
  int CurrentToken;

public:
  Parser(std::istream &input) : Input(input), ProgramIsValid(true) {
    Lex = new Lexer(Input);
    NextToken();
  }

  ~Parser() { delete Lex; }

  // Запуск синтаксического анализатора.
  StmtNode *Parse();
  bool ParserSuccess() const { return ProgramIsValid; }

private:
  // Парсеры для конструкций языка (рекурсивный спуск).
  StmtNode *ParseSeq();
  StmtNode *ParseStmt();
  ExprNode *ParseExpr();

  void NextToken() { CurrentToken = Lex->GetToken(); }

  void SkipNewline() {
    while (CurrentToken == tok_newline) {
      NextToken();
    }
  }

  // Обработка ошибок
  bool ProgramIsValid;

  StmtNode *StmtError(const std::string &message) {
    ProgramIsValid = false;
    return new StmtErrorNode(message);
  }

  ExprNode *ExprError(const std::string &message) {
    ProgramIsValid = false;
    return new ExprErrorNode(message);
  }

  void SkipAssign();
  void SkipToEndToken();
};
