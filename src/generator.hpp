#pragma once

#include <cctype>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "parser.hpp"

// Target: Linux x86_64 (GNU `as` AT&T syntax), raw syscalls, `_start` entry.
// No libc is used, so the resulting object can be linked with just `ld`.
//
// Runtime model
// -------------
// - The program is flat: no functions, no control flow (the parser doesn't
//   currently produce any), so codegen is a straight-line walk over the
//   Program node's children for now, control flow will be added in future versions.

// - Every declared variable gets an 8-byte slot in a single stack frame
//   allocated once at the top of _start. %rbp (the frame pointer) is set to
//   point at the base of that frame and never moves again, so
//   `offset(%rbp)` is always a valid way to reach a local.
// - %rbp is deliberately kept separate from %rsp: expression evaluation uses
//   native `push`/`pop` on %rsp as a small operand stack, and if variable
//   offsets were relative to %rsp instead, they'd be corrupted every time an
//   expression pushed or popped a temporary.
// - A top-level expression statement (e.g. `x + y * 2` on its own line)
//   evaluates to a value in %rax and the program exits immediately with
//   that value as its exit code (`exit` syscall, number 60). Since this
//   halts the program, anything after the first expression statement is
//   unreachable - that's a property of the language having no control flow,
//   not a bug here. If the AST never contains an expression statement,
//   execution falls off the end into a final `exit(0)`.
//
// Type support
// ------------
// - "int" (and "char" given a numeric literal): a plain 64-bit value in a
//   stack slot, usable in arithmetic expressions.
// - "char" given a quoted literal like "a": stored as that character's
//   ASCII value, same as int.
// - "str" / "char[]" (or anything initialized with a string literal): the
//   literal is emitted into .rodata and the variable's stack slot holds its
//   address (computed RIP-relative via `lea`, so this works whether the
//   binary ends up PIE or not). String variables can be declared but are
//   rejected if you try to use one inside an arithmetic expression - there's
//   no string operator support (concatenation, printing, etc.) yet.
// - "float" / "double": not implemented. x86_64 floating-point work belongs
//   in the SSE registers (movsd/addsd/etc, or an encodable-immediate check
//   plus a literal pool), and the tokenizer doesn't distinguish "3" from
//   "3.0" until codegen time anyway. Rather than silently truncate to an
//   integer, codegen reports an error and stops. Flagging this now so it's
//   a known gap rather than a surprise later.

class generator {
 public:
  inline explicit generator(ASTNode* root) : m_root(root) {}

  inline std::string generate() {
    // Generate the body first; this is what discovers every declared
    // variable (and therefore the final frame size) and every string
    // literal (the .rodata contents). The prologue that depends on the
    // frame size is prepended afterwards.
    for (ASTNode* stmt : m_root->children) {
      gen_statement(stmt);
    }

    // Fallback: if control fell off the end of the program without ever
    // hitting an expression statement, exit cleanly with status 0.
    m_body << "    # implicit exit if no expression statement ran\n";
    emit_exit_immediate(0);

    std::ostringstream out;
    out << ".global _start\n\n";

    if (!m_rodata.str().empty()) {
      out << ".section .rodata\n";
      out << m_rodata.str();
      out << "\n";
    }

    out << ".section .text\n";
    out << "_start:\n";

    int frame_size = align16(m_stack_size);
    if (frame_size > 0) {
      out << "    sub $" << frame_size << ", %rsp\n";
      out << "    mov %rsp, %rbp    # %rbp = stable frame base for locals\n";
    }

    out << m_body.str();

    // Marks the stack as non-executable so the linker doesn't default to an
    // executable stack (and warn about it) just because this object never
    // says otherwise, the way compiler-generated objects normally do.
    out << "\n.section .note.GNU-stack,\"\",@progbits\n";

    return out.str();
  }

 private:
  struct VarInfo {
    int offset;
    bool isString;
  };

  // ---- statement-level codegen ----

  void gen_statement(ASTNode* stmt) {
    if (!stmt) return;

    if (stmt->type == NodeType::Declaration) {
      gen_declaration(stmt);
      return;
    }

    if (stmt->type == NodeType::Expression) {
      if (stmt->children.empty()) {
        fail("empty expression statement");
      }
      gen_expr(stmt->children[0]);
      pop("%rax");  // final expression value
      m_body << "    # expression statement -> exit(%rax)\n";
      m_body << "    mov %rax, %rdi\n";
      m_body << "    mov $60, %rax\n";
      m_body << "    syscall\n";
      return;
    }

    fail("unsupported top-level statement");
  }

