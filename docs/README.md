# Compiler
  A simple compiler for a custom programming language, developed in parallel with the compiler itself.  
The project focuses on learning and implementing core compiler concepts, from lexical analysis through code generation.
## Overview
This compiler:
- Compiles source code to **ARM64 assembly**
- Targets **Linux-based systems**
- Uses a C/Python–inspired syntax:
  - No semicolons
  - Braces (`{}`) for function and block scopes (not indentation-based)

## Features
  - **Lexical analysis**
  - Tokenization of source code into identifiers, literals, constants, and operators
- **Parsing**
  - LL(1) grammar
  - Hand-written recursive descent parser
- **Abstract Syntax Tree (AST) generation**
  - Clean AST representation for expressions and declarations
- **Operator precedence**
  - Correct handling of `+ - * /`
  - Supports implicit multiplication in expressions
    
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
Note: This is an early-stage project; a build system (e.g. Makefile or CMake) may be added later.
 # Grammar

 ## Declaration Grammar

  Declaration → Type Identifier "=" Value Newline

  Type → TYPE_KEYWORD

  Value → Identifier | Literal | Constant

## Expression Grammar (BNF style)

  Expr → Term Expr_tail

  Expr_tail → + Term Expr_tail | - Term Expr_tail |  $\epsilon$

  Term → Factor Term_tail

  Term_tail → * Factor Term_tail | / Factor Term_tail | Factor Term_tail | $\epsilon$

  Factor → id | constant | literal | ( Expr )

## Notes

- Operator precedence is enforced structurally (* and / bind tighter than + and -)

- All binary operators are left-associative

- Implicit multiplication is supported when a Factor directly follows another Factor

- This rule is enforced in the parser rather than explicitly encoded in the grammar

## Project Status

- This compiler is under active development.
Planned future work includes:

- Function definitions and calls

- Control flow (if/while)

- Improved error recovery

- Code generation pipeline improvements

- Build system integration

