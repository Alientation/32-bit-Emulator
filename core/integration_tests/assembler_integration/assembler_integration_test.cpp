// End-to-end tests for the basm toolchain. The real `basm` executable preprocesses, assembles,
// and links .basm sources into .bo/.ba/.bexe files, which are inspected with ObjectFile. The
// real `emu32` executable then runs each .bexe and dumps its final state (--format plain), which
// the tests assert on.

#include "assembler/object_file.h"
#include "emulator32bit/emulator32bit.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <sys/wait.h>

namespace fs = std::filesystem;

namespace
{

/// Upper bound on instructions per program so a miscompiled loop can't hang the test.
constexpr int kMaxInstructions = 100000;

/// Default linker script layout.
constexpr word kDataStart = 0x1000;

/// Each test builds in its own scratch directory. It is removed if the test passes and kept
/// (along with basm.log, emu32.log and state.txt) if it fails.
class AssemblerIntegration : public ::testing::Test
{
  protected:
    fs::path m_dir;

    /// Exit code of the last emu32 run and the key=value state it dumped.
    int m_emu_exit = -1;
    std::unordered_map<std::string, std::string> m_state;

    void SetUp() override
    {
        const ::testing::TestInfo *info = ::testing::UnitTest::GetInstance()->current_test_info();
        m_dir = fs::temp_directory_path() / "aemu_assembler_integration"
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

    /// Writes a file into the scratch directory and returns its path.
    std::string write_file(const std::string &name, const std::string &contents)
    {
        const fs::path path = m_dir / name;
        fs::create_directories(path.parent_path());
        std::ofstream(path) << contents;
        return path.string();
    }

    std::string path(const std::string &name) const
    {
        return (m_dir / name).string();
    }

    bool exists(const std::string &name) const
    {
        return fs::exists(m_dir / name);
    }

    /// Runs a command from the scratch directory with output redirected to `log`. Returns the
    /// exit code.
    int run_command(const std::string &exe, const std::string &args, const std::string &log)
    {
        const std::string cmd =
            "cd \"" + m_dir.string() + "\" && \"" + exe + "\" " + args + " > " + log + " 2>&1";
        const int status = std::system(cmd.c_str());
        return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }

    /// Runs basm from the scratch directory. Output goes to basm.log. Returns the exit code.
    int basm(const std::string &args)
    {
        return run_command(ASSEMBLER_PATH, args, "basm.log");
    }

    /// Like basm(), but fails the test with the tail of the log if basm did not succeed.
    void build(const std::string &args)
    {
        const int code = basm(args);
        ASSERT_EQ(code, 0) << "basm " << args << "\n" << log_tail("basm.log");
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

    /// Runs emu32 with a plain state dump to state.txt and loads that dump into m_state. Returns
    /// the exit code.
    int emu32(const std::string &args)
    {
        fs::remove(m_dir / "state.txt");
        m_state.clear();
        m_emu_exit = run_command(EMULATOR_PATH, args + " --format plain -o state.txt", "emu32.log");

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
        return m_emu_exit;
    }

    /// Runs an executable on emu32 and expects it to halt within kMaxInstructions.
    void run(const std::string &exe_name, const std::string &extra_args = "")
    {
        ASSERT_TRUE(exists(exe_name)) << exe_name << " was not produced\n" << log_tail("basm.log");
        emu32("-e " + exe_name + " -l " + std::to_string(kMaxInstructions) + " " + extra_args);
        ASSERT_EQ(m_emu_exit, S32(Emulator32bit::EmuCLIExitCode::EXIT_HALTED))
            << "status=" << state("status") << "\n"
            << log_tail("emu32.log");
    }

    /// A value from the last state dump, or "" if it is missing (which also fails the test).
    std::string state(const std::string &key)
    {
        const auto it = m_state.find(key);
        if (it == m_state.end())
        {
            ADD_FAILURE() << "emu32 state dump has no '" << key << "'\n" << log_tail("emu32.log");
            return "";
        }
        return it->second;
    }

    word state_number(const std::string &key)
    {
        const std::string value = state(key);
        return value.empty() ? 0 : static_cast<word>(std::stoull(value, nullptr, 0));
    }

    word reg(U8 r)
    {
        return state_number("x" + std::to_string(r));
    }

    bool flag(const std::string &name)
    {
        return state(name) == "1";
    }

    /// Bytes dumped for a `-m addr:len` range.
    std::vector<byte> mem(word addr)
    {
        char key[32];
        std::snprintf(key, sizeof(key), "mem[0x%08x]", addr);
        std::vector<byte> bytes;
        std::stringstream ss(state(key));
        std::string hex_byte;
        while (ss >> hex_byte)
        {
            bytes.push_back(static_cast<byte>(std::stoul(hex_byte, nullptr, 16)));
        }
        return bytes;
    }

    /// Looks up a symbol by name. Fails the test if it is not present.
    static const ObjectFile::SymbolTableEntry *symbol(const ObjectFile &obj,
                                                      const std::string &name)
    {
        const auto str = obj.string_table.find(name);
        if (str == obj.string_table.end())
        {
            ADD_FAILURE() << "symbol " << name << " is not in the string table";
            return nullptr;
        }
        const auto sym = obj.symbol_table.find(str->second);
        if (sym == obj.symbol_table.end())
        {
            ADD_FAILURE() << "symbol " << name << " is not in the symbol table";
            return nullptr;
        }
        return &sym->second;
    }

    static bool has_relocation(const ObjectFile &obj, const std::string &name,
                               ObjectFile::RelocationEntry::Type type)
    {
        const auto str = obj.string_table.find(name);
        if (str == obj.string_table.end())
        {
            return false;
        }
        for (const ObjectFile::RelocationEntry &rel : obj.rel_text)
        {
            if (rel.symbol == str->second && rel.type == type)
            {
                return true;
            }
        }
        return false;
    }
};

using Binding = ObjectFile::SymbolTableEntry::BindingInfo;
using RelType = ObjectFile::RelocationEntry::Type;

} // namespace

TEST(assembler_integration, paths_defined)
{
    EXPECT_FALSE(std::string(EMULATOR_PATH).empty());
    EXPECT_FALSE(std::string(ASSEMBLER_PATH).empty());
}

// Single source file -> .bo -> .bexe, checking the object file and the program's result.
TEST_F(AssemblerIntegration, single_file_arithmetic)
{
    write_file("arith.basm", R"(.global _start

.text
_start:
                add     x0, xzr, 20             ; x0 = 20
                add     x1, xzr, 22             ; x1 = 22
                add     x2, x0, x1              ; x2 = 42
                sub     x3, x2, 2               ; x3 = 40
                lsl     x4, x3, 4               ; x4 = 640
                mul     x5, x0, x1              ; x5 = 440
                hlt
)");
    ASSERT_NO_FATAL_FAILURE(build("-o arith arith.basm -outdir ."));

    ASSERT_TRUE(exists("arith.bo"));
    ObjectFile obj(File(path("arith.bo")));
    EXPECT_EQ(obj.file_type, ObjectFile::kRelocatableFileType);
    EXPECT_EQ(obj.text_section.size(), 7u);
    const ObjectFile::SymbolTableEntry *start = symbol(obj, "_start");
    ASSERT_NE(start, nullptr);
    EXPECT_EQ(start->binding_info, Binding::GLOBAL);
    EXPECT_EQ(start->symbol_value, 0u);

    ASSERT_TRUE(exists("arith.bexe"));
    ObjectFile exe(File(path("arith.bexe")));
    EXPECT_EQ(exe.file_type, ObjectFile::kExecutableFileType);

    ASSERT_NO_FATAL_FAILURE(run("arith.bexe"));
    EXPECT_EQ(state("status"), "halted");
    EXPECT_EQ(state_number("instructions"), 6u); // the hlt itself is not counted
    EXPECT_EQ(reg(0), 20u);
    EXPECT_EQ(reg(1), 22u);
    EXPECT_EQ(reg(2), 42u);
    EXPECT_EQ(reg(3), 40u);
    EXPECT_EQ(reg(4), 640u);
    EXPECT_EQ(reg(5), 440u);
}

// Labels, cmp, and conditional/unconditional branches.
TEST_F(AssemblerIntegration, loop_with_branches)
{
    write_file("fib.basm", R"(.global _start

.text
_start:
                add     x0, xzr, 10             ; iterations
                add     x1, xzr, 0              ; fib(0)
                add     x2, xzr, 1              ; fib(1)
loop_begin:
                cmp     x0, 0
                b.le    loop_end
                add     x3, x1, 0
                add     x1, x2, 0
                add     x2, x2, x3
                sub     x0, x0, 1
                b       loop_begin
loop_end:
                hlt
)");
    ASSERT_NO_FATAL_FAILURE(build("-o fib fib.basm -outdir ."));
    ASSERT_NO_FATAL_FAILURE(run("fib.bexe"));

    EXPECT_EQ(reg(0), 0u);
    EXPECT_EQ(reg(1), 55u); // fib(10)
    EXPECT_EQ(reg(2), 89u); // fib(11)
}

// bl/ret within a single file.
TEST_F(AssemblerIntegration, call_and_return)
{
    write_file("call.basm", R"(.global _start

.text
_start:
                add     x0, xzr, 5
                bl      double
                bl      double
                add     x1, x0, 0
                hlt

double:
                add     x0, x0, x0
                ret
)");
    ASSERT_NO_FATAL_FAILURE(build("-o call call.basm -outdir ."));
    ASSERT_NO_FATAL_FAILURE(run("call.bexe"));

    EXPECT_EQ(reg(0), 20u);
    EXPECT_EQ(reg(1), 20u);
}

// .data/.bss symbols are reached through adrp + :lo12: relocations that the linker resolves.
TEST_F(AssemblerIntegration, data_and_bss_relocations)
{
    write_file("data.basm", R"(.global _start

.text
_start:
                adrp    x0, values
                add     x0, x0, :lo12:values
                ldr     x1, [x0]                ; values[0]
                ldr     x2, [x0, 4]             ; values[1]
                add     x3, x1, x2

                adrp    x4, result
                add     x4, x4, :lo12:result
                str     x3, [x4]
                ldr     x5, [x4]                ; read the stored sum back

                adrp    x6, message
                add     x6, x6, :lo12:message
                ldrb    x7, [x6]                ; 'h'
                ldrb    x8, [x6, 4]             ; 'o'
                hlt

.data
values:         .word 1000, 234
message:        .asciz "hello"

.bss
result:         .advance 4
)");
    ASSERT_NO_FATAL_FAILURE(build("-o data data.basm -outdir ."));

    ObjectFile obj(File(path("data.bo")));
    EXPECT_TRUE(has_relocation(obj, "values", RelType::R_EMU32_ADRP_HI20));
    EXPECT_TRUE(has_relocation(obj, "values", RelType::R_EMU32_O_LO12));
    EXPECT_TRUE(has_relocation(obj, "result", RelType::R_EMU32_ADRP_HI20));
    EXPECT_EQ(obj.data_section.size(), 8u + 6u);
    EXPECT_EQ(obj.bss_section, 4u);

    // .data is 14 bytes at 0x1000 and .bss follows directly, so result is at 0x100e.
    ASSERT_NO_FATAL_FAILURE(run("data.bexe", "-m 0x1000:14,0x100e:4"));
    EXPECT_EQ(mem(kDataStart),
              (std::vector<byte>{0xe8, 0x03, 0, 0, 0xea, 0, 0, 0, 'h', 'e', 'l', 'l', 'o', 0}));
    EXPECT_EQ(mem(0x100e), (std::vector<byte>{0xd2, 0x04, 0, 0})); // 1234
    EXPECT_EQ(reg(1), 1000u);
    EXPECT_EQ(reg(2), 234u);
    EXPECT_EQ(reg(3), 1234u);
    EXPECT_EQ(reg(5), 1234u);
    EXPECT_EQ(reg(7), word('h'));
    EXPECT_EQ(reg(8), word('o'));
}

// Two source files: _start calls a function defined (and exported with .global) in the other.
TEST_F(AssemblerIntegration, link_multiple_object_files)
{
    write_file("main.basm", R"(.global _start

.text
_start:
                add     x0, xzr, 7
                add     x1, xzr, 8
                bl      add_numbers
                add     x10, x0, 0
                hlt
)");
    write_file("math.basm", R"(.global add_numbers

.text
add_numbers:
                add     x0, x0, x1
                ret
)");
    ASSERT_NO_FATAL_FAILURE(build("-o prog main.basm math.basm -outdir ."));

    ObjectFile main_obj(File(path("main.bo")));
    const ObjectFile::SymbolTableEntry *undefined = symbol(main_obj, "add_numbers");
    ASSERT_NE(undefined, nullptr);
    EXPECT_EQ(undefined->binding_info, Binding::WEAK);

    ObjectFile math_obj(File(path("math.bo")));
    const ObjectFile::SymbolTableEntry *defined = symbol(math_obj, "add_numbers");
    ASSERT_NE(defined, nullptr);
    EXPECT_EQ(defined->binding_info, Binding::GLOBAL);

    ASSERT_NO_FATAL_FAILURE(run("prog.bexe"));
    EXPECT_EQ(reg(10), 15u);
}

// -c stops after producing object files.
TEST_F(AssemblerIntegration, compile_only)
{
    write_file("only.basm", R"(.global _start

.text
_start:
                hlt
)");
    ASSERT_NO_FATAL_FAILURE(build("-c -o only only.basm -outdir ."));
    EXPECT_TRUE(exists("only.bo"));
    EXPECT_FALSE(exists("only.bexe"));
}

// -ar packages object files into a .ba, which -l then links into an executable.
TEST_F(AssemblerIntegration, static_library)
{
    write_file("lib/square.basm", R"(.global square

.text
square:
                mul     x0, x0, x0
                ret
)");
    ASSERT_NO_FATAL_FAILURE(build("-ar -o libsquare lib/square.basm -outdir ."));
    ASSERT_TRUE(exists("libsquare.ba")) << log_tail("basm.log");

    write_file("main.basm", R"(.global _start

.text
_start:
                add     x0, xzr, 12
                bl      square
                hlt
)");
    ASSERT_NO_FATAL_FAILURE(build("-l libsquare.ba -o prog main.basm -outdir ."));
    ASSERT_NO_FATAL_FAILURE(run("prog.bexe"));
    EXPECT_EQ(reg(0), 144u);
}

// #include from an -I directory plus #define, resolved by the preprocessor before assembly.
TEST_F(AssemblerIntegration, preprocessor_include_and_define)
{
    write_file("include/constants.binc", R"(#define BASE 100
)");
    write_file("pre.basm", R"(#include <"constants.binc">
#define OFFSET 23

.global _start

.text
_start:
                add     x0, xzr, BASE
                add     x0, x0, OFFSET
                hlt
)");
    ASSERT_NO_FATAL_FAILURE(build("-I include -o pre pre.basm -outdir ."));
    ASSERT_NO_FATAL_FAILURE(run("pre.bexe"));
    EXPECT_EQ(reg(0), 123u);
}

// Bad input makes basm exit with a failure code instead of producing an executable.
TEST_F(AssemblerIntegration, invalid_instruction_fails)
{
    write_file("bad.basm", R"(.global _start

.text
_start:
                add     x0, xzr
                hlt
)");
    EXPECT_NE(basm("-o bad bad.basm -outdir ."), 0) << log_tail("basm.log");
    EXPECT_FALSE(exists("bad.bexe"));
}

// ---------------------------------------------------------------------------------------------
// emu32 behavior
// ---------------------------------------------------------------------------------------------

// --reg and --flags set the state the program starts with (the loader only sets the PC).
TEST_F(AssemblerIntegration, emu32_initial_registers_and_flags)
{
    write_file("init.basm", R"(.global _start

.text
_start:
                b.eq    zero_set                ; depends only on the initial flags
                mul     x2, x0, x1
                hlt
zero_set:
                add     x2, x0, x1
                hlt
)");
    ASSERT_NO_FATAL_FAILURE(build("-o init init.basm -outdir ."));

    ASSERT_NO_FATAL_FAILURE(run("init.bexe", "--reg x0=6,x1=0x7 --flags 0"));
    EXPECT_EQ(reg(2), 42u);
    EXPECT_FALSE(flag("Z"));

    ASSERT_NO_FATAL_FAILURE(run("init.bexe", "--reg x0=6,x1=0b111 --flags 0b0100"));
    EXPECT_EQ(reg(2), 13u);
    EXPECT_TRUE(flag("Z"));
}

// A program that never halts stops at --limit and emu32 reports it with its own exit code.
TEST_F(AssemblerIntegration, emu32_instruction_limit)
{
    write_file("spin.basm", R"(.global _start

.text
_start:
                add     x0, x0, 1
                b       _start
)");
    ASSERT_NO_FATAL_FAILURE(build("-o spin spin.basm -outdir ."));

    EXPECT_EQ(emu32("-e spin.bexe -l 101"), S32(Emulator32bit::EmuCLIExitCode::EXIT_LIMIT_REACHED))
        << log_tail("emu32.log");
    EXPECT_EQ(state("status"), "limit");
    EXPECT_EQ(state_number("instructions"), 101u);
    EXPECT_EQ(reg(0), 51u);
}

// An access to an unmapped page stops the program with a fault instead of crashing emu32.
TEST_F(AssemblerIntegration, emu32_reports_faults)
{
    write_file("fault.basm", R"(.global _start

.text
_start:
                add     x0, xzr, 1
                lsl     x0, x0, 30              ; nothing is mapped at 0x40000000
                ldr     x1, [x0]
                hlt
)");
    ASSERT_NO_FATAL_FAILURE(build("-o fault fault.basm -outdir ."));

    EXPECT_EQ(emu32("-e fault.bexe -l 100"), S32(Emulator32bit::EmuCLIExitCode::EXIT_FAULT))
        << log_tail("emu32.log");
    EXPECT_EQ(state("status"), "fault");
    EXPECT_EQ(state_number("instructions"), 2u);
    EXPECT_EQ(state_number("pc"), 8u); // the faulting ldr
}

// Bad arguments are rejected before anything runs.
TEST_F(AssemblerIntegration, emu32_usage_errors)
{
    EXPECT_EQ(emu32("-e missing.bexe"), S32(Emulator32bit::EmuCLIExitCode::EXIT_USAGE_ERROR));
    EXPECT_EQ(emu32("--reg x32=1"), S32(Emulator32bit::EmuCLIExitCode::EXIT_USAGE_ERROR));
    EXPECT_EQ(emu32("--reg x1=1,x1=2"), S32(Emulator32bit::EmuCLIExitCode::EXIT_USAGE_ERROR));
    EXPECT_EQ(emu32("--limit ten"), S32(Emulator32bit::EmuCLIExitCode::EXIT_USAGE_ERROR));
    // emu32() appends --format plain, so call this one directly.
    EXPECT_EQ(run_command(EMULATOR_PATH, "--format fancy", "emu32.log"),
              S32(Emulator32bit::EmuCLIExitCode::EXIT_USAGE_ERROR));
}

// ---------------------------------------------------------------------------------------------
// Instructions
// ---------------------------------------------------------------------------------------------

// Conditions after cmp. Each taken branch sets a bit in x10.
TEST_F(AssemblerIntegration, condition_codes)
{
    write_file("cond.basm", R"(.global _start

.text
_start:
                sub     x1, xzr, 1              ; x1 = -1 (0xffffffff)
                add     x2, xzr, 1
                add     x10, xzr, 0

                cmp     x1, x2                  ; -1 < 1
                b.lt    signed_less
                b       check_eq
signed_less:
                orr     x10, x10, 1
check_eq:
                cmp     x2, 1
                b.ne    check_ge
                orr     x10, x10, 2
check_ge:
                cmp     x2, x1                  ; 1 >= -1
                b.ge    signed_ge
                b       check_gt
signed_ge:
                orr     x10, x10, 4
check_gt:
                cmp     x1, x2                  ; -1 > 1 is false
                b.gt    done
                orr     x10, x10, 8
                cmp     x2, 2
                b.mi    negative                ; 1 - 2 < 0
                b       done
negative:
                orr     x10, x10, 16
done:
                hlt
)");
    ASSERT_NO_FATAL_FAILURE(build("-o cond cond.basm -outdir ."));
    ASSERT_NO_FATAL_FAILURE(run("cond.bexe"));
    EXPECT_EQ(reg(10), 0b11111u);
}

// Subtraction/cmp set C ARM style (C = no borrow), so HI/LS/HS/LO branch correctly.
TEST_F(AssemblerIntegration, unsigned_condition_codes)
{
    write_file("ucond.basm", R"(.global _start

.text
_start:
                sub     x1, xzr, 1              ; 0xffffffff
                add     x2, xzr, 1
                add     x10, xzr, 0

                cmp     x1, x2
                b.hi    higher                  ; 0xffffffff > 1 unsigned
                b       check_lo
higher:
                orr     x10, x10, 1
check_lo:
                cmp     x2, x1
                b.lo    lower                   ; 1 < 0xffffffff unsigned
                b       check_hs
lower:
                orr     x10, x10, 2
check_hs:
                cmp     x2, x2
                b.hs    same_or_higher
                b       done
same_or_higher:
                orr     x10, x10, 4
done:
                hlt
)");
    ASSERT_NO_FATAL_FAILURE(build("-o ucond ucond.basm -outdir ."));
    ASSERT_NO_FATAL_FAILURE(run("ucond.bexe"));
    EXPECT_EQ(reg(10), 0b111u);
}

// The s suffix sets NZCV, which emu32 dumps.
TEST_F(AssemblerIntegration, flag_setting_arithmetic)
{
    write_file("flags.basm", R"(.global _start

.text
_start:
                sub     x1, xzr, 1              ; 0xffffffff
                adds    x2, x1, 1               ; wraps to 0: Z and C set
                hlt
)");
    ASSERT_NO_FATAL_FAILURE(build("-o flags flags.basm -outdir ."));
    ASSERT_NO_FATAL_FAILURE(run("flags.bexe", "--flags 0"));
    EXPECT_EQ(reg(2), 0u);
    EXPECT_FALSE(flag("N"));
    EXPECT_TRUE(flag("Z"));
    EXPECT_TRUE(flag("C"));
    EXPECT_FALSE(flag("V"));
}

TEST_F(AssemblerIntegration, logical_and_shift_operations)
{
    write_file("logic.basm", R"(.global _start

.text
_start:
                mov     x0, $ff0                ; 0x00000ff0
                and     x1, x0, $0f0            ; 0x0f0
                orr     x2, x0, $00f            ; 0xfff
                eor     x3, x0, $0ff            ; 0xf0f
                bic     x4, x0, $f00            ; 0x0f0
                lsl     x5, x0, 4               ; 0xff00
                lsr     x6, x0, 4               ; 0xff
                sub     x7, xzr, 16             ; 0xfffffff0
                asr     x8, x7, 4               ; 0xffffffff
                lsr     x9, x7, 4               ; 0x0fffffff
                ror     x11, x0, 8              ; 0xf000000f
                add     x12, x0, x6, lsl 4      ; 0xff0 + 0xff0
                sub     x13, x0, x0, lsr 4      ; 0xff0 - 0xff
                mvn     x14, 0                  ; 0xffffffff
                mov     x15, x0                 ; register move
                hlt
)");
    ASSERT_NO_FATAL_FAILURE(build("-o logic logic.basm -outdir ."));
    ASSERT_NO_FATAL_FAILURE(run("logic.bexe"));
    EXPECT_EQ(reg(1), 0x0f0u);
    EXPECT_EQ(reg(2), 0xfffu);
    EXPECT_EQ(reg(3), 0xf0fu);
    EXPECT_EQ(reg(4), 0x0f0u);
    EXPECT_EQ(reg(5), 0xff00u);
    EXPECT_EQ(reg(6), 0xffu);
    EXPECT_EQ(reg(8), 0xffffffffu);
    EXPECT_EQ(reg(9), 0x0fffffffu);
    EXPECT_EQ(reg(11), 0xf000000fu);
    EXPECT_EQ(reg(12), 0x1fe0u);
    EXPECT_EQ(reg(13), 0xef1u);
    EXPECT_EQ(reg(14), 0xffffffffu);
    EXPECT_EQ(reg(15), 0xff0u);
}

// $hex, %binary and @octal literals in instructions and data.
TEST_F(AssemblerIntegration, number_literals)
{
    write_file("lit.basm", R"(.global _start

.text
_start:
                add     x0, xzr, $2A
                add     x1, xzr, %101010
                add     x2, xzr, @52
                add     x3, xzr, 42
                adrp    x4, table
                add     x4, x4, :lo12:table
                ldrb    x5, [x4]
                ldrb    x6, [x4, 1]
                ldrb    x7, [x4, 2]
                hlt
.data
table:          .byte $2A, %101010, @52
)");
    ASSERT_NO_FATAL_FAILURE(build("-o lit lit.basm -outdir ."));
    ASSERT_NO_FATAL_FAILURE(run("lit.bexe"));
    for (U8 r : {0, 1, 2, 3, 5, 6, 7})
    {
        EXPECT_EQ(reg(r), 42u) << "x" << int(r);
    }
}

// Pre-index and post-index write back the base register; register offsets can be shifted.
TEST_F(AssemblerIntegration, memory_addressing_modes)
{
    write_file("mem.basm", R"(.global _start

.text
_start:
                adrp    x0, buf
                add     x0, x0, :lo12:buf
                add     x9, x0, 0               ; keep the base address

                add     x1, xzr, $11
                str     x1, [x0], 4             ; buf[0], then x0 += 4
                add     x1, xzr, $22
                str     x1, [x0, 4]!            ; x0 += 4, then buf[2]
                add     x1, xzr, $33
                add     x2, xzr, 3
                str     x1, [x9, x2, lsl 2]     ; buf[3]
                add     x1, xzr, $44
                str     x1, [x9, 4]             ; buf[1]

                add     x1, xzr, $1234
                strh    x1, [x9, 16]
                add     x1, xzr, $ab
                strb    x1, [x9, 18]

                sub     x3, x0, x9              ; 8: x0 was written back twice
                ldr     x4, [x9, 12]
                ldrh    x5, [x9, 16]
                ldrb    x6, [x9, 18]
                ldrsb   x7, [x9, 18]            ; sign extends 0xab
                add     x1, xzr, 1
                lsl     x1, x1, 15
                strh    x1, [x9, 16]            ; 0x8000
                ldrsh   x8, [x9, 16]            ; sign extends 0x8000
                add     x1, xzr, $1234
                strh    x1, [x9, 16]
                hlt
.bss
buf:            .advance 20
)");
    ASSERT_NO_FATAL_FAILURE(build("-o mem mem.basm -outdir ."));
    // Without a .data section, .bss starts at the data address.
    ASSERT_NO_FATAL_FAILURE(run("mem.bexe", "-m 0x1000:20"));

    EXPECT_EQ(reg(9), kDataStart);
    EXPECT_EQ(reg(3), 8u);
    EXPECT_EQ(reg(4), 0x33u);
    EXPECT_EQ(reg(5), 0x1234u);
    EXPECT_EQ(reg(6), 0xabu);
    EXPECT_EQ(reg(7), 0xffffffabu);
    EXPECT_EQ(reg(8), 0xffff8000u);
    EXPECT_EQ(mem(kDataStart), (std::vector<byte>{0x11, 0, 0,    0, 0x44, 0, 0,    0,    0x22, 0,
                                                  0,    0, 0x33, 0, 0,    0, 0x34, 0x12, 0xab, 0}));
}

// Memory offsets are signed 12 bit values, so a negative offset (and `[sp, -4]!` push) works.
TEST_F(AssemblerIntegration, memory_negative_offsets)
{
    write_file("neg.basm", R"(.global _start

.text
_start:
                adrp    x0, buf
                add     x0, x0, :lo12:buf
                add     x9, x0, 16              ; points at the end of buf
                add     x1, xzr, $11
                str     x1, [x9, -4]            ; buf[3]
                add     x1, xzr, $22
                str     x1, [x9, -8]!           ; x9 -= 8, then buf[2]
                add     x1, xzr, $33
                str     x1, [x9], -4            ; buf[2] = $33, then x9 -= 4
                sub     x3, x9, x0              ; 4
                ldr     x4, [x0, 8]
                ldr     x5, [x0, 12]
                hlt
.bss
buf:            .advance 16
)");
    ASSERT_NO_FATAL_FAILURE(build("-o neg neg.basm -outdir ."));
    ASSERT_NO_FATAL_FAILURE(run("neg.bexe"));

    EXPECT_EQ(reg(3), 4u);
    EXPECT_EQ(reg(4), 0x33u);
    EXPECT_EQ(reg(5), 0x11u);
}

// Offsets outside of the signed 12 bit range are rejected.
TEST_F(AssemblerIntegration, memory_offset_out_of_range_fails)
{
    write_file("big.basm", R"(.global _start

.text
_start:
                ldr     x1, [x0, 2048]
                hlt
)");
    EXPECT_NE(basm("-o big big.basm -outdir ."), 0) << log_tail("basm.log");
    EXPECT_FALSE(exists("big.bexe"));
}

// Shifts with the S suffix update NZC like ARM, shifts without it leave the flags alone.
TEST_F(AssemblerIntegration, shift_flag_setting)
{
    write_file("shifts.basm", R"(.global _start

.text
_start:
                mov     x1, 1
                lsl     x1, x1, 31              ; 0x80000000, no flags
                lsls    x2, x1, 1               ; 0, C=1, Z=1
                hlt
)");
    ASSERT_NO_FATAL_FAILURE(build("-o shifts shifts.basm -outdir ."));
    ASSERT_NO_FATAL_FAILURE(run("shifts.bexe"));

    EXPECT_EQ(reg(1), 0x80000000u);
    EXPECT_EQ(reg(2), 0u);
    EXPECT_TRUE(flag("Z"));
    EXPECT_TRUE(flag("C"));
    EXPECT_FALSE(flag("N"));
}

// Recursion: factorial saves x29 and its argument on a stack carved out of .bss.
TEST_F(AssemblerIntegration, recursion_with_stack)
{
    write_file("fact.basm", R"(.global _start

.text
_start:
                adrp    sp, stack_top
                add     sp, sp, :lo12:stack_top
                add     x0, xzr, 6
                bl      factorial
                hlt

; x0 = x0!
factorial:
                cmp     x0, 1
                b.gt    recurse
                add     x0, xzr, 1
                ret
recurse:
                sub     sp, sp, 8
                str     x29, [sp]
                str     x0, [sp, 4]
                sub     x0, x0, 1
                bl      factorial
                ldr     x1, [sp, 4]
                ldr     x29, [sp]
                add     sp, sp, 8
                mul     x0, x0, x1
                ret

.bss
stack:          .advance 256
stack_top:
)");
    ASSERT_NO_FATAL_FAILURE(build("-o fact fact.basm -outdir ."));
    ASSERT_NO_FATAL_FAILURE(run("fact.bexe"));
    EXPECT_EQ(reg(0), 720u);
    EXPECT_EQ(state_number("sp"), kDataStart + 256); // balanced pushes and pops
}

// blx calls through a register holding a function's address.
TEST_F(AssemblerIntegration, indirect_call)
{
    write_file("indirect.basm", R"(.global _start

.text
_start:
                adrp    x5, triple
                add     x5, x5, :lo12:triple
                add     x0, xzr, 7
                blx     x5
                hlt

triple:
                add     x0, x0, x0, lsl 1
                ret
)");
    ASSERT_NO_FATAL_FAILURE(build("-o indirect indirect.basm -outdir ."));
    ASSERT_NO_FATAL_FAILURE(run("indirect.bexe"));
    EXPECT_EQ(reg(0), 21u);
}

// ---------------------------------------------------------------------------------------------
// Directives
// ---------------------------------------------------------------------------------------------

// Data directive sizes, little endian layout, and padding directives.
TEST_F(AssemblerIntegration, data_directive_layout)
{
    write_file("layout.basm", R"(.global _start

.text
_start:
                adrp    x0, words
                add     x0, x0, :lo12:words
                ldr     x1, [x0]
                adrp    x2, tail
                add     x2, x2, :lo12:tail
                ldrb    x3, [x2]
                hlt

.data
bytes:          .byte 1, 2, 3
                .align 4
hwords:         .dbyte $1234
words:          .word $deadbeef
dwords:         .dword $0102030405060708
chars:          .char 'h', 'i'
text:           .ascii "ab"
ztext:          .asciz "cd"
                .advance 2
                .org 32
tail:           .byte $7f
)");
    ASSERT_NO_FATAL_FAILURE(build("-o layout layout.basm -outdir ."));

    ObjectFile obj(File(path("layout.bo")));
    ASSERT_EQ(obj.data_section.size(), 33u);
    const std::vector<std::pair<std::string, word>> offsets = {
        {"bytes", 0},  {"hwords", 4}, {"words", 6},  {"dwords", 10},
        {"chars", 18}, {"text", 20},  {"ztext", 22}, {"tail", 32},
    };
    for (const auto &[name, offset] : offsets)
    {
        const ObjectFile::SymbolTableEntry *sym = symbol(obj, name);
        ASSERT_NE(sym, nullptr);
        EXPECT_EQ(sym->symbol_value, offset) << name;
    }

    ASSERT_NO_FATAL_FAILURE(run("layout.bexe", "-m 0x1000:33"));
    EXPECT_EQ(mem(kDataStart),
              (std::vector<byte>{1,   2,   3, 0, 0x34, 0x12, 0xef, 0xbe, 0xad, 0xde, 8,
                                 7,   6,   5, 4, 3,    2,    1,    'h',  'i',  'a',  'b',
                                 'c', 'd', 0, 0, 0,    0,    0,    0,    0,    0,    0x7f}));
    EXPECT_EQ(reg(1), 0xdeadbeefu);
    EXPECT_EQ(reg(3), 0x7fu);
}

// ---------------------------------------------------------------------------------------------
// Linking
// ---------------------------------------------------------------------------------------------

// A .data symbol exported from one file is read by code in another file.
TEST_F(AssemblerIntegration, cross_file_data_symbol)
{
    write_file("main.basm", R"(.global _start

.text
_start:
                adrp    x0, shared_table
                add     x0, x0, :lo12:shared_table
                ldr     x1, [x0, 4]
                bl      sum_table
                hlt

.data
padding:        .word 0, 0, 0
)");
    write_file("table.basm", R"(.global shared_table
.global sum_table

.text
; x2 = sum of the three table entries
sum_table:
                adrp    x3, shared_table
                add     x3, x3, :lo12:shared_table
                ldr     x2, [x3]
                ldr     x4, [x3, 4]
                add     x2, x2, x4
                ldr     x4, [x3, 8]
                add     x2, x2, x4
                ret

.data
shared_table:   .word 100, 20, 3
)");
    ASSERT_NO_FATAL_FAILURE(build("-o prog main.basm table.basm -outdir ."));

    ObjectFile exe(File(path("prog.bexe")));
    EXPECT_EQ(exe.data_section.size(), 24u); // both files' .data merged

    ASSERT_NO_FATAL_FAILURE(run("prog.bexe"));
    EXPECT_EQ(reg(1), 20u);
    EXPECT_EQ(reg(2), 123u);
}

// A library built from two objects, where one library function calls the other.
TEST_F(AssemblerIntegration, static_library_multiple_objects)
{
    write_file("lib/square.basm", R"(.global square

.text
square:
                mul     x0, x0, x0
                ret
)");
    write_file("lib/cube.basm", R"(.global cube

.text
; x0 = x0^3, clobbers x1 and x2
cube:
                add     x1, x0, 0
                add     x2, x29, 0              ; bl overwrites the link register
                bl      square
                mul     x0, x0, x1
                add     x29, x2, 0
                ret
)");
    ASSERT_NO_FATAL_FAILURE(build("-ar -o libpow lib/square.basm lib/cube.basm -outdir ."));
    ASSERT_TRUE(exists("libpow.ba")) << log_tail("basm.log");

    write_file("main.basm", R"(.global _start

.text
_start:
                add     x0, xzr, 3
                bl      cube
                add     x10, x0, 0
                add     x0, xzr, 9
                bl      square
                hlt
)");
    ASSERT_NO_FATAL_FAILURE(build("-l libpow.ba -o prog main.basm -outdir ."));
    ASSERT_NO_FATAL_FAILURE(run("prog.bexe"));
    EXPECT_EQ(reg(10), 27u);
    EXPECT_EQ(reg(0), 81u);
}

// ---------------------------------------------------------------------------------------------
// Preprocessor
// ---------------------------------------------------------------------------------------------

TEST_F(AssemblerIntegration, preprocessor_macros_and_conditionals)
{
    write_file("include/util.binc", R"(#ifndef UTIL_BINC
#define UTIL_BINC

; Parameter names must not collide with keywords (e.g. `b` is the branch instruction).
#macro load_sum(dst, lhs, rhs)
                add     dst, xzr, lhs
                add     dst, dst, rhs
#macend

#endif
)");
    write_file("macros.basm", R"(#include <"util.binc">
#include <"util.binc">

#define MODE 2

.global _start

.text
_start:
                #invoke load_sum(x0, 30, 12)
#ifequ MODE 2
                add     x1, xzr, 2
#else
                add     x1, xzr, 99
#endif
#ifdef UNDEFINED_THING
                add     x2, xzr, 99
#elsedef MODE
                add     x2, xzr, 7
#endif
                hlt
)");
    ASSERT_NO_FATAL_FAILURE(build("-I include -o macros macros.basm -outdir ."));
    ASSERT_NO_FATAL_FAILURE(run("macros.bexe"));
    EXPECT_EQ(reg(0), 42u);
    EXPECT_EQ(reg(1), 2u);
    EXPECT_EQ(reg(2), 7u);
}

// ---------------------------------------------------------------------------------------------
// Shipped sample programs (core/app/programs)
// ---------------------------------------------------------------------------------------------

TEST_F(AssemblerIntegration, sample_programs)
{
    // Copy them so the build output stays in the scratch directory.
    fs::copy(PROGRAMS_DIR, m_dir / "programs", fs::copy_options::recursive);
    fs::create_directories(m_dir / "out");

    ASSERT_NO_FATAL_FAILURE(build("-o out/palindrome programs/src/palindrome.basm -outdir out"));
    ASSERT_NO_FATAL_FAILURE(run("out/palindrome.bexe"));
    EXPECT_EQ(reg(10), 1u); // "aacbcaa" is a palindrome
    EXPECT_EQ(reg(11), 0u); // "abda" is not

    ASSERT_NO_FATAL_FAILURE(build("-o out/fibonacci programs/src/fibonacci.basm -outdir out"));
    ASSERT_NO_FATAL_FAILURE(run("out/fibonacci.bexe"));
    EXPECT_EQ(reg(1), 3u); // fib(4)

    ASSERT_NO_FATAL_FAILURE(build("-I programs/include -o out/showcase "
                                  "programs/src/showcase_preprocessor.basm -outdir out"));
    ASSERT_NO_FATAL_FAILURE(run("out/showcase.bexe"));
    EXPECT_EQ(reg(0), 22u); // $12 + 4
    EXPECT_EQ(reg(1), 5u);
}

// ---------------------------------------------------------------------------------------------
// Errors
// ---------------------------------------------------------------------------------------------

TEST_F(AssemblerIntegration, immediate_out_of_range_fails)
{
    write_file("big.basm", R"(.global _start

.text
_start:
                add     x0, xzr, 16384          ; immediates are unsigned 14 bit
                hlt
)");
    EXPECT_NE(basm("-o big big.basm -outdir ."), 0) << log_tail("basm.log");
    EXPECT_FALSE(exists("big.bexe"));
}

TEST_F(AssemblerIntegration, data_directive_outside_data_fails)
{
    write_file("misplaced.basm", R"(.global _start

.text
_start:
                hlt
                .word 5
)");
    EXPECT_NE(basm("-o misplaced misplaced.basm -outdir ."), 0) << log_tail("basm.log");
    EXPECT_FALSE(exists("misplaced.bexe"));
}

TEST_F(AssemblerIntegration, undefined_symbol_fails_to_link)
{
    write_file("undef.basm", R"(.global _start

.text
_start:
                bl      nowhere
                hlt
)");
    EXPECT_NE(basm("-o undef undef.basm -outdir ."), 0) << log_tail("basm.log");
    EXPECT_FALSE(exists("undef.bexe"));
}
