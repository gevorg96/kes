#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

using namespace std;

//--------------------
//   	 Lexer
//--------------------

enum Token{
tok_eof = -1,

tok_def = -2, tok_extern = -3,

tok_identifier = -4, tok_number = -5

};


static string IdStr;
static double NumVal;

static int gettok() 
{
	
	static int LastCh = ' ';

	while(isspace(LastCh))				//пропуск пробелов
		LastCh = getchar();

	if(isalpha(LastCh)){		
		IdStr = LastCh;				//идентефикаторы

		while(isalnum((LastCh = getchar())))
		  IdStr += LastCh;

		if(IdStr == "def") return tok_def;
		if(IdStr == "extern") return tok_extern;
		return tok_identifier; 
	}

	if(isdigit(LastCh) || LastCh == '.'){		//число 0-9.
		string NumStr;
	do{
		NumStr += LastCh;
		LastCh = getchar();

	} while(isdigit(LastCh) || LastCh == '.');

	 NumVal = strtod(NumStr.c_str(), 0);
	 return tok_number;
	}

	
	if(LastCh == '#') {							//комментарий
		do LastCh = getchar();
		while(LastCh != EOF && LastCh != '\n' && LastCh != '\r');

	if (LastCh != EOF)
		return gettok();
	}

	
	if (LastCh == EOF) 						//проверка конца файла
 		return tok_eof;

	int ThisCh = LastCh;						//иначе
	LastCh = getchar();
	return ThisCh;
}


//--------------------
//	  AST
//--------------------


class ExprAST{									//базовый класс
	public:
	  virtual ~ExprAST(){}
};


class NumExprAST : public ExprAST {			//узел выражения для числовых литералов
	  double Val;
	public:
	  NumExprAST (double val) : Val(val) {}
};


class VarExprAST : public ExprAST {				//узел выражения для переменных
	  string Name;
	public:
	  VarExprAST (const string &name) : Name(name) {}
};


class BinExprAST : public ExprAST{				//узел выражения для бинарных операторов
	  char Op;
	  ExprAST *LHS, *RHS;
	public:
	  BinExprAST (char op, ExprAST *lhs, ExprAST *rhs)
	  : Op(op), LHS(lhs), RHS(rhs) {}
};


class CallExprAST : public ExprAST{				//узел выражения для вызова функции
	  string Callee;	
	  vector<ExprAST*> Args;
	public:
	  CallExprAST (const string &callee, vector<ExprAST*> &args)
	  : Callee(callee), Args(args) {}
};



class ProtoAST {							//прототип функции(хранит имя функции и имена арг.)
	  string Name;
	  vector<string> Args;
	public:
	  ProtoAST (const string &name, const vector<string> &args)
	  : Name(name), Args(args) {}
};


class FuncAST {								//узел выражения определения функции
	  ProtoAST *Proto;
	  ExprAST *Body;
	public:
	  FuncAST (ProtoAST *proto, ExprAST *body)
	  : Proto(proto), Body(body) {}
};


//----------------------
//	   Parser
//----------------------

	
static int CurTok;					//текущий токен
static int getNextTok() {				//смотрим на 1 токен вперёд
	return CurTok = gettok();
}

static map<char, int> BinopPrecedence;			//приоритеты бинарных операторов

static int GetTokPrec() {				//возвращает приоритет текущего бинарного оператора
	if (!isascii(CurTok))
	  return -1;

	int TokPrec = BinopPrecedence[CurTok];		//проверка, является ли токен бинарным оператором
	if (TokPrec <= 0) return -1;
	return TokPrec;
}

		
ExprAST *Error (const char *Str) { 			//Обработчики ошибок
	fprintf(stderr, "Error: %s\n", Str);		//	
	return 0;					//
}							//

ProtoAST *ErrorP (const char *Str) {			//
	Error(Str);					//
	return 0;					//
}							//

FuncAST *ErrorF (const char *Str) {			//
	Error(Str);					//
	return 0;					//
}


static ExprAST *ParseExpr();


static ExprAST *ParseIdExpr() {				//парсер идентефикаторов
	string Idname = IdStr;
	
	getNextTok();

	if (CurTok != '(') return new VarExprAST(Idname);	//переменная

	getNextTok();					//получаем "("
	vector<ExprAST*> Args;
	
	if (CurTok != ')') {
	  while(1) {

		ExprAST *Arg = ParseExpr();
		if (!Arg) return 0;
		Args.push_back(Arg);

		if (CurTok == ')') break;

		if (CurTok != ',') 
		  return Error("Expected ')' or ',' in arg list");
		getNextTok();
	  }
	}

	getNextTok();

	return new CallExprAST(Idname, Args);
}


