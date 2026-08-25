#include <sys/wait.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "generator.hpp"
#include "parser.hpp"
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

// Turns a source path into a sibling .s path: foo/bar.lang -> foo/bar.s,
// bar (no extension) -> bar.s.
static std::string deriveAsmPath(const std::string& sourcePath) {
  size_t slash = sourcePath.find_last_of('/');
  std::string dir =
      (slash == std::string::npos) ? "" : sourcePath.substr(0, slash + 1);
  std::string base =
      (slash == std::string::npos) ? sourcePath : sourcePath.substr(slash + 1);
  size_t dot = base.find_last_of('.');
  std::string stem = (dot == std::string::npos) ? base : base.substr(0, dot);
  return dir + stem + ".s";
}

// Swaps whatever extension `path` has (if any) for `newExt`, e.g.
// replaceExtension("out/foo.s", ".o") -> "out/foo.o".
static std::string replaceExtension(const std::string& path,
                                     const std::string& newExt) {
  size_t slash = path.find_last_of('/');
  size_t dot = path.find_last_of('.');
  if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) {
    return path + newExt;
  }
  return path.substr(0, dot) + newExt;
}

// ============================================================================
// Codegen test suite (`./main --test`)
//
// generator::fail() calls std::exit() directly on a codegen error - that's
// the right behavior for the real compiler (bad program -> stop), but it
// means a "this should fail" test case would take the whole test binary
// down with it if run in-process. runGeneratorIsolated() forks a child for
// each case so a std::exit() in there only ends that one test.
//
// The generated assembly is handed back to the parent through its own pipe
// via a raw write(), deliberately bypassing std::cout - parser.hpp prints
// " >>> parse sucessful" to std::cout on every successful declaration, and
// letting that share a channel with the assembly text corrupts the .s file.
// The child's real stdout is just discarded.
// ============================================================================

struct IsolatedResult {
  int exitCode;
  std::string asmText;
  std::string stderrText;
};

static std::string readAllFromFd(int fd) {
  std::string result;
  char buf[4096];
  ssize_t n;
  while ((n = read(fd, buf, sizeof(buf))) > 0) {
    result.append(buf, static_cast<size_t>(n));
  }
  return result;
}

static std::string trim(const std::string& s) {
  size_t start = s.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) return "";
  size_t end = s.find_last_not_of(" \t\r\n");
  return s.substr(start, end - start + 1);
}

// Runs tokenizer -> parser -> generator on `source` in a forked child.
// Returns the child's exit code, the generated assembly (on success), and
// whatever generator::fail() wrote to stderr (on failure).
static IsolatedResult runGeneratorIsolated(const std::string& source) {
  int asmPipe[2];
  int errPipe[2];
  if (pipe(asmPipe) != 0 || pipe(errPipe) != 0) {
    std::cerr << "failed to create pipes for isolated test run\n";
    std::exit(EXIT_FAILURE);
  }

  // Flush before forking: otherwise a child inherits whatever this
  // process's std::cout buffer is still holding (e.g. earlier test
  // results that haven't hit a newline-triggered flush yet), and the
  // freopen below - which closes the inherited stream - flushes that
  // stale content straight to the terminal, duplicating prior output.
  std::cout.flush();

  pid_t pid = fork();
  if (pid < 0) {
    std::cerr << "fork() failed\n";
    std::exit(EXIT_FAILURE);
  }

  if (pid == 0) {
    // child
    std::freopen("/dev/null", "w", stdout);  // discard parser's debug prints
    dup2(errPipe[1], STDERR_FILENO);         // capture generator::fail()
    close(asmPipe[0]);
    close(errPipe[0]);
    close(errPipe[1]);

    tokenizer tk(source);
    std::vector<token> tokens = tk.tokenize();
    parser p(tokens);
    ASTNode* root = p.parse_program();
    generator g(root);
    std::string asmOut = g.generate();  // may std::exit(EXIT_FAILURE) on error

    // hand the assembly back via a raw write, never through std::cout
    size_t written = 0;
    while (written < asmOut.size()) {
      ssize_t n = write(asmPipe[1], asmOut.data() + written,
                         asmOut.size() - written);
      if (n <= 0) break;
      written += static_cast<size_t>(n);
    }
    close(asmPipe[1]);
    std::exit(EXIT_SUCCESS);
  }

  // parent
  close(asmPipe[1]);
  close(errPipe[1]);

  IsolatedResult result;
  result.asmText = readAllFromFd(asmPipe[0]);
  result.stderrText = readAllFromFd(errPipe[0]);
  close(asmPipe[0]);
  close(errPipe[0]);

  int status = 0;
  waitpid(pid, &status, 0);
  result.exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  return result;
}

// True if `as` and `ld` are on PATH, so the success-case tests can actually
// assemble, link, and run the emitted program instead of just checking that
// codegen didn't error. Since codegen now targets x86_64 and this project
// builds on x86_64, no cross-toolchain or emulator is needed - the same
// `as`/`ld` used to build this binary can build the generated one.
static bool toolchainAvailable() {
  int rc = std::system(
      "command -v as > /dev/null 2>&1 && command -v ld > /dev/null 2>&1");
  return rc == 0;
}

// Assembles + links + runs `asmText` and reports the program's exit code.
// Returns false if the toolchain itself failed (bad assembly, link error,
// etc.) rather than the resulting program running. The .s/.o/.bin files are
// left behind under test_output/ so the generated assembly for every test
// case stays inspectable after the run finishes.
static bool assembleLinkAndRun(const std::string& asmText, int& exitCodeOut,
                                const std::string& tag) {
  std::system("mkdir -p test_output");
  std::string base = "test_output/" + tag;
  std::string sPath = base + ".s";
  std::string oPath = base + ".o";
  std::string binPath = base + ".bin";

  {
    std::ofstream f(sPath);
    f << asmText;
  }

  std::string cmd = "as -o " + oPath + " " + sPath + " && ld -o " + binPath +
                     " " + oPath + " && ./" + binPath;
  int status = std::system(cmd.c_str());
  if (status == -1) return false;
  exitCodeOut = WEXITSTATUS(status);
  return true;
}

