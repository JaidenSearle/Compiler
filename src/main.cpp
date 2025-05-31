#include <vector>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

#include "tokenizer.hpp"


int main(int argc, char* argv[]) {
  std::string filename;
  if(argc != 2){

    std::cerr << "incorrect usage, use ./main <file path> instead." << std::endl;
    
    return EXIT_FAILURE;
  }
  std::cout << "enter a file name: ";
  std::cin >> filename;

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
  //std::ofstream output_file(filename);
  //output_file <<//asm generation ;
  //output_file.close();

  //return EXIT_SUCCESS;


  //test code
 for (const auto& t : tokens) {
        std::cout << "Token Type: ";
        switch (t.type) {
            case tokenType::identifier: std::cout << "identifier"; break;
            case tokenType::keyword: std::cout << "keyword"; break;
            case tokenType::punctuator: std::cout << "punctuator"; break;
            case tokenType::op: std::cout << "operator"; break;
            case tokenType::literal: std::cout << "literal"; break;
            case tokenType::constant: std::cout << "constant"; break;
        }

        if (t.value) {
            std::cout << ", Value: " << *t.value;
            
        }
        if(t.line){
          std::cout << ", Line:" << *t.line;
        }

        std::cout << '\n';
    }
    return 0;
}