#pragma once

#include <cctype>
#include <string>
#include <vector>

#include "parser.hpp"
#include "symbol_table.hpp"

// Walks the entire AST once, collecting every semantic error it finds
// (rather than stopping at the first one, the way generator used to), and
// builds the SymbolTable that codegen will use. Everything checked here -
// redeclarations, undeclared identifiers, type mismatches, unsupported
// types - no longer needs to be re-checked inside generator; by the time
// generator runs it can just trust the AST and the table.
//
// Usage:
//   semantic_analyzer sema(root);
//   if (!sema.analyze()) {
//     for (auto& e : sema.errors()) std::cerr << e << "\n";
//     // do not proceed to codegen
//   } else {
//     generator gen(root, sema.symbol_table());
//     ...
//   }
class semantic_analyzer {
 public:
  explicit semantic_analyzer(ASTNode* root) : m_root(root) {}

  // Returns true if the whole program is semantically valid. Every
  // top-level statement is checked regardless of earlier failures, so one
  // run surfaces every problem instead of just the first.
  bool analyze() {
    if (!m_root) {
      m_errors.push_back("internal error: null AST root");
      return false;
    }
    for (ASTNode* stmt : m_root->children) {
      check_statement(stmt);
    }
    return m_errors.empty();
  }

  const std::vector<std::string>& errors() const { return m_errors; }

  // Only meaningful once analyze() has returned true.
  const SymbolTable& symbol_table() const { return m_table; }

 private:
  //statement-level checks

  void check_statement(ASTNode* stmt) {
    if (!stmt) return;

    switch (stmt->type) {
      case NodeType::Declaration:
        check_declaration(stmt);
        return;
      case NodeType::Return:
        if (!stmt->children.empty()) {
          check_expr(stmt->children[0]);
        }
        return;
      case NodeType::Expression:
        if (stmt->children.empty()) {
          m_errors.push_back("empty expression statement");
          return;
        }
        check_expr(stmt->children[0]);
        return;
      default:
        m_errors.push_back("unsupported top-level statement");
        return;
    }
  }

  void check_declaration(ASTNode* decl) {
    const std::string& typeName = decl->value;

    if (decl->children.empty() || decl->children[0]->children.size() != 2) {
      m_errors.push_back("malformed declaration node");
      return;
    }

    ASTNode* eq = decl->children[0];
    ASTNode* identNode = eq->children[0];
    ASTNode* valNode = eq->children[1];
    const std::string& name = identNode->value;

    if (typeName == "float" || typeName == "double" ||
        typeName == "float[]" || typeName == "double[]") {
      m_errors.push_back("type '" + typeName +
                          "' is not supported by codegen yet (declaring '" +
                          name + "')");
      return;
    }

    // Arrays aren't implemented anywhere past this point yet - "str[]" and
    // "char[]" are accepted (they're just treated as an alias for a single
    // string, same as "str"), but "int[]" would otherwise silently fall
    // through to the plain-scalar-int path below and be miscompiled, so
    // it's rejected explicitly instead.
    if (typeName == "int[]") {
      m_errors.push_back("array types are not supported yet (declaring '" +
                          name + "')");
      return;
    }

    if (m_table.symbols.count(name)) {
      m_errors.push_back("redeclaration of variable '" + name + "'");
      return;
    }

    bool isStringType =
        (typeName == "str" || typeName == "str[]" || typeName == "char[]");

    // NOTE: the parser currently tags every declaration initializer as
    // NodeType::Constant regardless of whether the source token was an
    // identifier, a string literal, or a numeric constant - so type
    // checking here has to fall back to inspecting the *text* of
    // valNode->value to tell them apart, same as generator used to.
    bool ok = true;
    if (isStringType) {
      if (!is_string_literal(valNode->value)) {
        m_errors.push_back("'" + name +
                            "' must be initialized with a string literal");
        ok = false;
      }
    } else if (is_numeric_literal(valNode->value)) {
      if (valNode->value.find('.') != std::string::npos) {
        m_errors.push_back("floating point literal '" + valNode->value +
                            "' not supported yet (initializing '" + name +
                            "')");
        ok = false;
      }
    } else if (is_string_literal(valNode->value)) {
      // Only meaningful for `char c = "a"`.
      if (typeName != "char") {
        m_errors.push_back("type '" + typeName +
                            "' cannot be initialized with a string literal "
                            "(declaring '" +
                            name + "')");
        ok = false;
      } else if (valNode->value.size() != 3) {
        m_errors.push_back(
            "'" + name +
            "' is type 'char' but was initialized with a "
            "multi-character literal " +
            valNode->value);
        ok = false;
      }
    } else {
      // Identifier reference: must already be declared, and be
      // type-compatible (string-ness must match).
      auto it = m_table.symbols.find(valNode->value);
      if (it == m_table.symbols.end()) {
        m_errors.push_back("unknown identifier '" + valNode->value +
                            "' (initializing '" + name + "')");
        ok = false;
      } else if (isStringType != it->second.isString) {
        m_errors.push_back("cannot initialize '" + name + "' (" + typeName +
                            ") from '" + valNode->value + "' (" +
                            it->second.type + ") - incompatible types");
        ok = false;
      }
    }

    if (!ok) return;

    int offset = m_table.total_size;
    m_table.total_size += 8;
    m_table.symbols[name] = {typeName, offset, isStringType};
  }

  //expression checks

  // Recursively type-checks an expression subtree. Every leaf must either
  // resolve to a plain numeric value, or an error is recorded.
  void check_expr(ASTNode* node) {
    if (!node) {
      m_errors.push_back("null expression node");
      return;
    }

    switch (node->type) {
      case NodeType::Constant:
        if (node->value.find('.') != std::string::npos) {
          m_errors.push_back("floating point literal '" + node->value +
                              "' not supported yet");
        }
        return;
      case NodeType::Identifier: {
        auto it = m_table.symbols.find(node->value);
        if (it == m_table.symbols.end()) {
          m_errors.push_back("unknown identifier '" + node->value + "'");
        } else if (it->second.isString) {
          m_errors.push_back("'" + node->value +
                              "' is a string and can't be used in an "
                              "arithmetic expression");
        }
        return;
      }
      case NodeType::BinOp:
        if (node->children.size() != 2) {
          m_errors.push_back("malformed binary operator node ('" +
                              node->value + "')");
          return;
        }
        check_expr(node->children[0]);
        check_expr(node->children[1]);
        return;
      default:
        m_errors.push_back("unsupported expression node");
        return;
    }
  }

  //helpers

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

  ASTNode* m_root;
  SymbolTable m_table;
  std::vector<std::string> m_errors;
};
