#include <vector>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

#include "tokenizer.hpp"
#include "parser.hpp"
void printAST(const ASTNode* node, int depth = 0) 
{
    std::string indent(depth * 2, ' ');

    auto nodeTypeToStr = [](NodeType t) 
    {
        switch (t) 
        {
            case NodeType::Program: return "Program";
            case NodeType::Declaration: return "Declaration";
            case NodeType::Identifier: return "Identifier";
            case NodeType::BinOp: return "BinOp";
            case NodeType::Constant: return "Constant";
            case NodeType::Literal: return "Literal";
            case NodeType::newline: return "Newline";
            case NodeType::punctuator: return "Punctuator";
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

//Test function
void testAST(const std::vector<token>& contents) 
{

    parser p(contents);
    ASTNode* root = p.parse_program();

    printAST(root);
}

int main(int argc, char* argv[]) 
{
  std::string filename;
  if(argc != 2)
  {

    std::cerr << "incorrect usage, use ./main <file path> instead." << std::endl;
    
    return EXIT_FAILURE;
  }
  //std::cout << "enter a file name: ";
  //std::cin >> filename;

  //read contents of program file
    std::string contents; {
    std::stringstream contents_stream;
    std::fstream input(argv[1],std::ios::in);
    contents_stream << input.rdbuf();
    contents = contents_stream.str();
 }

 tokenizer tk(contents);
 std::vector<token> tokens = tk.tokenize();

  //output to assembly file

  //output to assembly file
  //std::ofstream output_file(filename);
  //output_file <<//asm generation ;
  //output_file.close();

  //return EXIT_SUCCESS;


  //test code
  testAST(tokens);
  return 0;
}