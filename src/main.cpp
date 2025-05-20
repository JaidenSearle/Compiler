#include <vector>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <optional>

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



//output to assembly file
//std::ofstream output_file(filename);
//output_file <<//asm generation ;
//output_file.close();

//return EXIT_SUCCESS;

}