// End-to-end tests for the C compiler. The real `bcc` compiles a C file to basm, the real `basm`
// assembles it with the startup code (ccompiler/lib/crt0.basm) and links it, and the real `emu32`
// runs the program. The tests assert on the value main returned, which crt0 leaves in x0.

#include "util/types.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <unordered_map>

namespace fs = std::filesystem;

namespace
{

/// Upper bound on instructions per program so a miscompiled loop can't hang the test.
constexpr int kMaxInstructions = 100000;

/// Each test works in its own scratch directory. It is removed if the test passes and kept (with
/// the logs of the three tools and state.txt) if it fails.
class CompilerIntegration : public ::testing::Test
{
  protected:
    fs::path m_dir;

    /// The key=value state the last emu32 run dumped.
    std::unordered_map<std::string, std::string> m_state;

    void SetUp() override
    {
        const ::testing::TestInfo *info = ::testing::UnitTest::GetInstance()->current_test_info();
        m_dir = fs::temp_directory_path() / "aemu_ccompiler_integration"
                / (std::string(info->test_suite_name()) + "." + info->name());
        fs::remove_all(m_dir);
        fs::create_directories(m_dir);
    }

    void TearDown() override
    {
        if (HasFailure())
        {
            std::cerr << "Build artifacts kept in " << m_dir << "\n";
        }
        else
        {
            fs::remove_all(m_dir);
        }
    }

    void write_file(const std::string &name, const std::string &contents)
    {
        std::ofstream(m_dir / name) << contents;
    }

    std::string log_tail(const std::string &log, size_t max_chars = 4000) const
    {
        std::ifstream in(m_dir / log);
        std::stringstream ss;
        ss << in.rdbuf();
        const std::string contents = ss.str();
        return contents.size() > max_chars ? contents.substr(contents.size() - max_chars)
                                           : contents;
    }

    /// Runs a command from the scratch directory with its output in `log`. Returns the exit code.
    int run_command(const std::string &exe, const std::string &args, const std::string &log)
    {
        const std::string cmd =
            "cd \"" + m_dir.string() + "\" && \"" + exe + "\" " + args + " > " + log + " 2>&1";
        const int status = std::system(cmd.c_str());
        return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }

    /// Compiles `source`, a whole C file, to test.basm, with the bcc options `options`. Returns
    /// the exit code of bcc, whose messages are in bcc.log.
    int compile(const std::string &source, const std::string &options = "")
    {
        write_file("test.c", source);
        return run_command(CCOMPILER_PATH, options + " -o test.basm test.c", "bcc.log");
    }

    /// Compiles `source`, links it with crt0 and runs it. Fails the test unless the program halts.
    /// x0 starts as 99, so that a result of 0 is something main did.
    void run_c(const std::string &source, const std::string &options = "")
    {
        ASSERT_EQ(compile(source, options), 0) << log_tail("bcc.log");
        ASSERT_EQ(run_command(ASSEMBLER_PATH, "-o prog \"" CRT0_PATH "\" test.basm -outdir .",
                              "basm.log"),
                  0)
            << log_tail("basm.log");
        run_command(EMULATOR_PATH,
                    "-e prog.bexe -l " + std::to_string(kMaxInstructions)
                        + " --reg x0=99 --format plain -o state.txt",
                    "emu32.log");

        m_state.clear();
        std::ifstream in(m_dir / "state.txt");
        std::string line;
        while (std::getline(in, line))
        {
            const size_t eq = line.find('=');
            if (eq != std::string::npos)
            {
                m_state[line.substr(0, eq)] = line.substr(eq + 1);
            }
        }
        ASSERT_EQ(m_state["status"], "halted") << log_tail("emu32.log");
    }

    /// Runs a program and expects main to return `expected`.
    void expect_result(const std::string &source, word expected, const std::string &options = "")
    {
        ASSERT_NO_FATAL_FAILURE(run_c(source, options));
        EXPECT_EQ(static_cast<word>(std::stoull(m_state["x0"], nullptr, 0)), expected);
    }

