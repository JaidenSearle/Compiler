#pragma once

#include <cctype>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "parser.hpp"
#include "symbol_table.hpp"

// Target: Linux x86_64 (GNU `as` AT&T syntax), raw syscalls, `_start` entry.
// No libc is used, so the resulting object can be linked with just `ld`.
//
// Assumes the AST it's given has already been checked by semantic_analyzer,
// and takes the SymbolTable that analyzer built as a constructor argument.
// Variable offsets and string-ness are looked up from that table rather
// than recomputed here, and none of the type/redeclaration/undeclared-
// identifier checks that used to live in this file are repeated - a
// generator error below signals a broken internal invariant (e.g. this AST
// was never run through semantic_analyzer), not a bad user program.
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
// - A `return <expr>` statement evaluates the expression and prints its
//   decimal value to stdout (with a trailing newline), then the program
//   exits with status 0 (`exit` syscall, number 60). A bare `return` just
//   exits with status 0 without printing anything. Either form halts the
//   program immediately, so anything after the first return statement is
//   unreachable - that's a property of the language having no control flow,
//   not a bug here. If the AST never contains a return statement, execution
//   falls off the end into a final `exit(0)`.
// - A bare expression statement (e.g. `x + y * 2` on its own line, with no
//   `return`) is still legal - it's evaluated for any side effects and its
//   result is discarded. It has no effect on the program's output or exit
//   status; use `return` to actually surface a value.
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
//   no string operator support (concatenation, printing, etc.) yet, and
//   `return` only knows how to print integers.
// - "float" / "double": not implemented. x86_64 floating-point work belongs
//   in the SSE registers (movsd/addsd/etc, or an encodable-immediate check
//   plus a literal pool), and the tokenizer doesn't distinguish "3" from
//   "3.0" until codegen time anyway. Rather than silently truncate to an
//   integer, codegen reports an error and stops. Flagging this now so it's
//   a known gap rather than a surprise later.

class generator {
 public:
  inline generator(ASTNode* root, SymbolTable symbols)
      : m_root(root), m_symbols(std::move(symbols)) {}

  inline std::string generate() {
    // Generate the body first; this is what discovers every declared
    // variable (and therefore the final frame size), every string
    // literal (the .rodata contents), and whether the print-integer
    // helper is needed at all.
    for (ASTNode* stmt : m_root->children) {
      gen_statement(stmt);
    }

    // Fallback: if control fell off the end of the program without ever
    // hitting a return statement, exit cleanly with status 0.
    m_body << "    # implicit exit if no return statement ran\n";
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

    int frame_size = align16(m_symbols.total_size);
    if (frame_size > 0) {
      out << "    sub $" << frame_size << ", %rsp\n";
      out << "    mov %rsp, %rbp    # %rbp = stable frame base for locals\n";
    }

    out << m_body.str();

    // print_int is only emitted (and only takes up space in the binary) if
    // some `return <expr>` actually needed it.
    if (m_needs_print_int) {
      out << "\n" << print_int_subroutine();
    }

    // Marks the stack as non-executable so the linker doesn't default to an
    // executable stack (and warn about it) just because this object never
    // says otherwise, the way compiler-generated objects normally do.
    out << "\n.section .note.GNU-stack,\"\",@progbits\n";

    return out.str();
  }

 private:
  //statement-level codegen

