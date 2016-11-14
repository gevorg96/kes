#include "ast.h"
#include "parser.h"
#include "generator.h"
#include <iostream>
#include <fstream>
#include <llvm/Support/raw_os_ostream.h>
#include <llvm/Bitcode/ReaderWriter.h>

Module *Generate(StmtNode *Prog);

int main(int argc, char **argv) {
  std::ifstream input;
  Parser *P;

  if (argc > 1) {
    input.open(argv[1], std::ifstream::in);
    if (!input.is_open())
      return -1;

    P = new Parser(input);
  } else {
    P = new Parser(std::cin);
  }

  auto Prog = P->Parse();
  if (P->ParserSuccess()) {
    Module *Main = Generate(Prog);
    if (Main != nullptr) {
      Main->dump();
      
      // Сохранение биткода LLVM IR в stdout
      llvm::raw_os_ostream os(std::cout);
      llvm::WriteBitcodeToFile(Main, os);
      return 0;
    }
  }

  std::cerr << "Incorrent program." << std::endl << std::endl;
  if (Prog) {
    Prog->Format(std::cerr, 0);
    std::cerr << std::endl;
  }

  return 0;
}