    /// Expects bcc to reject `source` with a message that contains `message`.
    void expect_error(const std::string &source, const std::string &message)
    {
        EXPECT_EQ(compile(source), 1);
        EXPECT_NE(log_tail("bcc.log").find(message), std::string::npos)
            << "bcc.log does not contain '" << message << "':\n"
            << log_tail("bcc.log");
        EXPECT_FALSE(fs::exists(m_dir / "test.basm"));
    }
};

} // namespace

TEST(compiler_integration, paths_defined)
{
    EXPECT_FALSE(std::string(EMULATOR_PATH).empty());
    EXPECT_FALSE(std::string(ASSEMBLER_PATH).empty());
    EXPECT_FALSE(std::string(CCOMPILER_PATH).empty());
    EXPECT_FALSE(std::string(CRT0_PATH).empty());
}

TEST_F(CompilerIntegration, returns_a_constant)
{
    expect_result("int main(void) { return 42; }", 42);
    expect_result("int main() { return 2147483647; }", 2147483647u);
}

TEST_F(CompilerIntegration, falling_off_main_returns_zero)
{
    expect_result("int main(void) { }", 0);
    expect_result("int main(void) { { ; } return 5; return 6; }", 5);
}

TEST_F(CompilerIntegration, integer_constants_and_comments)
{
    expect_result(R"(// a line comment
int main(void)
{
    /* a block
       comment */
    return 0x10 + 010 + 0b11 + 1'000; // 16 + 8 + 3 + 1000
})",
                  1027);
}

TEST_F(CompilerIntegration, arithmetic_and_precedence)
{
    expect_result("int main(void) { return 2 + 3 * 4 - (10 - 4) / 2 % 2; }", 13);
    expect_result("int main(void) { return 100 - 30 - 20; }", 50);
    // Division rounds toward zero and the remainder has the sign of the dividend.
    expect_result("int main(void) { return -7 / 2; }", static_cast<word>(-3));
    expect_result("int main(void) { return -7 % 3; }", static_cast<word>(-1));
    expect_result("int main(void) { return 7 % -3; }", 1);
}

TEST_F(CompilerIntegration, bitwise_and_shifts)
{
    expect_result("int main(void) { return (~5 & 0xFF) ^ (1 << 4 | 3); }", 250 ^ 19);
    // A right shift of a negative number keeps the sign.
    expect_result("int main(void) { return -16 >> 2; }", static_cast<word>(-4));
    expect_result("int main(void) { return 1 << 4 >> 1; }", 8);
}

TEST_F(CompilerIntegration, comparisons_and_logic)
{
    expect_result("int main(void) { return (1 < 2) + (2 <= 2) + (3 > 4) + (4 >= 4) + (5 == 5)"
                  " + (5 != 5) + !0 + !7; }",
                  5);
    // The comparison is signed.
    expect_result("int main(void) { return -1 < 0; }", 1);
    expect_result("int main(void) { return (2 && 3) + (0 || 5) + (0 && 1) + (0 || 0); }", 2);
}

TEST_F(CompilerIntegration, unary_operators)
{
    expect_result("int main(void) { return - -3 + +4 + ~~1 + !!9; }", 9);
}

TEST_F(CompilerIntegration, c_names_that_are_basm_keywords)
{
    // add, b, sp and eq are an instruction, a register and a condition in basm, so a label of
    // that name does not assemble. The functions are _add, _b, ...
    expect_result(R"(int add(void) { return 1; }
void b(void) { return; }
int sp(void) { return 2; }
int eq(void) { return 3; }
int main(void) { return 7; })",
                  7);
}

TEST_F(CompilerIntegration, rejects_what_is_not_valid)
{
    expect_error("int main(void) { return 1 }", "expected ';'");
    expect_error("int main(void) { return 1; ", "expected '}'");
    expect_error("int main(void) { return (1; }", "expected ')'");
    expect_error("int main(void) { return ; }", "has to return a value");
    expect_error("void f(void) { return 1; }", "cannot return a value");
    expect_error("int f(void) { return 1; } int f(void) { return 2; }", "'f' is defined twice");
    expect_error("int main(void) { return 1 + ; }", "expected an expression");
    expect_error("char main(void) { }", "expected 'int' or 'void'");
    expect_error("int main(void) { return 1 @ 2; }", "invalid token");
    expect_error("int main(void) { /* never closed", "unterminated comment");
}