static ExprAST *ParseNumExpr() {				//парсер чисел
	ExprAST *Result = new NumExprAST(NumVal);
	getNextTok();
	return Result;
}

static ExprAST *ParseParExpr() {				//парсер выражений в скобках
	getNextTok();
	
	ExprAST *V = ParseExpr();
	if (!V) return 0;

	if ( CurTok != ')')	return Error ("Expected ')'");
	
	getNextTok();

	return V;
}


static ExprAST *ParsePrimary() {				//парсер произвольного первичного выражения
	switch (CurTok) {

	  default: return Error ("unknown token when expecting an expression");
	  case tok_identifier: 	return ParseIdExpr();
	  case tok_number: 	return ParseNumExpr();
	  case '(':		return ParseParExpr();
	}
}


static ExprAST *ParseBinopRHS ( int ExprPrec, ExprAST *LHS) {

	while(1) {
	  int TokPrec = GetTokPrec();				//получаем приоритет бинарного оператора
	

	  if (TokPrec < ExprPrec) return LHS;		//используем оператор

	  int Binop = CurTok;
	  getNextTok();								//съесть оператор

	  ExprAST *RHS = ParsePrimary();			//разбор первичного выражения после бин.оп.
	  if (!RHS) return 0;

	  int NextPrec = GetTokPrec();				//если оп. связан с RHS меньшим приоритетом,
	  if (TokPrec < NextPrec) {					//чем оп. после RHS, то берём часть вместе с RHS как LHS
		RHS = ParseBinopRHS (TokPrec+1, RHS);
		if (!RHS) return 0;
	  }

	LHS = new BinExprAST(Binop, LHS, RHS);		//собираем операнды
	}
}


static ExprAST *ParseExpr() {
	
	ExprAST *LHS = ParsePrimary();
	if (!LHS) return 0;

	return ParseBinopRHS (0, LHS);
}


static ProtoAST *ParseProto() {					//парсер прототипов функций

	if (CurTok != tok_identifier)
	  return ErrorP ("Expected func name in prototype");

	string FnName = IdStr;
	getNextTok();

	if (CurTok != '(')
    	  return ErrorP("Expected '(' in prototype");

	vector<string> ArgNames;					//считываем список аргументов

	while (getNextTok() == tok_identifier)
		ArgNames.push_back (IdStr);

	if (CurTok != ')')
		return ErrorP ("Expected ')' in prototype");

	getNextTok();								//получаем ")"

	return new ProtoAST (FnName, ArgNames);
}


static FuncAST *ParseDef() {
	getNextTok();
	
	ProtoAST *Proto = ParseProto();
	if (Proto == 0) return 0;

	if (ExprAST *E = ParseExpr()) return new FuncAST (Proto, E);
	return 0;
}



static FuncAST *ParseTopLevelExpr() {			//парсинг выражений верхнего уровня

	if (ExprAST *E = ParseExpr()) {
		ProtoAST *Proto = new ProtoAST ("", vector<string>());
		return new FuncAST(Proto, E);
	}
	
	return 0;
}


static ProtoAST *ParseExtern() {

	getNextTok();								//получаем extern
	
	return ParseProto();
}


//----------------------------
//	Top-Level Parsing
//----------------------------


static void HandleDef() {
	if (ParseDef()) {
	  fprintf(stderr, "Parsed a function definition.\n");
	} else {
	  getNextTok();						//пропуск токена для восстановления после ошибки
	}
}


static void HandleExtern() {
	if (ParseExtern()) {
	  fprintf(stderr, "Parsed an extern.\n"); 
	} else {
	  getNextTok();						//..
	} 
}

static void HandleTopLevelExpr() {
	if (ParseTopLevelExpr()) {
	  fprintf(stderr, "Parsed a top-level expr.\n");
	} else {
	   getNextTok();					//..
	}
}

static void MainLoop() {					//top = def| external| expr| ';'
	while (1) {
	  fprintf(stderr, "ready> ");
	  switch (CurTok) {
		case tok_eof:		return;
		case ';': 		getNextTok(); break;	//игнорируем ';' верхнего уровня
		case tok_def:		HandleDef(); break;
		case tok_extern:	HandleExtern(); break;
		default:		HandleTopLevelExpr(); break;
	  }
	}
}


//---------------------------
//	Main driver code
//---------------------------

int main() {
								//задаём бинарные операторы
	BinopPrecedence['<'] = 10;
	BinopPrecedence['+'] = 20;
	BinopPrecedence['-'] = 20;
	BinopPrecedence['*'] = 30;

	fprintf(stderr, "ready> ");
	getNextTok();

	MainLoop();						//цикл интерпретатора
	
	return 0;
}






