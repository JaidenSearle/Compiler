#pragma once

#include <string>
#include <unordered_map>

// A single declared variable's resolved semantic info: its declared type
// name as spelled in source (e.g. "int", "str"), the stack-frame offset
// assigned to it (relative to %rbp, in bytes), and whether it holds a
// string (an address into .rodata) rather than a plain numeric value.
struct SymbolInfo {
  std::string type;
  int offset;
  bool isString;
};

// Built once by semantic_analyzer while it walks the AST, then handed to
// generator so codegen never has to re-derive variable offsets or
// re-validate types - it can just trust the table and focus on emitting
// instructions.
struct SymbolTable {
  std::unordered_map<std::string, SymbolInfo> symbols;
  int total_size = 0;  // bytes needed across all slots, pre-16-byte-alignment
};