TEST_F(CompilerIntegration, rejects_invalid_integer_constants)
{
    expect_error("int main(void) { return 18446744073709551616; }", "too large");
    expect_error("int main(void) { return 09; }", "invalid integer constant");
    expect_error("int main(void) { return 0x; }", "invalid integer constant");
    expect_error("int main(void) { return 12uu; }", "invalid integer constant");
}

TEST_F(CompilerIntegration, rejects_constants_of_other_types_than_int)
{
    // The lexer knows them, the parser does not yet: their value would be read as an int.
    expect_error("int main(void) { return 2147483648; }",
                 "constants of type 'long long' are not supported yet");
    expect_error("int main(void) { return 12u; }",
                 "constants of type 'unsigned int' are not supported yet");
    expect_error("int main(void) { return 0x80000000; }",
                 "constants of type 'unsigned int' are not supported yet");
    expect_error("int main(void) { return 1.5; }",
                 "constants of type 'double' are not supported yet");
}

TEST_F(CompilerIntegration, character_constants_and_digraphs)
{
    expect_result("int main(void) <% return 'A' + '\\n' + '\\377'; %>", 65 + 10 - 1);
}

TEST_F(CompilerIntegration, preprocessor)
{
    write_file("defs.h", "#ifndef DEFS_H\n#define DEFS_H\n#define TWICE(x) ((x) + (x))\n"
                         "int seven(void) { return 7; }\n#endif\n");
    expect_result(R"(#include "defs.h"
#include "defs.h"
#define LIMIT 10
#define CAT(a, b) a ## b
#if LIMIT > 5 && defined TWICE && __STDC_VERSION__ >= 201710L
int CAT(ma, in)(void) { return TWICE(LIMIT) + __LINE__; }
#else
int main(void) { return 0; }
#endif
)",
                  26);
}

TEST_F(CompilerIntegration, preprocessor_options)
{
    fs::create_directories(m_dir / "inc");
    write_file("inc/value.h", "#define VALUE 30\n");
    expect_result("#include <value.h>\nint main(void) { return VALUE + EXTRA + ONE; }", 35,
                  "-I inc -D EXTRA=4 -DONE -U __bcc__");

    write_file("test.c", "#define F(x) x + x\nF(1)\n");
    ASSERT_EQ(run_command(CCOMPILER_PATH, "-E -o test.i test.c", "bcc.log"), 0)
        << log_tail("bcc.log");
    EXPECT_EQ(log_tail("test.i"), "1 + 1\n");
}

TEST_F(CompilerIntegration, preprocessor_errors_name_the_file_and_line)
{
    write_file("bad.h", "\n\nint f(void) { return 1 + ; }\n");
    expect_error("#include \"bad.h\"\nint main(void) { return 0; }", "bad.h:3:");
    expect_error("#line 50 \"renamed.c\"\nint main(void) { return @; }", "renamed.c:50:");
    expect_error("#if 1\nint main(void) { return 0; }\n", "unterminated #if");
    expect_error("#error not done\n", "#error not done");
}

TEST_F(CompilerIntegration, reports_the_place_of_an_error)
{
    EXPECT_EQ(compile("int main(void)\n{\n    return 1 + ;\n}\n"), 1);
    const std::string log = log_tail("bcc.log");
    EXPECT_NE(log.find("test.c:3:     return 1 + ;"), std::string::npos) << log;
    EXPECT_NE(log.find("^ expected an expression"), std::string::npos) << log;
}

TEST_F(CompilerIntegration, usage_errors)
{
    EXPECT_EQ(run_command(CCOMPILER_PATH, "", "bcc.log"), 1);
    EXPECT_EQ(run_command(CCOMPILER_PATH, "a.c b.c", "bcc.log"), 1);
    EXPECT_EQ(run_command(CCOMPILER_PATH, "a.c -I", "bcc.log"), 1);
    EXPECT_EQ(run_command(CCOMPILER_PATH, "missing.c", "bcc.log"), 1);
    EXPECT_NE(log_tail("bcc.log").find("cannot open 'missing.c'"), std::string::npos);
}
