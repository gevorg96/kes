#include "parser.h"

#include <cctype>
#include <ios>
#include <sstream>

// Лексический анализатор
// =====================================================================

static int isnewline(int c) { return (c == '\n' || c == '\r'); }

int Lexer::GetToken() {
  // Пропускаем ведущие пробелы и символы табуляции.
  while (LastChar == ' ' || LastChar == '\t') {
    LastChar = Input.get();
  }

  // Если буква - значит, идентификатор или ключевое слово.
  if (isalpha(LastChar)) {
    IdentifierName = LastChar;
    while (isalnum((LastChar = Input.get()))) {
      IdentifierName += LastChar;
    }

    // Ключевые слова
    if (IdentifierName == "is")
      return tok_is;
    if (IdentifierName == "if")
      return tok_if;
    if (IdentifierName == "else")
      return tok_else;
    if (IdentifierName == "end")
      return tok_end;
    if (IdentifierName == "negative")
      return tok_negative;
    if (IdentifierName == "zero")
      return tok_zero;
    if (IdentifierName == "positive")
      return tok_positive;
    if (IdentifierName == "input")
      return tok_input;
    if (IdentifierName == "print")
      return tok_print;

    // Если никакое ключевое слово не подошло, это идентификатор.
    return tok_identifier;
  }

  // Если цифра - значит, константа
  if (isdigit(LastChar)) {
    ConstValue = LastChar - '0';
    while (isdigit((LastChar = Input.get()))) {
      ConstValue = ConstValue * 10 + (LastChar - '0');
    }

    return tok_const;
  }

  // Если перевод строки, возвращаем специальный маркер.
  // Несколько символов перевода строки считаем одним.
  if (isnewline(LastChar)) {
    while (isnewline((LastChar = Input.get()))) {
      // Просто пропускаем остальные переводы строки.
    }

    return tok_newline;
  }

  // Проверим, не дошли ли мы до конца файла.
  if (Input.eof())
    return tok_eof;

  // Возвращаем сам символ, переходя при этом к следующему.
  int ThisChar = LastChar;
  LastChar = Input.get();
  return ThisChar;
}

// Синтаксический анализатор.
// =====================================================================

static std::string TokenErrorName(int token) {
  switch (token) {
  case tok_eof:
    return "end of file";
  case tok_newline:
    return "newline";
  case tok_const:
    return "integer constant";
  case tok_identifier:
    return "identifier";
  case tok_if:
    return "'if'";
  case tok_else:
    return "'else'";
  case tok_end:
    return "'end'";
  case tok_is:
    return "'is'";
  case tok_negative:
    return "'negative'";
  case tok_zero:
    return "'zero'";
  case tok_positive:
    return "'positive'";
  case tok_input:
    return "'input'";
  case tok_print:
    return "'print'";
  }

  std::stringstream ss;
  ss << "\"" << (char)token << "\"";
  return ss.str();
}

static CompareOp ParseCompareOp(int token) {
  CompareOp Op = ZERO;
  switch (token) {
  case tok_negative:
    Op = NEGATIVE;
    break;
  case tok_zero:
    Op = ZERO;
    break;
  case tok_positive:
    Op = POSITIVE;
    break;
  }

  return Op;
}

static BinaryOp ParseBinaryOp(int token) {
  if (token == '+') {
    return ADD;
  } else {
    return SUB;
  }
}

static std::string Expected(const std::string &msg, int found) {
  std::stringstream ss;
  ss << msg << " expected, but ";
  ss << TokenErrorName(found) << " found";
  return ss.str();
}

StmtNode *Parser::Parse() {
  auto Stmt = ParseSeq();
  if (CurrentToken != tok_eof) {
    auto Prog = new SeqNode();
    Prog->Add(Stmt);
    Prog->Add(StmtError(Expected("End of file", CurrentToken)));
    return Prog;
  }

  return Stmt;
}

