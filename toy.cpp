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


static string IdentifierStr;
static double NumVal;

static int gettok() 
{
	
	static int LastChar = ' ';

	while(isspace(LastChar))				//пропуск пробелов
		LastChar = getchar();

	if(isalpha(LastChar)){		
		IdentifierStr = LastChar;				//идентефикаторы

		while(isalnum((LastChar = getchar())))
		  IdentifierStr += LastChar;

		if(IdentifierStr == "def") return tok_def;
		if(IdentifierStr == "extern") return tok_extern;
		return tok_identifier; 
	}

	if(isdigit(LastChar) || LastChar == '.'){		//число 0-9.
		string NumStr;
	do{
		NumStr += LastChar;
		LastChar = getchar();

	} while(isdigit(LastChar) || LastChar == '.');

	 NumVal = strtod(NumStr.c_str(), 0);
	 return tok_number;
	}

	
	if(LastChar == '#') {							//комментарий
		do LastChar = getchar();
		while(LastChar != EOF && LastChar != '\n' && LastChar != '\r');

	if (LastChar != EOF)
		return gettok();
	}

	
	if (LastChar == EOF) 						//проверка конца файла
 		return tok_eof;

	int ThisChar = LastChar;						//иначе
	LastChar = getchar();
	return ThisChar;
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


class NumberExprAST : public ExprAST {			//узел выражения для числовых литералов
	  double Val;
	public:
	  NumberExprAST (double Val) : Val(move(Val)) {}

	  Value *codegen() override;
};


class VariableExprAST : public ExprAST {				//узел выражения для переменных
	  string Name;
	public:
	  VariableExprAST (const string &Name) : Name(Name) {}

	  Value *codegen() override;
};


class BinaryExprAST : public ExprAST{				//узел выражения для бинарных операторов
	  char Op;
	  unique_ptr<ExprAST> LHS, RHS;
	public:
	  BinaryExprAST (char Op, unique_ptr<ExprAST> LHS, unique_ptr<ExprAST> RHS)
	  : Op(Op), LHS(move(LHS)), RHS(move(RHS)) {}

	Value *codegen() override;
};


class CallExprAST : public ExprAST{				//узел выражения для вызова функции
	  string Callee;	
	  vector<unique_ptr<ExprAST>> Args;
	public:
	  CallExprAST (const string &Callee, vector<unique_ptr<ExprAST>> Args)
	  : Callee(Callee), Args(move(Args)) {}

	  Value *codegen() override;
};



class PrototypeAST {							//прототип функции(хранит имя функции и имена арг.)
	  string Name;
	  vector<string> Args;
	public:
	  PrototypeAST (const string &Name, const vector<string> Args)
	  : Name(Name), Args(move(Args)) {}
	  Function *codegen();
	  const string &getName() const {return Name;}
};


class FunctionAST {								//узел выражения определения функции
	  unique_ptr<PrototypeAST> Proto;
	  unique_ptr<ExprAST> Body;
	public:
	  FunctionAST (unique_ptr<PrototypeAST> Proto, unique_ptr<ExprAST> Body)
	  : Proto(move(Proto)), Body(move(Body)) {}

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

static map<char, int> BinopPrecedence;			//приоритеты бинарных операторов

static int GetTokPrecedence() {				//возвращает приоритет текущего бинарного оператора
	if (!isascii(CurTok))
	  return -1;

	int TokPrec = BinopPrecedence[CurTok];		//проверка, является ли токен бинарным оператором
	if (TokPrec <= 0) return -1;
	return TokPrec;
}

		
unique_ptr<ExprAST> Error (const char *Str) { 			//Обработчики ошибок
	fprintf(stderr, "Error: %s\n", Str);		//	
	return nullptr;					//
}							//

unique_ptr<PrototypeAST> ErrorP (const char *Str) {			//
	Error(Str);					//
	return nullptr;					//
}							//



static unique_ptr<ExprAST> ParseExpression();




static unique_ptr<ExprAST> ParseNumberExpr() {				//парсер чисел
	auto Result = llvm::make_unique<NumberExprAST>(NumVal);
	getNextTok();
	return move(Result);
}

static unique_ptr<ExprAST> ParseParentExpr() {				//парсер выражений в скобках
	getNextTok();
	
	auto V = ParseExpression();
	if (!V) return nullptr;

	if ( CurTok != ')')	return Error("Expected ')'");
	
	getNextTok();

	return V;
}


static unique_ptr<ExprAST> ParseIdentifierExpr() {				//парсер идентефикаторов
	string IdName = IdentifierStr;
	
	getNextTok();

	if (CurTok != '(') 
		return llvm::make_unique<VariableExprAST>(IdName);	//переменная

	getNextTok();					//получаем (
	vector<unique_ptr<ExprAST>> Args;
	
	if (CurTok != ')') {
	  while(1) {

		if(auto Arg = ParseExpression())
			Args.push_back(move(Arg));
		else return nullptr;

		if (CurTok == ')') 
			break;

		if (CurTok != ',') 
		  return Error("Expected ')' or ',' in arg list");
		getNextTok();
	  }
	}

	getNextTok();

	return llvm::make_unique<CallExprAST>(IdName, move(Args));
}




static unique_ptr<ExprAST> ParsePrimary() {				//парсер произвольного первичного выражения
	switch (CurTok) {

	  default: return Error ("unknown token when expecting an expression");
	  case tok_identifier: 	return ParseIdentifierExpr();
	  case tok_number: 		return ParseNumberExpr();
	  case '(':			return ParseParentExpr();
	}
}


static unique_ptr<ExprAST> ParseBinOpRHS ( int ExprPrec, unique_ptr<ExprAST> LHS) {

	while(1) {
	  int TokPrec = GetTokPrecedence();				//получаем приоритет бинарного оператора
	

	  if (TokPrec < ExprPrec) return LHS;		//используем оператор

	  int Binop = CurTok;
	  getNextTok();								//съесть оператор

	  auto RHS = ParsePrimary();			//разбор первичного выражения после бин.оп.
	  if (!RHS) return nullptr;

	  int NextPrec = GetTokPrecedence();				//если оп. связан с RHS меньшим приоритетом,
	  if (TokPrec < NextPrec) {					//чем оп. после RHS, то берём часть вместе с RHS как LHS
		RHS = ParseBinOpRHS (TokPrec+1, move(RHS));
		if (!RHS) return nullptr;
	  }

	LHS = llvm::make_unique<BinaryExprAST>(Binop, move(LHS), move(RHS));		//собираем операнды
	}
}


static unique_ptr<ExprAST> ParseExpression() {
	
	auto LHS = ParsePrimary();
	if (!LHS) return nullptr;

	return ParseBinOpRHS (0, move(LHS));
}


static unique_ptr<PrototypeAST> ParsePrototype() {					//парсер прототипов функций

	if (CurTok != tok_identifier)
	  return ErrorP ("Expected func name in prototype");

	string FnName = IdentifierStr;
	getNextTok();

	if (CurTok != '(')
    	  return ErrorP("Expected '(' in prototype");

	vector<string> ArgNames;					//считываем список аргументов

	while (getNextTok() == tok_identifier)
		ArgNames.push_back(IdentifierStr);

	if (CurTok != ')')
		return ErrorP("Expected ')' in prototype");

	getNextTok();								//получаем ")"

	return llvm::make_unique <PrototypeAST> (FnName, move(ArgNames));
}


static unique_ptr<FunctionAST> ParseDefinition() {
	getNextTok();
	
	auto Proto = ParsePrototype();
	if (!Proto) return nullptr;

	if (auto E = ParseExpression()) 
		return llvm::make_unique <FunctionAST> (move(Proto), move(E));
	return nullptr;
}



static unique_ptr<FunctionAST> ParseTopLevelExpr() {			//парсинг выражений верхнего уровня

	if (auto E = ParseExpression()) {
		auto Proto = llvm::make_unique <PrototypeAST> ("____anon_expr", vector<string>());
		return llvm::make_unique <FunctionAST> (move(Proto), move(E));
	}
	
	return nullptr;
}


static unique_ptr<PrototypeAST> ParseExtern() {

	getNextTok();								//получаем extern
	
	return ParsePrototype();
}


//--------------------------
//	Code Generation
//--------------------------
static LLVMContext TheContext;
static unique_ptr<Module> TheModule;
static IRBuilder<> Builder(TheContext);
static map<string, Value*> NamedValues;


Value *ErrorV (const char *Str) {
	Error(Str);
	return nullptr;
}


Value *NumberExprAST:: codegen() {
	return ConstantFP::get(TheContext, APFloat(Val));
}


Value *VariableExprAST::codegen() {

	Value *V = NamedValues[Name];
	if (!V) return ErrorV("Unknown variable name.");
	return V;
}

Value *BinaryExprAST::codegen() {
	Value *L = LHS->codegen();
	Value *R = RHS->codegen();
	if (!L || !R) return 0;

	switch(Op) {
	  case '+': return Builder.CreateFAdd(L, R, "addtmp");
	  case '-': return Builder.CreateFSub(L, R, "subtmp");
	  case '*': return Builder.CreateFMul(L, R, "multmp");
	  case '<': 
		L = Builder.CreateFCmpULT(L, R, "cmptmp");
		return Builder.CreateUIToFP(L, Type::getDoubleTy(TheContext), "booltmp");

	  default: return ErrorV("invalid binary operator");
	}
}

Value *CallExprAST:: codegen() {

	Function *CalleeF = TheModule->getFunction(Callee);
	if (!CalleeF)
	  return ErrorV("Unknown function referenced");

	if (CalleeF->arg_size() != Args.size())
	  return ErrorV("Incorrect # arguments passed");

	vector<Value*> ArgsV;
	for (unsigned i = 0, e = Args.size(); i != e; ++i) {
	  ArgsV.push_back(Args[i]->codegen());
	  if (!ArgsV.back()) return nullptr;
	}
	  
 	return Builder.CreateCall(CalleeF, ArgsV, "calltmp");
	  
}


Function *PrototypeAST:: codegen() {
	vector <Type*> Doubles (Args.size(), Type::getDoubleTy(TheContext));
	
	FunctionType *FT = FunctionType::get(Type::getDoubleTy(TheContext), Doubles, false);

	Function *F = Function::Create(FT, Function::ExternalLinkage, Name, TheModule.get());

	unsigned Idx = 0;
	for (auto &Arg : F->args()) 
	  Arg.setName(Args[Idx++]);

	return F;
}


Function *FunctionAST:: codegen() {

	Function *TheFunction = TheModule->getFunction(Proto->getName());
	if (!TheFunction) TheFunction = Proto->codegen();

	if(!TheFunction) return nullptr;

	BasicBlock *BB = BasicBlock::Create(TheContext, "entry", TheFunction);
	Builder.SetInsertPoint(BB);
	
	NamedValues.clear();

	for(auto &Arg: TheFunction->args())
		NamedValues[Arg.getName()] = &Arg;
	
	if(Value *RetVal = Body->codegen()){
		Builder.CreateRet(RetVal);
		verifyFunction(*TheFunction);
		return TheFunction;
	}

	TheFunction->eraseFromParent();
	return nullptr;
}



//----------------------------
//	Top-Level Parsing & JIT
//----------------------------


static void HandleDef() {
	if (auto FnAST = ParseDefinition()) {
	  if (auto *LF = FnAST->codegen()) {
	    fprintf(stderr, "Parsed a function definition.\n");
	    LF->dump();
	  }
	} else {
	  getNextTok();						//пропуск токена для восстановления после ошибки
	}
}


static void HandleExtern() {
	if (auto ProtoAST = ParseExtern()) {
	  if(auto *F = ProtoAST->codegen()){
	    fprintf(stderr, "Parsed an extern.\n"); 
	    F->dump();
	  }	
	} else {
	  getNextTok();						//..
	} 
}

static void HandleTopLevelExpr() {
	if (auto FnAST = ParseTopLevelExpr()) {
	  if(auto *LF = FnAST->codegen()) {
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
		default:			HandleTopLevelExpr(); break;
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
	BinopPrecedence['*'] = 40;

	fprintf(stderr, "ready>> ");
	getNextTok();

	
	TheModule = llvm::make_unique<Module>("\n------my cool jit------", TheContext);

	
	MainLoop();						//цикл интерпретатора
	
	TheModule->dump();

	return 0;
}