struct CodegenCase {
  std::string name;
  std::string source;
  bool shouldSucceed;
  int expectedExit;  // only meaningful when shouldSucceed is true
};

void runCodegenTests() {
  std::vector<CodegenCase> cases = {
      // straight-line arithmetic, operator precedence
      {"arithmetic_precedence", "int x = 5\nint y = 3\nx + y * 2\n", true,
       11},
      // parentheses override precedence
      {"parentheses", "int x = 5\nint y = 3\n(x + y) * 2\n", true, 16},
      // division + chaining across several variables
      {"division_and_chaining",
       "int a = 20\nint b = 4\nint c = 3\na / b - c\n", true, 2},
      // string declaration alongside an int used as the exit value
      {"string_decl_with_int_exit",
       "str greeting = \"hello world\"\nint x = 7\nx\n", true, 7},
      // no expression statement -> falls through to the implicit exit(0)
      {"falls_through_with_no_expression_statement",
       "int x = 5\nint y = 10\n", true, 0},
      // error cases: codegen should report an error and exit(1)
      {"redeclaration_is_rejected", "int x = 5\nint x = 3\nx\n", false, 0},
      {"unknown_identifier_is_rejected", "int x = 5\nx + z\n", false, 0},
      {"string_in_arithmetic_is_rejected", "str s = \"hi\"\ns + 1\n", false,
       0},
  };

  bool haveToolchain = toolchainAvailable();
  if (!haveToolchain) {
    std::cout << "note: `as`/`ld` not found on PATH - success-case tests "
                 "will only check\n"
                 "      that codegen doesn't error, not that the emitted "
                 "program actually\n"
                 "      produces the right exit code.\n\n";
  }

  int passed = 0;
  int failed = 0;

  for (const auto& c : cases) {
    IsolatedResult r = runGeneratorIsolated(c.source);

    if (!c.shouldSucceed) {
      bool ok = (r.exitCode == EXIT_FAILURE);
      std::cout << (ok ? "[PASS] " : "[FAIL] ") << c.name;
      if (!r.stderrText.empty()) {
        std::cout << "  (" << trim(r.stderrText) << ")";
      }
      std::cout << "\n";
      ok ? ++passed : ++failed;
      continue;
    }

    if (r.exitCode != EXIT_SUCCESS) {
      std::cout << "[FAIL] " << c.name
                << "  codegen unexpectedly errored: " << trim(r.stderrText)
                << "\n";
      ++failed;
      continue;
    }

    if (!haveToolchain) {
      std::cout << "[SKIP] " << c.name
                << "  (codegen succeeded; not verified against a real exit "
                   "code)\n";
      continue;
    }

    int actualExit = -1;
    if (!assembleLinkAndRun(r.asmText, actualExit, c.name)) {
      std::cout << "[FAIL] " << c.name << "  assemble/link/run failed\n";
      ++failed;
      continue;
    }

    bool ok = (actualExit == c.expectedExit);
    std::cout << (ok ? "[PASS] " : "[FAIL] ") << c.name << "  expected exit "
              << c.expectedExit << ", got " << actualExit << "\n";
    ok ? ++passed : ++failed;
  }

  std::cout << "\n"
            << passed << " passed, " << failed << " failed"
            << (haveToolchain ? "" : " (some tests skipped - no aarch64 toolchain)")
            << "\n"
            << "generated .s/.o/.bin files for each case are in test_output/\n";

  if (failed > 0) std::exit(EXIT_FAILURE);
}

int main(int argc, char* argv[]) {
  if (argc < 2 || argc > 3) {
    std::cerr << "usage:\n"
                 "  ./main <file path> [output.s]   compile a source file\n"
                 "  ./main --test                   run the codegen test "
                 "suite\n";
    return EXIT_FAILURE;
  }

  if (std::string(argv[1]) == "--test") {
    if (argc != 2) {
      std::cerr << "--test doesn't take extra arguments\n";
      return EXIT_FAILURE;
    }
    runCodegenTests();
    return 0;
  }

  // read contents of program file
  std::string contents;
  {
    std::stringstream contents_stream;
    std::fstream input(argv[1], std::ios::in);
    if (!input) {
      std::cerr << "error: could not open '" << argv[1] << "'\n";
      return EXIT_FAILURE;
    }
    contents_stream << input.rdbuf();
    contents = contents_stream.str();
  }

  tokenizer tk(contents);
  std::vector<token> tokens = tk.tokenize();

  parser p(tokens);
  ASTNode* root = p.parse_program();
  printAST(root);

  generator g(root);
  std::string asmOut = g.generate();  // exits with an error message if the
                                       // program doesn't compile

  std::string outputPath = (argc == 3) ? argv[2] : deriveAsmPath(argv[1]);
  std::ofstream asmFile(outputPath);
  if (!asmFile) {
    std::cerr << "error: could not open '" << outputPath << "' for writing\n";
    return EXIT_FAILURE;
  }
  asmFile << asmOut;
  asmFile.close();

  std::cout << "\n--- generated arm64 assembly ---\n" << asmOut;
  std::cout << "wrote " << outputPath << "\n";
  std::string objPath = replaceExtension(outputPath, ".o");
  std::cout << "assemble/link with:\n"
            << "  as -o " << objPath << " " << outputPath << "\n"
            << "  ld -o out " << objPath << "\n";

  return 0;
}