  void gen_declaration(ASTNode* decl) {
    const std::string& typeName = decl->value;

    if (decl->children.empty() || decl->children[0]->children.size() != 2) {
      fail("malformed declaration node");
    }

    ASTNode* eq = decl->children[0];
    ASTNode* identNode = eq->children[0];
    ASTNode* valNode = eq->children[1];
    const std::string& name = identNode->value;

    if (m_vars.count(name)) {
      fail("redeclaration of variable '" + name + "'");
    }

    bool isStringType =
        (typeName == "str" || typeName == "str[]" || typeName == "char[]");
    if (typeName == "float" || typeName == "double" ||
        typeName == "float[]" || typeName == "double[]") {
      fail("type '" + typeName + "' is not supported by codegen yet");
    }

    int offset = m_stack_size;
    m_stack_size += 8;
    m_vars[name] = {offset, isStringType};

    m_body << "    # " << typeName << " " << name << " = " << valNode->value
           << "\n";

    if (isStringType) {
      if (!is_string_literal(valNode->value)) {
        fail("'" + name + "' must be initialized with a string literal");
      }
      std::string label = "str_" + std::to_string(m_string_count++);
      m_rodata << "    " << label << ": .asciz " << valNode->value << "\n";
      m_body << "    lea " << label << "(%rip), %rax\n";
      m_body << "    mov %rax, " << offset << "(%rbp)\n";
      return;
    }

    if (is_numeric_literal(valNode->value)) {
      if (valNode->value.find('.') != std::string::npos) {
        fail("floating point literal '" + valNode->value +
             "' not supported yet");
      }
      m_body << "    movabs $" << valNode->value << ", %rax\n";
    } else if (is_string_literal(valNode->value)) {
      // Only meaningful for `char c = "a"` — take the first character's
      // ASCII value. Multi-character strings assigned to a scalar type
      // are rejected rather than silently truncated.
      if (typeName != "char" || valNode->value.size() < 3) {
        fail("type '" + typeName +
             "' cannot be initialized with a string literal");
      }
      unsigned char c = static_cast<unsigned char>(valNode->value[1]);
      m_body << "    mov $" << static_cast<int>(c) << ", %rax\n";
    } else {
      // Identifier reference: copy an existing variable's value.
      auto it = m_vars.find(valNode->value);
      if (it == m_vars.end()) {
        fail("unknown identifier '" + valNode->value + "'");
      }
      if (it->second.isString) {
        fail("cannot initialize '" + name + "' from string variable '" +
             valNode->value + "'");
      }
      m_body << "    mov " << it->second.offset << "(%rbp), %rax\n";
    }

    m_body << "    mov %rax, " << offset << "(%rbp)\n";
  }

  // ---- expression codegen (leaves result on the %rsp-based operand stack)
  // ----

  void gen_expr(ASTNode* node) {
    if (!node) fail("null expression node");

    switch (node->type) {
      case NodeType::Constant: {
        if (node->value.find('.') != std::string::npos) {
          fail("floating point literal '" + node->value +
               "' not supported yet");
        }
        m_body << "    movabs $" << node->value << ", %rax\n";
        push("%rax");
        return;
      }
      case NodeType::Identifier: {
        auto it = m_vars.find(node->value);
        if (it == m_vars.end()) {
          fail("unknown identifier '" + node->value + "'");
        }
        if (it->second.isString) {
          fail("'" + node->value +
               "' is a string and can't be used in an arithmetic "
               "expression");
        }
        m_body << "    mov " << it->second.offset << "(%rbp), %rax\n";
        push("%rax");
        return;
      }
      case NodeType::BinOp: {
        if (node->children.size() != 2) fail("malformed BinOp node");
        gen_expr(node->children[0]);
        gen_expr(node->children[1]);
        pop("%rbx");  // right
        pop("%rax");  // left
        if (node->value == "+") {
          m_body << "    add %rbx, %rax\n";
        } else if (node->value == "-") {
          m_body << "    sub %rbx, %rax\n";
        } else if (node->value == "*") {
          m_body << "    imul %rbx, %rax\n";
        } else if (node->value == "/") {
          // idiv divides the 128-bit value %rdx:%rax by its operand, so
          // %rax (currently the left operand) needs sign-extending into
          // %rdx first. Quotient ends up back in %rax.
          m_body << "    cqto\n";
          m_body << "    idiv %rbx\n";
        } else {
          fail("unsupported operator '" + node->value + "'");
        }
        push("%rax");
        return;
      }
      default:
        fail("unsupported expression node");
    }
  }

  // ---- small helpers ----

  void push(const std::string& reg) { m_body << "    push " << reg << "\n"; }

  void pop(const std::string& reg) { m_body << "    pop " << reg << "\n"; }

  void emit_exit_immediate(int code) {
    m_body << "    mov $" << code << ", %rdi\n";
    m_body << "    mov $60, %rax\n";
    m_body << "    syscall\n";
  }

  static bool is_numeric_literal(const std::string& v) {
    if (v.empty()) return false;
    size_t i = (v[0] == '-') ? 1 : 0;
    if (i >= v.size()) return false;
    for (; i < v.size(); ++i) {
      if (!std::isdigit(static_cast<unsigned char>(v[i])) && v[i] != '.') {
        return false;
      }
    }
    return true;
  }

  static bool is_string_literal(const std::string& v) {
    return v.size() >= 2 && v.front() == '"' && v.back() == '"';
  }

  static int align16(int n) { return (n + 15) & ~15; }

  [[noreturn]] void fail(const std::string& msg) {
    std::cerr << "generator error: " << msg << "\n";
    std::exit(EXIT_FAILURE);
  }

  ASTNode* m_root;
  std::unordered_map<std::string, VarInfo> m_vars;
  std::ostringstream m_body;
  std::ostringstream m_rodata;
  int m_stack_size = 0;
  int m_string_count = 0;
};