StmtNode *Parser::ParseStmt() {
  if (CurrentToken == tok_identifier) {
    std::string IdName = Lex->IdentifierName;
    NextToken();
    if (CurrentToken == '=') {
      NextToken();
      return new AssignNode(IdName, ParseExpr());
    } else {
      SkipAssign();
      return StmtError(Expected("'='", CurrentToken));
    }
  } else if (CurrentToken == tok_if) {
    ExprNode *Cond;
    CompareOp Op;
    StmtNode *Then = nullptr, *Else = nullptr;

    NextToken();
    SkipNewline();
    Cond = ParseExpr();
    SkipNewline();
    if (CurrentToken == tok_is) {
      NextToken();
      SkipNewline();
      if (CurrentToken == tok_negative || CurrentToken == tok_zero ||
          CurrentToken == tok_positive) {
        Op = ParseCompareOp(CurrentToken);
        NextToken();
        Then = ParseSeq();
        if (CurrentToken == tok_else) {
          NextToken();
          Else = ParseSeq();
        }

        if (CurrentToken == tok_end) {
          NextToken();
          return new IfNode(Op, Cond, Then, Else);
        } else { // Должна быть лексема tok_end (или tok_else).
          return StmtError(Expected("'end' or 'else'", CurrentToken));
        }
      } else { // Должен быть оператор сравнения, но его нет.
        int FoundToken = CurrentToken;
        SkipToEndToken();
        return StmtError(
            Expected("'negative', 'zero' or 'positive'", FoundToken));
      }
    } else { // Должна быть лексема tok_is, но ее нет.
      int FoundToken = CurrentToken;
      SkipToEndToken();
      return StmtError(Expected("'is'", FoundToken));
    }
  } else if (CurrentToken == tok_input) {
    NextToken();
    SkipNewline();
    if (CurrentToken == tok_identifier) {
      auto Stmt = new InputNode(Lex->IdentifierName);
      NextToken();
      return Stmt;
    } else {
      return StmtError(Expected("Identifier", CurrentToken));
    }
  } else if (CurrentToken == tok_print) {
    NextToken();
    SkipNewline();
    return new PrintNode(ParseExpr());
  } else {
    // Этот код на самом деле никогда не выполняется,
    // так как в ParseSeq есть проверка на то,
    // что оператор начинается с правильной лексемы.

    int FoundToken = CurrentToken;
    NextToken();
    return StmtError(Expected("Assignment, if, input or print", FoundToken));
  }
}

StmtNode *Parser::ParseSeq() {
  auto Seq = new SeqNode();

  while (true) {
    // Пропускаем переводы строки,
    // пока не дойдем до очередного оператора.
    SkipNewline();

    // Оператор должен начинаться с одной из лексем,
    // входящих в FIRST(Stmt).
    if (CurrentToken == tok_identifier || CurrentToken == tok_if ||
        CurrentToken == tok_input || CurrentToken == tok_print) {
      Seq->Add(ParseStmt());
    } else {
      // Если лексема не подходит, значит,
      // список операторов закончился.
      break;
    }
  }

  // Вернем накопленный список операторов.
  return Seq;
}

ExprNode *Parser::ParseExpr() {
  ExprNode *Expr = nullptr;

  if (CurrentToken == tok_const) {
    Expr = new ConstNode(Lex->ConstValue);
  } else if (CurrentToken == tok_identifier) {
    Expr = new VarNode(Lex->IdentifierName);
  } else { // Ошибка, сообщаем и выходим.
    return ExprError(Expected("Constant or variable", CurrentToken));
  }

  NextToken();
  SkipNewline();
  while (CurrentToken == '+' || CurrentToken == '-') {
    BinaryOp Op = ParseBinaryOp(CurrentToken);
    NextToken();
    SkipNewline();
    if (CurrentToken == tok_const) {
      Expr = new BinaryNode(Op, Expr, new ConstNode(Lex->ConstValue));
    } else if (CurrentToken == tok_identifier) {
      Expr = new BinaryNode(Op, Expr, new VarNode(Lex->IdentifierName));
    } else {
      // Ошибка: после знака операции нет выражения.
      Expr = new BinaryNode(
          Op, Expr, ExprError(Expected("Constant or variable", CurrentToken)));
      return Expr;
    }

    NextToken();
    SkipNewline();
  }

  return Expr;
}

void Parser::SkipAssign() {
  while (CurrentToken != tok_newline && CurrentToken != tok_eof) {
    NextToken();
  }
}

void Parser::SkipToEndToken() {
  while (CurrentToken != tok_eof && CurrentToken != tok_end) {
    NextToken();
  }
}
