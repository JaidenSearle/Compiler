# Compiler
  A simple compiler for a custom language being developed alongside of the compiler development. 

## Overview
- Compiles source code to ARM64 assembly
- Targeted for Linux based systems
- Will  have similar syntax to C and Python (No semicolons, uses brackets for function scopes rather than tabs)
## Features
  - Lexical analysis (tokenizer)
  - Parsing (grammar type: LL(1), recursive descent)
  - AST generation
    
## Project Structure
/src

main.cpp

tokenizer.hpp

parser.hpp

test.txt (Source code file to be compiled)
## Usage
project compiles with gcc:
```bash
gcc main.cpp -o program_name
```
to run compiler:
```bash
./main source_file.txt
```
