#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "generator.hpp"
#include "parser.hpp"
#include "semantic_analyzer.hpp"
#include "tokenizer.hpp"

void printAST(const ASTNode* node, int depth = 0) {
  std::string indent(depth * 2, ' ');

  auto nodeTypeToStr = [](NodeType t) {
    switch (t) {
      case NodeType::Program:
        return "Program";
      case NodeType::Declaration:
        return "Declaration";
      case NodeType::Expression:
        return "Expression";
      case NodeType::Identifier:
        return "Identifier";
      case NodeType::BinOp:
        return "BinOp";
      case NodeType::Constant:
        return "Constant";
      case NodeType::Literal:
        return "Literal";
      case NodeType::Return:
        return "Return";
      case NodeType::newline:
        return "Newline";
      case NodeType::punctuator:
        return "Punctuator";
    }
    return "Unknown";
  };

  std::cout << indent << nodeTypeToStr(node->type);
  if (!node->value.empty()) {
    std::cout << " (" << node->value << ")";
  }
  std::cout << "\n";

  for (auto* child : node->children) {
    printAST(child, depth + 1);
  }
}

// Swaps (or appends) a ".s" extension onto the source path, e.g.
// "test.txt" -> "test.s", "test" -> "test.s".
std::string deriveOutputPath(const std::string& sourcePath) {
  size_t dot = sourcePath.find_last_of('.');
  size_t slash = sourcePath.find_last_of("/\\");
  bool dotBelongsToExtension = (dot != std::string::npos) &&
                                (slash == std::string::npos || dot > slash);
  std::string base =
      dotBelongsToExtension ? sourcePath.substr(0, dot) : sourcePath;
  return base + ".s";
}

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cerr << "incorrect usage, use ./main <file path> instead."
              << std::endl;
    return EXIT_FAILURE;
  }

  // read contents of program file
  std::string contents;
  {
    std::stringstream contents_stream;
    std::fstream input(argv[1], std::ios::in);
    contents_stream << input.rdbuf();
    contents = contents_stream.str();
  }

  tokenizer tk(contents);
  std::vector<token> tokens = tk.tokenize();

  parser p(tokens);
  ASTNode* root = p.parse_program();

  std::cout << "\n--- AST ---\n";
  printAST(root);

  // semantic analysis: walks the whole AST, reports every error it finds,
  // and (if the program is valid) builds the symbol table codegen needs.
  semantic_analyzer sema(root);
  if (!sema.analyze()) {
    std::cerr << "\n--- semantic errors ---\n";
    for (const std::string& err : sema.errors()) {
      std::cerr << "error: " << err << "\n";
    }
    std::cerr << sema.errors().size() << " error(s); no assembly generated."
              << std::endl;
    return EXIT_FAILURE;
  }

  // codegen: generator trusts the AST/symbol table pair it's given, since
  // semantic_analyzer already vetted both.
  generator gen(root, sema.symbol_table());
  std::string asmOut = gen.generate();

  std::string outPath = deriveOutputPath(argv[1]);
  std::ofstream outFile(outPath);
  outFile << asmOut;
  outFile.close();

  std::cout << "\nCompiled successfully, run program with ./test" << std::endl;
  std::system(("as " + outPath + " -o test.o && ld -o test test.o").c_str());
  return EXIT_SUCCESS;
}