  void gen_statement(ASTNode* stmt) {
    if (!stmt) return;

    if (stmt->type == NodeType::Declaration) {
      gen_declaration(stmt);
      return;
    }

    if (stmt->type == NodeType::Return) {
      gen_return(stmt);
      return;
    }

    if (stmt->type == NodeType::Expression) {
      if (stmt->children.empty()) {
        fail("empty expression statement");
      }
      gen_expr(stmt->children[0]);
      pop("%rax");  // value isn't surfaced anywhere; only side effects (if
                    // any existed) would matter here
      m_body << "    # expression statement (value discarded, use 'return' "
                "to print a value)\n";
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

    // semantic_analyzer already validated this declaration and assigned it
    // a slot; look it up rather than recomputing anything.
    const SymbolInfo& info = lookup(name);

    m_body << "    # " << typeName << " " << name << " = " << valNode->value
           << "\n";

    if (info.isString) {
      // Guaranteed by semantic_analyzer to be a string literal.
      std::string label = "str_" + std::to_string(m_string_count++);
      m_rodata << "    " << label << ": .asciz " << valNode->value << "\n";
      m_body << "    lea " << label << "(%rip), %rax\n";
      m_body << "    mov %rax, " << info.offset << "(%rbp)\n";
      return;
    }

    if (is_numeric_literal(valNode->value)) {
      m_body << "    movabs $" << valNode->value << ", %rax\n";
    } else if (is_string_literal(valNode->value)) {
      // Only reachable for `char c = "a"` - semantic_analyzer already
      // confirmed it's a single-character literal on a 'char' variable.
      unsigned char c = static_cast<unsigned char>(valNode->value[1]);
      m_body << "    mov $" << static_cast<int>(c) << ", %rax\n";
    } else {
      // Identifier reference - semantic_analyzer already confirmed it's
      // declared and type-compatible.
      const SymbolInfo& src = lookup(valNode->value);
      m_body << "    mov " << src.offset << "(%rbp), %rax\n";
    }

    m_body << "    mov %rax, " << info.offset << "(%rbp)\n";
  }

  // `return` evaluate the (optional) expression, print it as a decimal
  // integer followed by a newline, then exit(0). A bare `return` skips
  // straight to exit(0). Either way this halts the program, matching the
  // parser's `Return → "return" [ Expr ] Newline` grammar.
  void gen_return(ASTNode* ret) {
    if (ret->children.empty()) {
      m_body << "    # return (no value)\n";
      emit_exit_immediate(0);
      return;
    }

    gen_expr(ret->children[0]);
    pop("%rax");  // value to print

    m_body << "    # return statement -> print value, exit(0)\n";
    m_body << "    call print_int\n";
    emit_exit_immediate(0);
    m_needs_print_int = true;
  }

  //expression codegen (leaves result on the %rsp-based operand stack)

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
        // semantic_analyzer already confirmed this identifier is declared
        // and non-string wherever it appears in an arithmetic expression.
        const SymbolInfo& info = lookup(node->value);
        m_body << "    mov " << info.offset << "(%rbp), %rax\n";
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

  //helpers

  void push(const std::string& reg) { m_body << "    push " << reg << "\n"; }

  void pop(const std::string& reg) { m_body << "    pop " << reg << "\n"; }

  void emit_exit_immediate(int code) {
    m_body << "    mov $" << code << ", %rdi\n";
    m_body << "    mov $60, %rax\n";
    m_body << "    syscall\n";
  }

  // Every call site here is only reachable for a name semantic_analyzer
  // already resolved, so a miss means this AST/table pair is mismatched -
  // an internal error, not a normal compile error.
  const SymbolInfo& lookup(const std::string& name) {
    auto it = m_symbols.symbols.find(name);
    if (it == m_symbols.symbols.end()) {
      fail("internal error: '" + name +
           "' missing from symbol table - was semantic_analyzer run on "
           "this AST first?");
    }
    return it->second;
  }

  // Emitted once (only when a `return <expr>` was actually generated) after
  // the fallback exit, so control can never fall into it by accident - every
  // path that reaches it does so via an explicit `call print_int`.
  //
  // Contract: %rax holds a signed 64-bit value on entry. Writes its decimal
  // representation followed by '\n' to stdout (fd 1). Clobbers %rax, %rbx,
  // %rdx, %rsi, %rdi, %rcx, %r11 (the last two per the `syscall`
  // calling convention) and restores everything else it touches.
  static std::string print_int_subroutine() {
    return
        "print_int:\n"
        "    push %rbx\n"
        "    push %r12\n"
        "    push %r13\n"
        "    push %r14\n"
        "    sub $32, %rsp          # scratch buffer for up to 20 digits + "
        "sign\n"
        "\n"
        "    mov %rax, %r12         # r12 = remaining value\n"
        "    xor %r13, %r13         # r13 = 1 if negative, else 0\n"
        "    cmp $0, %r12\n"
        "    jge 1f\n"
        "    mov $1, %r13\n"
        "    neg %r12\n"
        "1:\n"
        "    lea 32(%rsp), %r14     # r14 = write cursor, starts past the "
        "buffer end\n"
        "    mov $10, %rbx\n"
        "    cmp $0, %r12\n"
        "    jne 3f\n"
        "    dec %r14\n"
        "    movb $'0', (%r14)      # value was exactly 0\n"
        "    jmp 5f\n"
        "3:\n"
        "2:\n"
        "    cmp $0, %r12\n"
        "    je 5f\n"
        "    xor %rdx, %rdx\n"
        "    mov %r12, %rax\n"
        "    div %rbx               # %rax = quotient, %rdx = remainder\n"
        "    add $'0', %dl\n"
        "    dec %r14\n"
        "    mov %dl, (%r14)\n"
        "    mov %rax, %r12\n"
        "    jmp 2b\n"
        "5:\n"
        "    cmp $1, %r13\n"
        "    jne 4f\n"
        "    dec %r14\n"
        "    movb $'-', (%r14)\n"
        "4:\n"
        "    lea 32(%rsp), %rax\n"
        "    sub %r14, %rax         # length = end - cursor\n"
        "    mov %rax, %rdx\n"
        "    mov %r14, %rsi\n"
        "    mov $1, %rdi           # fd = stdout\n"
        "    mov $1, %rax           # syscall write\n"
        "    syscall\n"
        "\n"
        "    movb $'\\n', 31(%rsp)\n"
        "    lea 31(%rsp), %rsi\n"
        "    mov $1, %rdx\n"
        "    mov $1, %rdi\n"
        "    mov $1, %rax\n"
        "    syscall\n"
        "\n"
        "    add $32, %rsp\n"
        "    pop %r14\n"
        "    pop %r13\n"
        "    pop %r12\n"
        "    pop %rbx\n"
        "    ret\n";
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
  SymbolTable m_symbols;
  std::ostringstream m_body;
  std::ostringstream m_rodata;
  int m_string_count = 0;
  bool m_needs_print_int = false;
};