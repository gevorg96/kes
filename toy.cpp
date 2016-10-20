#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace llvm;
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
		std::string NumStr;
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

namespace {

class ExprAST{									//базовый класс
	public:
	  virtual ~ExprAST(){}


	  virtual Value *codegen() = 0;
};


class NumExprAST : public ExprAST {			//узел выражения для числовых литералов
	  double Val;
	public:
	  NumExprAST (double val) : Val(val) {}

	  Value *codegen() override;
};


class VarExprAST : public ExprAST {				//узел выражения для переменных
	  std::string Name;
	public:
	  VarExprAST (const std::string &name) : Name(name) {}

	  Value *codegen() override;
};


class BinExprAST : public ExprAST{				//узел выражения для бинарных операторов
	  char Op;
	  ExprAST *LHS, *RHS;
	public:
	  BinExprAST (char op, ExprAST *lhs, ExprAST *rhs)
	  : Op(op), LHS(lhs), RHS(rhs) {}

	Value *codegen() override;
};


class CallExprAST : public ExprAST{				//узел выражения для вызова функции
	  std::string Callee;	
	  std::vector<ExprAST*> Args;
	public:
	  CallExprAST (const std::string &callee, std::vector<ExprAST*> &args)
	  : Callee(callee), Args(args) {}

	  Value *codegen() override;
};



class ProtoAST {							//прототип функции(хранит имя функции и имена арг.)
	  std::string Name;
	  std::vector<string> Args;
	public:
	  ProtoAST (const std::string &name, const std::vector<std::string> &args)
	  : Name(name), Args(args) {}
	  Function *codegen();
	  const std::string &getName() const {return Name;}
};


class FuncAST {								//узел выражения определения функции
	  ProtoAST *Proto;
	  ExprAST *Body;
	public:
	  FuncAST (ProtoAST *proto, ExprAST *body)
	  : Proto(proto), Body(body) {}

	  Function *codegen();
};
}

//----------------------
//	   Parser
//----------------------

	
static int CurTok;					//текущий токен
static int getNextTok() {				//смотрим на 1 токен вперёд
	return CurTok = gettok();
}

static std::map<char, int> BinopPrecedence;			//приоритеты бинарных операторов

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
	std::string Idname = IdStr;
	
	getNextTok();

	if (CurTok != '(') return new VarExprAST(Idname);	//переменная

	getNextTok();					//получаем "("
	std::vector<ExprAST*> Args;
	
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

	std::string FnName = IdStr;
	getNextTok();

	if (CurTok != '(')
    	  return ErrorP("Expected '(' in prototype");

	std::vector<string> ArgNames;					//считываем список аргументов

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


//--------------------------
//	Code Generation
//--------------------------

static Module *TheModule;
static IRBuilder<> Builder(getGlobalContext());
static std::map<std::string, Value*> NamedValues;


Value *ErrorV (const char *Str) {
	Error(Str);
	return 0;
}


Value *NumExprAST:: codegen() {
	return ConstantFP::get(getGlobalContext(), APFloat(Val));
}


Value *VarExprAST::codegen() {

	Value *V = NamedValues[Name];
	return V ? V : ErrorV("Unknown variable name.");
}

Value *BinExprAST::codegen() {
	Value *L = LHS->codegen();
	Value *R = RHS->codegen();
	if (!L || !R) return 0;

	switch(Op) {
	  case '+': return Builder.CreateFAdd(L, R, "addtmp");
	  case '-': return Builder.CreateFSub(L, R, "subtmp");
	  case '*': return Builder.CreateFMul(L, R, "multmp");
	  case '<': 
		L = Builder.CreateFCmpULT(L, R, "cmptmp");
		return Builder.CreateUIToFP(L, Type::getDoubleTy(getGlobalContext()), "booltmp");

	  default: return ErrorV("invalid binary operator");
	}
}

Value *CallExprAST:: codegen() {

	Function *CalleeF = TheModule->getFunction(Callee);
	if (CalleeF == 0)
	  return ErrorV("Unknown function referenced");

	if (CalleeF->arg_size() != Args.size())
	  return ErrorV("Incorrect # arguments passed");

	std::vector<Value*> ArgsV;
	for (unsigned i = 0, e = Args.size(); i != e; ++i) {
	  ArgsV.push_back(Args[i]->codegen());
	  if (ArgsV.back() == 0) return 0;
	}
	
	  return Builder.CreateCall(CalleeF, ArgsV, "calltmp");
}


Function *ProtoAST:: codegen() {
	std::vector <const Type*> Doubles(Args.size(), Type::getDoubleTy(getGlobalContext()));
	
	FunctionType *FT = FunctionType::get(Type::getDoubleTy(getGlobalContext()), Doubles, false);

	Function *F = Function::Create(FT, Function::ExternalLinkage, Name, TheModule);

	if (F->getName() != Name) {

	  F->eraseFromParent();
	  F = TheModule->getFunction(Name);

	  if (!F->empty()) {
		ErrorF("redefinition of function");
		return 0;
	  }

	  if (F->arg_size() != Args.size()) {
		ErrorF("redefinition of function with different # args");
		return 0;
	  }
	}


	unsigned Idx = 0;
	for (Function::arg_iterator AI = F->arg_begin(); Idx != Args.size(); ++AI, ++Idx) {
	  AI->setName(Args[Idx]);
	}

	return F;
}


Function *FuncAST:: codegen() {
	NamedValues.clear();

	Function *TheFunction = Proto->codegen();
	if (TheFunction == 0) return 0;

	BasicBlock *BB = BasicBlock::Create(getGlobalContext(), "entry", TheFunction);
	Builder.SetInsertPoint(BB);

	if (Value *RetVal = Body->codegen()) {
	  Builder.CreateRet(RetVal);
	  verifyFunction(*TheFunction);
	  return TheFunction;
	}
	
	TheFunction->eraseFromParent();
	return 0;
}



//----------------------------
//	Top-Level Parsing & JIT
//----------------------------


static void HandleDef() {
	if (FuncAST *F = ParseDef()) {
	  if (Function *LF = F->codegen()) {
	    fprintf(stderr, "Parsed a function definition.\n");
	    LF->dump();
	  }
	} else {
	  getNextTok();						//пропуск токена для восстановления после ошибки
	}
}


static void HandleExtern() {
	if (ProtoAST *P = ParseExtern()) {
	  if(Function *F = P->codegen()){
	    fprintf(stderr, "Parsed an extern.\n"); 
	    F->dump();
	  }	
	} else {
	  getNextTok();						//..
	} 
}

static void HandleTopLevelExpr() {
	if (FuncAST *F = ParseTopLevelExpr()) {
	  if(Function *LF = F->codegen()) {
	    fprintf(stderr, "Parsed a top-level expr.\n");
	    LF->dump();
	  }
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

	LLVMContext &Context = getGlobalContext();
								//задаём бинарные операторы
	BinopPrecedence['<'] = 10;
	BinopPrecedence['+'] = 20;
	BinopPrecedence['-'] = 20;
	BinopPrecedence['*'] = 30;

	fprintf(stderr, "ready> ");
	getNextTok();

	
	TheModule = new Module("my cool jit", Context);

	
	MainLoop();						//цикл интерпретатора
	
	TheModule->dump();

	return 0;
}







