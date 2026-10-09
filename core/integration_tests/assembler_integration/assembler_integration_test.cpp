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
                .align 4                        ; .data is 14 bytes, str needs a multiple of 4
result:         .advance 4
)");
    ASSERT_NO_FATAL_FAILURE(build("-o data data.basm -outdir ."));

    ObjectFile obj(File(path("data.bo")));
    EXPECT_TRUE(has_relocation(obj, "values", RelType::R_EMU32_ADRP_HI20));
    EXPECT_TRUE(has_relocation(obj, "values", RelType::R_EMU32_O_LO12));
    EXPECT_TRUE(has_relocation(obj, "result", RelType::R_EMU32_ADRP_HI20));
    EXPECT_EQ(obj.data_section.size(), 8u + 6u);
    EXPECT_EQ(obj.bss_section, 4u);

    // .data is 14 bytes at 0x1000 and .bss follows directly, aligned to 4: result is at 0x1010.
    ASSERT_NO_FATAL_FAILURE(run("data.bexe", "-m 0x1000:14,0x1010:4"));
    EXPECT_EQ(mem(kDataStart),
              (std::vector<byte>{0xe8, 0x03, 0, 0, 0xea, 0, 0, 0, 'h', 'e', 'l', 'l', 'o', 0}));
    EXPECT_EQ(mem(0x1010), (std::vector<byte>{0xd2, 0x04, 0, 0})); // 1234
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

// ldr and str need an address that is a multiple of the size of the access, ldur and stur do not.
TEST_F(AssemblerIntegration, ldr_faults_on_a_misaligned_address_and_ldur_does_not)
{
    write_file("unaligned.basm", R"(.global _start

.text
_start:
                adrp    x0, bytes
                add     x0, x0, :lo12:bytes
                ldur    x1, [x0, 1]             ; 0x05040302
                ldurh   x2, [x0, 3]             ; 0x0504
                ldursh  x3, [x0, 5]             ; 0x80ff
                mov     x4, 0x1234
                sturh   x4, [x0, 7]
                stur    x1, [x0, 9]
                ldr     x5, [x0, 1]             ; faults: bytes + 1 is not a multiple of 4
                hlt

.data
bytes:          .byte 1, 2, 3, 4, 5, 0xff, 0x80, 0, 0, 0, 0, 0, 0, 0
)");
    ASSERT_NO_FATAL_FAILURE(build("-o unaligned unaligned.basm -outdir ."));

    EXPECT_EQ(emu32("-e unaligned.bexe -l 100 -m 0x1000:14"),
              S32(Emulator32bit::EmuCLIExitCode::EXIT_FAULT))
        << log_tail("emu32.log");
    EXPECT_EQ(state("status"), "fault");
    EXPECT_NE(state("message").find("Misaligned load of 4 bytes at address 0x00001001"),
              std::string::npos)
        << state("message");
    EXPECT_EQ(state_number("instructions"), 8u);
    EXPECT_EQ(state_number("pc"), 32u); // the faulting ldr
    EXPECT_EQ(reg(1), 0x05040302u);
    EXPECT_EQ(reg(2), 0x0504u);
    EXPECT_EQ(reg(3), 0xFFFF80FFu);
    EXPECT_EQ(reg(5), 0u) << "the load that faulted wrote nothing";
    EXPECT_EQ(mem(kDataStart),
              (std::vector<byte>{1, 2, 3, 4, 5, 0xff, 0x80, 0x34, 0x12, 2, 3, 4, 5, 0}));
}

// The code of a program cannot be written by the program.
TEST_F(AssemblerIntegration, emu32_faults_on_a_store_to_the_code)
{
    write_file("selfmod.basm", R"(.global _start

.text
_start:
                adrp    x1, _start
                add     x1, x1, :lo12:_start
                str     x1, [x1]                ; over the first instruction
                hlt
)");
    ASSERT_NO_FATAL_FAILURE(build("-o selfmod selfmod.basm -outdir ."));

    EXPECT_EQ(emu32("-e selfmod.bexe -l 100"), S32(Emulator32bit::EmuCLIExitCode::EXIT_FAULT))
        << log_tail("emu32.log");
    EXPECT_EQ(state("status"), "fault");
    EXPECT_NE(state("message").find("read-only"), std::string::npos) << state("message");
}

// A program that uses more pages than there is RAM has its pages swapped out to the disk, it does
// not run into the ROM or the end of the memory.
TEST_F(AssemblerIntegration, emu32_swaps_pages_when_the_ram_is_full)
{
    write_file("pages.basm", R"(.global _start

.bss
buffer:         .advance 40960                  ; ten pages

.text
_start:
                adrp    x1, buffer
                add     x1, x1, :lo12:buffer
                add     x3, xzr, 0
loop:
                add     x3, x3, 1
                str     x3, [x1]
                add     x1, x1, 4096
                cmp     x3, 10
                b.lt    loop
                hlt
)");
    ASSERT_NO_FATAL_FAILURE(build("-o pages pages.basm -outdir ."));

    // Four pages of RAM hold the code, the data and two of the pages of the buffer.
    ASSERT_NO_FATAL_FAILURE(run("pages.bexe", "--ram-pages 4 --rom-start 4 --rom-pages 4"));
    EXPECT_EQ(reg(3), 10u);
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
                mov     x0, 0xff0                ; 0x00000ff0
                and     x1, x0, 0x0f0            ; 0x0f0
                orr     x2, x0, 0x00f            ; 0xfff
                eor     x3, x0, 0x0ff            ; 0xf0f
                bic     x4, x0, 0xf00            ; 0x0f0
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

// 0x hex, 0b binary and 0o octal literals in instructions and data.
TEST_F(AssemblerIntegration, number_literals)
{
    write_file("lit.basm", R"(.global _start

.text
_start:
                add     x0, xzr, 0x2A
                add     x1, xzr, 0b101010
                add     x2, xzr, 0o52
                add     x3, xzr, 42
                adrp    x4, table
                add     x4, x4, :lo12:table
                ldrb    x5, [x4]
                ldrb    x6, [x4, 1]
                ldrb    x7, [x4, 2]
                hlt
.data
table:          .byte 0x2A, 0b101010, 0o52
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

                add     x1, xzr, 0x11
                str     x1, [x0], 4             ; buf[0], then x0 += 4
                add     x1, xzr, 0x22
                str     x1, [x0, 4]!            ; x0 += 4, then buf[2]
                add     x1, xzr, 0x33
                add     x2, xzr, 3
                str     x1, [x9, x2, lsl 2]     ; buf[3]
                add     x1, xzr, 0x44
                str     x1, [x9, 4]             ; buf[1]

                add     x1, xzr, 0x1234
                strh    x1, [x9, 16]
                add     x1, xzr, 0xab
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
                add     x1, xzr, 0x1234
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
                add     x1, xzr, 0x11
                str     x1, [x9, -4]            ; buf[3]
                add     x1, xzr, 0x22
                str     x1, [x9, -8]!           ; x9 -= 8, then buf[2]
                add     x1, xzr, 0x33
                str     x1, [x9], -4            ; buf[2] = 0x33, then x9 -= 4
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
                ldur    x1, [x0]                ; words is at offset 6, not a multiple of 4
                adrp    x2, tail
                add     x2, x2, :lo12:tail
                ldrb    x3, [x2]
                hlt

.data
bytes:          .byte 1, 2, 3
                .align 4
hwords:         .dbyte 0x1234
words:          .word 0xdeadbeef
dwords:         .dword 0x0102030405060708
chars:          .byte 'h', 'i'
text:           .ascii "ab"
ztext:          .asciz "cd"
                .advance 2
                .org 32
tail:           .byte 0x7f
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
    EXPECT_TRUE(exe.rel_text.empty()) << "the linker resolves every relocation";

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

TEST_F(AssemblerIntegration, preprocessor_macro_labels_are_scoped_per_invocation)
{
    // The macro has a loop with a label. Invoked twice, each expansion branches to its own label.
    write_file("macro_loop.basm", R"(#macro add_n_times(dst, amount, count)
                add     x9, xzr, count
loop:
                add     dst, dst, amount
                subs    x9, x9, 1
                b.ne    loop
#macend

.global _start

.text
_start:
                add     x0, xzr, 0
                add     x1, xzr, 0
                #invoke add_n_times(x0, 3, 4)
                #invoke add_n_times(x1, 5, 2)
                hlt
)");
    ASSERT_NO_FATAL_FAILURE(build("-o macro_loop macro_loop.basm -outdir ."));
    ASSERT_NO_FATAL_FAILURE(run("macro_loop.bexe"));
    EXPECT_EQ(reg(0), 12u);
    EXPECT_EQ(reg(1), 10u);
}

TEST_F(AssemblerIntegration, preprocessor_error_reports_the_source_location)
{
    write_file("bad.basm", ".global _start\n.text\n_start:\n                #invoke missing()\n");
    EXPECT_NE(basm("-o bad bad.basm -outdir ."), 0);
    EXPECT_FALSE(exists("bad.bexe"));

    // file:line:column of the offending name, the message, the source line and a caret.
    const std::string log = log_tail("basm.log");
    EXPECT_NE(log.find("bad.basm:4:25: error: no macro 'missing' is defined with 0 argument(s)"),
              std::string::npos)
        << log;
}

TEST_F(AssemblerIntegration, assembler_error_reports_the_source_location)
{
    write_file("bad.basm", ".global _start\n.text\n_start:\n                add     x0, xzr\n");
    EXPECT_NE(basm("-o bad bad.basm -outdir ."), 0);
    EXPECT_FALSE(exists("bad.bexe"));

    // The assembler gets the tokens from the preprocessor, so the location is in the original
    // source, with its indentation.
    const std::string log = log_tail("basm.log");
    EXPECT_NE(log.find("bad.basm:4:32: error: expected ',', got end of line"), std::string::npos)
        << log;
    EXPECT_NE(log.find("                add     x0, xzr"), std::string::npos) << log;
    EXPECT_EQ(log.find("bad.bi:"), std::string::npos) << log;
}

TEST_F(AssemblerIntegration, assembler_error_in_a_macro_names_the_expansion)
{
    write_file("bad.basm", ".global _start\n"
                           "#macro twice(r)\n"
                           "    add r, r\n"
                           "#macend\n"
                           ".text\n"
                           "_start:\n"
                           "    #invoke twice(x1)\n");
    EXPECT_NE(basm("-o bad bad.basm -outdir ."), 0);

    // The error is in the macro body, and the note says where the macro was used.
    const std::string log = log_tail("basm.log");
    EXPECT_NE(log.find("bad.basm:3:"), std::string::npos) << log;
    EXPECT_NE(log.find("bad.basm:7:5: note: in expansion of macro 'twice'"), std::string::npos)
        << log;
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
    EXPECT_EQ(reg(0), 22u); // 0x12 + 4
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

// A linker script (-ld) can place .text away from address 0. Branches and the adrp/lo12 pair of
// the data access have to be resolved against the moved sections.
TEST_F(AssemblerIntegration, linker_script_moves_text)
{
    write_file("moved.basm", R"(.global _start

.text
_start:
                adrp    x0, value
                add     x0, x0, :lo12:value
                ldr     x1, [x0]
                bl      bump
                b       done
                add     x1, x1, 100             ; skipped
done:
                hlt

bump:
                add     x1, x1, 1
                ret

.data
value:          .word 41
)");
    write_file("moved.ld", R"(ENTRY(_start)

SECTIONS (
    .text = 0x400;
    .data = 0x2000;
    .bss;
)
)");
    ASSERT_NO_FATAL_FAILURE(build("-o moved moved.basm -outdir . -ld moved.ld"));

    ObjectFile exe(File(path("moved.bexe")));
    EXPECT_EQ(exe.sections[exe.section_table.at(".text")].address, 0x400u);
    EXPECT_EQ(exe.sections[exe.section_table.at(".data")].address, 0x2000u);
    EXPECT_EQ(symbol(exe, "_start")->symbol_value, 0x400u);

    ASSERT_NO_FATAL_FAILURE(run("moved.bexe"));
    EXPECT_EQ(reg(1), 42u);
    EXPECT_EQ(state_number("pc"), 0x400u + 6 * 4); // the hlt
}

// ENTRY(symbol) starts the program at that symbol instead of _start.
TEST_F(AssemblerIntegration, linker_script_entry_symbol)
{
    write_file("entry.basm", R"(.global main

.text
unused:
                add     x0, xzr, 99
                hlt
main:
                add     x0, xzr, 7
                hlt
)");
    write_file("entry.ld", R"(ENTRY(main)

SECTIONS (
    .text = 0x0;
    .data = 0x1000;
    .bss;
)
)");
    ASSERT_NO_FATAL_FAILURE(build("-o entry entry.basm -outdir . -ld entry.ld"));
    ASSERT_NO_FATAL_FAILURE(run("entry.bexe"));
    EXPECT_EQ(reg(0), 7u);
    EXPECT_EQ(state_number("pc"), 12u);
}

TEST_F(AssemblerIntegration, linker_script_undefined_entry_fails)
{
    write_file("noentry.basm", R"(.global _start

.text
_start:
                hlt
)");
    write_file("noentry.ld",
               "ENTRY(nothing)\nSECTIONS (\n .text = 0x0;\n .data = 0x1000;\n .bss;\n)\n");
    EXPECT_NE(basm("-o noentry noentry.basm -outdir . -ld noentry.ld"), 0);
    EXPECT_NE(log_tail("basm.log").find("nothing"), std::string::npos) << log_tail("basm.log");
    EXPECT_FALSE(exists("noentry.bexe"));
}

TEST_F(AssemblerIntegration, undefined_symbol_error_names_the_symbol)
{
    write_file("undef2.basm", R"(.global _start

.text
_start:
                bl      missing_function
                hlt
)");
    EXPECT_NE(basm("-o undef2 undef2.basm -outdir ."), 0);
    EXPECT_NE(log_tail("basm.log").find("missing_function"), std::string::npos)
        << log_tail("basm.log");
}

// Garbage and truncated executables are rejected with a message instead of being loaded.
TEST_F(AssemblerIntegration, loading_a_file_that_is_not_an_executable_fails)
{
    write_file("garbage.bexe", std::string(200, 'x'));
    EXPECT_NE(emu32("-e garbage.bexe"), 0);
    EXPECT_NE(log_tail("emu32.log").find("not an object file"), std::string::npos)
        << log_tail("emu32.log");
}

TEST_F(AssemblerIntegration, loading_a_truncated_executable_fails)
{
    write_file("whole.basm", R"(.global _start

.text
_start:
                add     x0, xzr, 1
                hlt
)");
    ASSERT_NO_FATAL_FAILURE(build("-o whole whole.basm -outdir ."));

    std::ifstream in(m_dir / "whole.bexe", std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    const std::string bytes = ss.str();
    ASSERT_GT(bytes.size(), 64u);
    write_file("cut.bexe", bytes.substr(0, bytes.size() - 20));

    EXPECT_NE(emu32("-e cut.bexe"), 0);
    EXPECT_EQ(m_state.count("status"), 0u);
}

static const char *const kLoopProgram = R"(.global _start

.text
_start:
                add     x0, xzr, 0
loop:
                add     x0, x0, 1
                cmp     x0, 3
                b.ne    loop
                hlt
)";

static size_t count_of(const std::string &text, const std::string &needle)
{
    size_t n = 0;
    for (size_t at = text.find(needle); at != std::string::npos; at = text.find(needle, at + 1))
    {
        ++n;
    }
    return n;
}

// Building or loading does not dump object files unless asked to.
TEST_F(AssemblerIntegration, object_files_are_not_dumped_by_default)
{
    write_file("loop.basm", kLoopProgram);
    ASSERT_NO_FATAL_FAILURE(build("-o loop loop.basm -outdir ."));
    EXPECT_EQ(count_of(log_tail("basm.log", 1 << 20), "SYMBOL TABLE"), 0u);

    ASSERT_NO_FATAL_FAILURE(run("loop.bexe"));
    EXPECT_EQ(count_of(log_tail("emu32.log", 1 << 20), "SYMBOL TABLE"), 0u);
    EXPECT_EQ(reg(0), 3u);
}

// -dump lists the symbols and disassembly of the object file and the executable, and names the
// real target of a branch.
TEST_F(AssemblerIntegration, dump_lists_object_file_and_executable)
{
    write_file("loop.basm", kLoopProgram);
    ASSERT_NO_FATAL_FAILURE(build("-dump -o loop loop.basm -outdir ."));

    const std::string log = log_tail("basm.log", 1 << 20);
    EXPECT_EQ(count_of(log, "SYMBOL TABLE"), 2u) << log;
    EXPECT_EQ(count_of(log, "Cannot print object file"), 0u) << log;
    EXPECT_NE(log.find("loop.bo:"), std::string::npos) << log;
    EXPECT_NE(log.find("loop.bexe:"), std::string::npos) << log;
    EXPECT_NE(log.find("b.ne"), std::string::npos) << log;
    // The branch goes back to `loop`, so the annotation is the label with no offset.
    EXPECT_NE(log.find("<loop>\n"), std::string::npos) << log;
}

// -D defines a symbol for the preprocessor, with or without a value.
TEST_F(AssemblerIntegration, command_line_defines)
{
    write_file("def.basm", R"(.global _start

.text
_start:
                mov     x0, VALUE
#ifdef EXTRA
                add     x0, x0, 100
#endif
                hlt
)");
    ASSERT_NO_FATAL_FAILURE(build("-D VALUE=9 -o plain def.basm -outdir ."));
    ASSERT_NO_FATAL_FAILURE(run("plain.bexe"));
    EXPECT_EQ(reg(0), 9u);

    ASSERT_NO_FATAL_FAILURE(build("-D VALUE=9 -D EXTRA -o extra def.basm -outdir ."));
    ASSERT_NO_FATAL_FAILURE(run("extra.bexe"));
    EXPECT_EQ(reg(0), 109u);

    EXPECT_NE(basm("-o none def.basm -outdir ."), 0) << "VALUE is not defined, so it is a symbol";
}

// A symbol that is declared but not defined anywhere is an error at link time, it used to be
// linked at address 0.
TEST_F(AssemblerIntegration, linking_an_undefined_symbol_fails)
{
    write_file("undef.basm", R"(.global _start
.global missing

.text
_start:
                bl      missing
                hlt
)");
    EXPECT_NE(basm("-o undef undef.basm -outdir ."), 0);
    EXPECT_NE(log_tail("basm.log").find("undefined reference to 'missing'"), std::string::npos)
        << log_tail("basm.log");
    EXPECT_FALSE(exists("undef.bexe"));
}

// Code that is bigger than the 4 KiB before the data used to be written over by the data.
TEST_F(AssemblerIntegration, big_programs_do_not_run_into_their_data)
{
    std::string source = ".global _start\n.data\nvalue: .word 0x12345678\n.text\n_start:\n";
    for (int i = 0; i < 1100; i++) source += "                nop\n";
    source += "                adrp    x1, value\n"
              "                add     x1, x1, :lo12:value\n"
              "                ldr     x0, [x1]\n"
              "                hlt\n";
    write_file("big.basm", source);
    ASSERT_NO_FATAL_FAILURE(build("-o big big.basm -outdir ."));

    ASSERT_NO_FATAL_FAILURE(run("big.bexe"));
    EXPECT_EQ(reg(0), 0x12345678u);
    EXPECT_EQ(state_number("instructions"), 1103u);
}

// A table of function addresses in .data, filled in by the linker, and the functions called through it.
TEST_F(AssemblerIntegration, a_table_of_addresses_in_data)
{
    write_file("table.basm", R"(.global _start

.data
table:          .word first, second

.text
_start:
                adrp    x1, table
                add     x1, x1, :lo12:table
                ldr     x2, [x1]
                ldr     x3, [x1, 4]
                blx     x2
                add     x4, x0, 0
                blx     x3
                hlt

first:
                mov     x0, 5
                ret
second:
                mov     x0, 9
                ret
)");
    ASSERT_NO_FATAL_FAILURE(build("-o table table.basm -outdir ."));

    ASSERT_NO_FATAL_FAILURE(run("table.bexe"));
    EXPECT_EQ(reg(4), 5u) << "called through the first word";
    EXPECT_EQ(reg(0), 9u) << "called through the second word";
    EXPECT_EQ(reg(2), 8 * 4u) << "first is after the 8 instructions of _start";
    EXPECT_EQ(reg(3), 10 * 4u) << "and second after the 2 instructions of first";
}

// The data of the files of a program is joined in a way that keeps what .align asked for.
TEST_F(AssemblerIntegration, align_is_kept_when_files_are_linked)
{
    write_file("main.basm", R"(.global _start
.global wide

.data
odd:            .byte 1, 2, 3

.text
_start:
                adrp    x1, wide
                add     x1, x1, :lo12:wide
                ldr     x0, [x1]
                hlt
)");
    write_file("wide.basm", R"(.global wide

.data
                .align 8
wide:           .word 0x01020304
)");
    ASSERT_NO_FATAL_FAILURE(build("-o aligned main.basm wide.basm -outdir ."));

    ASSERT_NO_FATAL_FAILURE(run("aligned.bexe"));
    EXPECT_EQ(reg(0), 0x01020304u);
    EXPECT_EQ(reg(1) % 8, 0u);
}

// A branch offset written as a number is a signed distance in bytes from the branch itself, and
// an expression can compute it.
TEST_F(AssemblerIntegration, a_numeric_branch_offset_can_go_backwards)
{
    write_file("loop.basm", R"(.global _start

.text
_start:
                mov     x0, 0
                mov     x1, 5
                add     x0, x0, 2
                subs    x1, x1, 1
                b.ne    -2 * 4
                hlt
)");
    ASSERT_NO_FATAL_FAILURE(build("-o loop loop.basm -outdir ."));
    ASSERT_NO_FATAL_FAILURE(run("loop.bexe"));
    EXPECT_EQ(reg(0), 10u);
}

// .equ names a number, and the distance between two labels of a section is one too, which gives
// the size of a table without counting it by hand.
TEST_F(AssemblerIntegration, the_size_of_a_table_is_the_distance_between_its_labels)
{
    write_file("table.basm", R"(.global _start

.data
table:          .word 10, 20, 30
table_end:
.equ            TABLE_BYTES, table_end - table
.equ            SHIFT, 2
size:           .word TABLE_BYTES

.text
_start:
                mov     x0, TABLE_BYTES >> SHIFT
                adrp    x2, size
                add     x2, x2, :lo12:size
                ldr     x1, [x2]
                hlt
)");
    ASSERT_NO_FATAL_FAILURE(build("-o table table.basm -outdir ."));
    ASSERT_NO_FATAL_FAILURE(run("table.bexe"));
    EXPECT_EQ(reg(0), 3u);
    EXPECT_EQ(reg(1), 12u);
}

// `symbol + number` is a relocation with an addend, also for a symbol of another file: in a
// data word, in adrp and :lo12:, and as a branch target.
TEST_F(AssemblerIntegration, a_symbol_plus_a_number_is_resolved_by_the_linker)
{
    write_file("main.basm", R"(.global _start

.data
ptr:            .word table + 8

.text
_start:
                adrp    x1, ptr
                add     x1, x1, :lo12:ptr
                ldr     x2, [x1]
                ldr     x0, [x2]
                adrp    x3, table + 4
                add     x3, x3, :lo12:table + 4
                ldr     x4, [x3]
                bl      func + 4
                b       over + 4
over:           mov     x5, 1
                mov     x6, 2
                hlt
)");
    write_file("lib.basm", R"(.global table
.global func

.data
table:          .word 10, 20, 30

.text
func:           mov     x7, 1
                add     x7, x7, 5
                ret
)");
    ASSERT_NO_FATAL_FAILURE(build("-o prog main.basm lib.basm -outdir ."));
    ASSERT_NO_FATAL_FAILURE(run("prog.bexe"));
    EXPECT_EQ(reg(0), 30u) << "the word holds the address of table + 8";
    EXPECT_EQ(reg(4), 20u) << "adrp and :lo12: with table + 4";
    EXPECT_EQ(reg(7), 5u) << "bl func + 4 skips the first instruction";
    EXPECT_EQ(reg(5), 0u) << "b over + 4 skips the first instruction";
    EXPECT_EQ(reg(6), 2u);
}

TEST_F(AssemblerIntegration, preprocessor_compares_numbers_by_value)
{
    write_file("version.basm", R"(.global _start

.text
_start:
#ifless VERSION 10
                mov     x0, 1
#else
                mov     x0, 2
#endif
                hlt
)");
    ASSERT_NO_FATAL_FAILURE(build("-D VERSION=9 -o nine version.basm -outdir ."));
    ASSERT_NO_FATAL_FAILURE(run("nine.bexe"));
    EXPECT_EQ(reg(0), 1u);

    ASSERT_NO_FATAL_FAILURE(build("-D VERSION=11 -o eleven version.basm -outdir ."));
    ASSERT_NO_FATAL_FAILURE(run("eleven.bexe"));
    EXPECT_EQ(reg(0), 2u);
}

// Only the linker resolves addresses, so an object file is not a program.
TEST_F(AssemblerIntegration, loading_an_object_file_with_relocations_fails)
{
    write_file("data.basm", R"(.global _start

.text
_start:
                adrp    x0, value
                add     x0, x0, :lo12:value
                hlt

.data
value:          .word 5
)");
    ASSERT_NO_FATAL_FAILURE(build("-c -o data data.basm -outdir ."));
    ASSERT_TRUE(exists("data.bo"));

    EXPECT_NE(emu32("-e data.bo"), 0);
    EXPECT_EQ(m_state.count("status"), 0u);
    EXPECT_NE(log_tail("emu32.log").find("still has relocations"), std::string::npos)
        << log_tail("emu32.log");
}

TEST_F(AssemblerIntegration, help_and_version_exit_cleanly_without_building)
{
    EXPECT_EQ(basm("--help"), 0) << log_tail("basm.log");
    EXPECT_NE(log_tail("basm.log").find("basm [options] file..."), std::string::npos);

    EXPECT_EQ(basm("--version"), 0) << log_tail("basm.log");
    EXPECT_NE(log_tail("basm.log").find("Assembler Version"), std::string::npos);
}

TEST_F(AssemblerIntegration, basm_fails_with_a_message_instead_of_crashing)
{
    write_file("bad.basm", ".text\n  bogus x0\n");
    EXPECT_EQ(basm("-o bad bad.basm -outdir ."), 1) << log_tail("basm.log");
    EXPECT_NE(log_tail("basm.log").find("bad.basm:2:"), std::string::npos) << log_tail("basm.log");
    EXPECT_FALSE(exists("bad.bexe"));

    EXPECT_EQ(basm("--no-such-flag"), 1);
    EXPECT_NE(log_tail("basm.log").find("Invalid flag"), std::string::npos);
}

TEST_F(AssemblerIntegration, dump_with_compile_only_lists_just_the_object_file)
{
    write_file("loop.basm", kLoopProgram);
    ASSERT_NO_FATAL_FAILURE(build("-c -dump -o loop loop.basm -outdir ."));
    EXPECT_EQ(count_of(log_tail("basm.log", 1 << 20), "SYMBOL TABLE"), 1u);
}

constexpr const char *kDoubleProgram = R"(.global _start
.global double

.text
_start:
                add     x0, xzr, 5
                bl      double
                bl      double
                hlt

double:
                add     x0, x0, x0
                ret
)";

// --break takes a symbol of the executable, stops before the instruction and exits with 4.
TEST_F(AssemblerIntegration, emu32_breakpoint_on_a_symbol)
{
    write_file("call.basm", kDoubleProgram);
    ASSERT_NO_FATAL_FAILURE(build("-o call call.basm -outdir ."));
    ObjectFile exe(File(path("call.bexe")));
    const ObjectFile::SymbolTableEntry *double_fn = symbol(exe, "double");
    ASSERT_NE(double_fn, nullptr);

    EXPECT_EQ(emu32("-e call.bexe -l 100 --break double"),
              S32(Emulator32bit::EmuCLIExitCode::EXIT_BREAKPOINT))
        << log_tail("emu32.log");
    EXPECT_EQ(state("status"), "breakpoint");
    EXPECT_EQ(state_number("pc"), double_fn->symbol_value);
    EXPECT_EQ(reg(0), 5u); // the first call to double did not run yet
    EXPECT_EQ(state_number("instructions"), 2u);

    EXPECT_EQ(emu32("-e call.bexe -l 100 --break nowhere"),
              S32(Emulator32bit::EmuCLIExitCode::EXIT_USAGE_ERROR));
}

TEST_F(AssemblerIntegration, emu32_trace_shows_symbols_and_changes)
{
    write_file("call.basm", kDoubleProgram);
    ASSERT_NO_FATAL_FAILURE(build("-o call call.basm -outdir ."));
    ASSERT_NO_FATAL_FAILURE(run("call.bexe", "--trace trace.txt"));

    std::ifstream in(m_dir / "trace.txt");
    std::stringstream ss;
    ss << in.rdbuf();
    const std::string trace = ss.str();
    EXPECT_NE(trace.find("<_start>"), std::string::npos) << trace;
    EXPECT_NE(trace.find("<double>"), std::string::npos) << trace;
    EXPECT_NE(trace.find("x0=0x5->0xa"), std::string::npos) << trace;
    EXPECT_NE(trace.find("x0=0xa->0x14"), std::string::npos) << trace;
    EXPECT_NE(trace.find("; halt"), std::string::npos) << trace;
}

// --history keeps the instructions that led to a fault, the faulting one last.
TEST_F(AssemblerIntegration, emu32_history_ends_with_the_faulting_instruction)
{
    write_file("fault.basm", R"(.global _start

.text
_start:
                add     x0, xzr, 1
                add     x1, xzr, 0
                str     x0, [x1]            ; the code is not writable
                hlt
)");
    ASSERT_NO_FATAL_FAILURE(build("-o fault fault.basm -outdir ."));

    EXPECT_EQ(emu32("-e fault.bexe -l 100 --history 4"),
              S32(Emulator32bit::EmuCLIExitCode::EXIT_FAULT))
        << log_tail("emu32.log");
    EXPECT_EQ(state("status"), "fault");
    EXPECT_NE(state("history[2]").find("str"), std::string::npos) << state("history[2]");
    EXPECT_TRUE(m_state.find("history[3]") == m_state.end());
}

// A vector table of 7 entries of 4 instructions (class 0 is unused), for the programs below.
// Each entry is a branch to a handler of the program, the others are no-ops.
constexpr const char *kVectorTable = R"(
.align 16
vectors:
                hlt
                nop
                nop
                nop
                b       undefined_handler           ; 1: undefined instruction
                nop
                nop
                nop
                b       svc_handler                 ; 2: supervisor call
                nop
                nop
                nop
                hlt                                 ; 3: instruction abort
                nop
                nop
                nop
                hlt                                 ; 4: data abort
                nop
                nop
                nop
                hlt                                 ; 5: breakpoint
                nop
                nop
                nop
)";

// A kernel that installs a vector table and takes system calls: `swi n` goes to the handler, which
// counts it and returns with eret.
TEST_F(AssemblerIntegration, supervisor_calls_go_through_the_vector_table)
{
    write_file("kernel.basm", std::string(R"(.global _start
.text
_start:
                adrp    x0, vectors
                add     x0, x0, :lo12:vectors
                msr     vbar, x0
                swi     7
                swi     9
                hlt

svc_handler:
                add     x10, x10, 1             ; count the calls
                mrs     x11, esr
                mrs     x12, elr
                eret

undefined_handler:
                hlt
)") + kVectorTable);
    ASSERT_NO_FATAL_FAILURE(build("-o kernel kernel.basm -outdir ."));
    ASSERT_NO_FATAL_FAILURE(run("kernel.bexe", "--trace trace.txt"));

    EXPECT_EQ(reg(10), 2u);
    EXPECT_EQ(reg(11), (2u << 26) | 9u) << "class 2, the number of the last call";
    EXPECT_EQ(reg(12), 0x14u) << "the instruction after the second swi (the hlt)";
    EXPECT_EQ(state("mode"), "kernel");
    EXPECT_EQ(state_number("esr"), reg(11));
    EXPECT_NE(state_number("vbar"), 0u);

    std::ifstream in(m_dir / "trace.txt");
    std::stringstream ss;
    ss << in.rdbuf();
    EXPECT_NE(ss.str().find("-- exception: supervisor call"), std::string::npos) << ss.str();
}

// eret into user mode, where hlt is not allowed: the handler sees why.
TEST_F(AssemblerIntegration, hlt_in_user_mode_is_an_undefined_instruction)
{
    write_file("user.basm", std::string(R"(.global _start
.global user
.text
_start:
                adrp    x0, vectors
                add     x0, x0, :lo12:vectors
                msr     vbar, x0
                adrp    x1, user
                add     x1, x1, :lo12:user
                msr     elr, x1
                msr     spsr, 16                ; user mode, IRQs not masked
                eret

user:
                add     x4, xzr, 5
                hlt                             ; privileged

svc_handler:
                hlt

undefined_handler:
                mrs     x5, esr
                mrs     x6, elr
                mrs     x7, spsr
                hlt
)") + kVectorTable);
    ASSERT_NO_FATAL_FAILURE(build("-o user user.basm -outdir ."));
    ObjectFile exe(File(path("user.bexe")));
    const ObjectFile::SymbolTableEntry *user = symbol(exe, "user");
    ASSERT_NE(user, nullptr);

    ASSERT_NO_FATAL_FAILURE(run("user.bexe"));
    EXPECT_EQ(reg(4), 5u) << "the user code ran";
    EXPECT_EQ(reg(5), (1u << 26) | 2u) << "undefined instruction, privileged";
    EXPECT_EQ(reg(6), user->symbol_value + 4);
    EXPECT_EQ(reg(7), 16u) << "it came from user mode";
    EXPECT_EQ(state("mode"), "kernel");
}

// udiv and sdiv, and the remainder a compiler builds from them (docs/abi.md#division).
TEST_F(AssemblerIntegration, division_and_remainder)
{
    write_file("div.basm", R"(.global _start
.text
_start:
                mov     x0, 100
                mov     x1, 7
                udiv    x2, x0, x1              ; 14
                udiv    x3, x0, 9               ; 11, an immediate divisor
                udiv    x4, x0, xzr             ; a division by zero is 0

                mvn     x5, 6                   ; -7
                mov     x6, 2
                sdiv    x7, x5, x6              ; -3, rounded toward zero
                mul     x8, x7, x6              ; -6
                sub     x9, x5, x8              ; -7 % 2 = -1

                sdivs   x10, x5, x6             ; the same, setting the flags
                hlt
)");
    ASSERT_NO_FATAL_FAILURE(build("-o div div.basm -outdir ."));
    ASSERT_NO_FATAL_FAILURE(run("div.bexe"));

    EXPECT_EQ(reg(2), 14u);
    EXPECT_EQ(reg(3), 11u);
    EXPECT_EQ(reg(4), 0u);
    EXPECT_EQ(reg(7), 0xFFFFFFFDu);
    EXPECT_EQ(reg(9), 0xFFFFFFFFu);
    EXPECT_EQ(reg(10), 0xFFFFFFFDu);
    EXPECT_TRUE(flag("N"));
    EXPECT_FALSE(flag("Z"));
}

TEST_F(AssemblerIntegration, emulator_calls_can_be_turned_off)
{
    write_file("call.basm", R"(.global _start
.text
_start:
                add     x8, xzr, 1003
                swi     1
                hlt
)");
    ASSERT_NO_FATAL_FAILURE(build("-o call call.basm -outdir ."));
    ASSERT_NO_FATAL_FAILURE(run("call.bexe"));

    EXPECT_EQ(emu32("-e call.bexe -l 100 --no-semihosting"),
              S32(Emulator32bit::EmuCLIExitCode::EXIT_FAULT))
        << log_tail("emu32.log");
    EXPECT_NE(state("message").find("emulator calls are off"), std::string::npos)
        << state("message");
}

// .rodata is read only, and the linker gives it a page of its own between the code and the data.
TEST_F(AssemblerIntegration, rodata_is_read_only)
{
    write_file("ro.basm", R"(.global _start
.text
_start:
                adrp    x0, table
                add     x0, x0, :lo12:table
                ldr     x1, [x0]                ; the address of func
                ldr     x2, [x0, 4]
                ldrb    x3, [x0, 8]             ; 'h'
                blx     x1
                adrp    x5, counter
                add     x5, x5, :lo12:counter
                ldr     x6, [x5]
                hlt
func:
                mov     x4, 77
                ret

.rodata
table:          .word func, 0x1234
                .asciz "hi"
.data
counter:        .word 9
)");
    ASSERT_NO_FATAL_FAILURE(build("-o ro ro.basm -outdir ."));

    ObjectFile obj(File(path("ro.bo")));
    EXPECT_EQ(obj.rodata_section.size(), 8u + 3u);
    ASSERT_EQ(obj.rel_rodata.size(), 1u) << ".word func is an address";
    EXPECT_EQ(obj.rel_rodata[0].offset, 0u);
    EXPECT_TRUE(obj.rel_data.empty());

    ASSERT_NO_FATAL_FAILURE(run("ro.bexe"));
    EXPECT_EQ(reg(2), 0x1234u);
    EXPECT_EQ(reg(3), word('h'));
    EXPECT_EQ(reg(4), 77u);
    EXPECT_EQ(reg(6), 9u);

    ObjectFile exe(File(path("ro.bexe")));
    const word rodata = exe.sections[exe.section_table.at(".rodata")].address;
    const word data = exe.sections[exe.section_table.at(".data")].address;
    EXPECT_EQ(rodata, 0x1000u) << "the first page after the code";
    EXPECT_EQ(data, 0x2000u) << "not on the page of the read only data";
}

TEST_F(AssemblerIntegration, rodata_cannot_be_written)
{
    write_file("rowrite.basm", R"(.global _start
.text
_start:
                adrp    x1, constant
                add     x1, x1, :lo12:constant
                str     x1, [x1]
                hlt
.rodata
constant:       .word 5
)");
    ASSERT_NO_FATAL_FAILURE(build("-o rowrite rowrite.basm -outdir ."));

    EXPECT_EQ(emu32("-e rowrite.bexe -l 100"), S32(Emulator32bit::EmuCLIExitCode::EXIT_FAULT))
        << log_tail("emu32.log");
    EXPECT_NE(state("message").find("read-only"), std::string::npos) << state("message");
}

// A section that the linker script does not place would be loaded at address 0.
TEST_F(AssemblerIntegration, linker_script_must_place_sections_with_contents)
{
    write_file("noro.basm", R"(.global _start
.text
_start:         hlt
.rodata
value:          .word 1
)");
    write_file("noro.ld",
               "ENTRY(_start)\nSECTIONS (\n .text = 0x0;\n .data = 0x1000;\n .bss;\n)\n");
    EXPECT_NE(basm("-o noro noro.basm -ld noro.ld -outdir ."), 0);
    EXPECT_NE(log_tail("basm.log").find("does not place it"), std::string::npos)
        << log_tail("basm.log");

    write_file("ro.ld",
               "ENTRY(_start)\nSECTIONS (\n .text = 0x0;\n .rodata = 0x3000;\n .data = 0x1000;\n "
               ".bss;\n)\n");
    ASSERT_NO_FATAL_FAILURE(build("-o placed noro.basm -ld ro.ld -outdir ."));
    ObjectFile exe(File(path("placed.bexe")));
    EXPECT_EQ(exe.sections[exe.section_table.at(".rodata")].address, 0x3000u);
}

// `.weak`: a definition that another file replaces, and a reference that may stay undefined.
TEST_F(AssemblerIntegration, weak_definitions_and_references)
{
    write_file("main.basm", R"(.global _start
.weak greet
.weak hook
.text
_start:
                adrp    x0, hook                ; nothing defines it, so 0
                add     x0, x0, :lo12:hook
                bl      greet
                hlt
greet:          mov     x1, 1
                ret
)");
    write_file("override.basm", R"(.global greet
.text
greet:          mov     x1, 2
                ret
)");
    ASSERT_NO_FATAL_FAILURE(build("-o alone main.basm -outdir ."));
    ASSERT_NO_FATAL_FAILURE(run("alone.bexe"));
    EXPECT_EQ(reg(0), 0u) << "an undefined weak symbol is 0";
    EXPECT_EQ(reg(1), 1u) << "the weak definition is used when there is no other";

    ASSERT_NO_FATAL_FAILURE(build("-o strong_last main.basm override.basm -outdir ."));
    ASSERT_NO_FATAL_FAILURE(run("strong_last.bexe"));
    EXPECT_EQ(reg(1), 2u);

    ASSERT_NO_FATAL_FAILURE(build("-o strong_first override.basm main.basm -outdir ."));
    ASSERT_NO_FATAL_FAILURE(run("strong_first.bexe"));
    EXPECT_EQ(reg(1), 2u) << "the strong definition wins whatever the order";
}

TEST_F(AssemblerIntegration, strong_references_still_need_a_definition)
{
    write_file("strong.basm", R"(.global _start
.weak maybe
.text
_start:
                bl      maybe                   ; weak
                bl      missing                 ; not
                hlt
)");
    EXPECT_NE(basm("-o strong strong.basm -outdir ."), 0);
    const std::string log = log_tail("basm.log");
    EXPECT_NE(log.find("undefined reference to 'missing'"), std::string::npos) << log;
    EXPECT_EQ(log.find("undefined reference to 'maybe'"), std::string::npos) << log;
}

TEST_F(AssemblerIntegration, two_strong_definitions_still_conflict)
{
    write_file("one.basm", ".global _start\n.global f\n.text\n_start: hlt\nf: hlt\n");
    write_file("two.basm", ".global f\n.text\nf: hlt\n");
    EXPECT_NE(basm("-o dup one.basm two.basm -outdir ."), 0);
    EXPECT_NE(log_tail("basm.log").find("Multiple definition of symbol 'f'"), std::string::npos)
        << log_tail("basm.log");
}

// `.comm` reserves .bss space for a symbol that several files declare.
TEST_F(AssemblerIntegration, common_symbols_are_shared)
{
    write_file("a.basm", R"(.global _start
.comm counter, 4, 4
.text
_start:
                adrp    x1, counter
                add     x1, x1, :lo12:counter
                mov     x2, 5
                str     x2, [x1]
                bl      read_counter
                hlt
)");
    write_file("b.basm", R"(.global read_counter
.comm counter, 4, 4
.text
read_counter:
                adrp    x1, counter
                add     x1, x1, :lo12:counter
                ldr     x3, [x1]
                ret
)");
    ASSERT_NO_FATAL_FAILURE(build("-o common a.basm b.basm -outdir ."));
    ASSERT_NO_FATAL_FAILURE(run("common.bexe"));
    EXPECT_EQ(reg(3), 5u) << "both files use the same word";

    ObjectFile obj(File(path("a.bo")));
    const auto *counter = symbol(obj, "counter");
    ASSERT_NE(counter, nullptr);
    EXPECT_EQ(counter->binding_info, Binding::WEAK_DECLARED);
    EXPECT_EQ(counter->section, obj.section_table.at(".bss"));
}

TEST_F(AssemblerIntegration, a_real_definition_replaces_a_common_symbol)
{
    write_file("a.basm", R"(.global _start
.comm value, 4, 4
.text
_start:
                adrp    x1, value
                add     x1, x1, :lo12:value
                ldr     x3, [x1]
                hlt
)");
    write_file("b.basm", ".global value\n.data\nvalue: .word 42\n");
    ASSERT_NO_FATAL_FAILURE(build("-o real a.basm b.basm -outdir ."));
    ASSERT_NO_FATAL_FAILURE(run("real.bexe"));
    EXPECT_EQ(reg(3), 42u);
}

// .init_array collects function addresses from all files between two symbols.
TEST_F(AssemblerIntegration, init_array_bounds)
{
    write_file("main.basm", R"(.global _start
.text
_start:
                adrp    x10, __init_array_start
                add     x10, x10, :lo12:__init_array_start
                adrp    x11, __init_array_end
                add     x11, x11, :lo12:__init_array_end
                adrp    x13, __fini_array_start
                add     x13, x13, :lo12:__fini_array_start
                adrp    x14, __fini_array_end
                add     x14, x14, :lo12:__fini_array_end
                mov     x12, 0
                sub     x15, x11, x10
loop:
                cmp     x10, x11
                b.hs    done
                ldr     x1, [x10], 4
                blx     x1
                b       loop
done:
                hlt
init_a:         add     x12, x12, 1
                ret
init_b:         add     x12, x12, 10
                ret

.init_array
                .word init_a, init_b
)");
    write_file("other.basm", R"(.global init_c
.text
init_c:         add     x12, x12, 100
                ret
.init_array
                .word init_c
)");
    ASSERT_NO_FATAL_FAILURE(build("-o ctors main.basm other.basm -outdir ."));
    ASSERT_NO_FATAL_FAILURE(run("ctors.bexe"));
    EXPECT_EQ(reg(12), 111u) << "all three ran";
    EXPECT_EQ(reg(15), 12u) << "three words";
    EXPECT_EQ(reg(13), reg(14)) << "no .fini_array: the bounds are the same";
}

// What the C compiler is expected to use: ldr =, cset/csel and the extension and byte instructions.
TEST_F(AssemblerIntegration, compiler_helper_instructions)
{
    write_file("helpers.basm", R"(.global _start
.text
_start:
                ldr     x0, =0x12345678          ; three instructions
                ldr     x1, =0 - 5              ; mvn
                ldr     x2, =0x80000000
                ldr     x3, =table + 4          ; an address
                ldr     x4, [x3]                ; table[1]

                mov     x5, 3
                mov     x6, 9
                cmp     x5, x6
                cset    x7, lt                  ; 3 < 9
                cset    x8, gt
                csetm   x9, lt
                csel    x10, x5, x6, gt         ; the larger
                csel    x11, x5, x6, lt         ; the smaller
                cneg    x12, x5, lt             ; -3

                ldr     x13, =0xFFFF8081
                sxtb    x14, x13                ; 0xFFFFFF81
                uxtb    x15, x13                ; 0x81
                sxth    x16, x13                ; 0xFFFF8081
                uxth    x17, x13                ; 0x8081
                clz     x18, x5                 ; 30
                rev     x19, x0                 ; 0x78563412
                rev16   x20, x0                 ; 0x34127856
                hlt

.rodata
table:          .word 11, 22, 33
)");
    ASSERT_NO_FATAL_FAILURE(build("-o helpers helpers.basm -outdir ."));
    ASSERT_NO_FATAL_FAILURE(run("helpers.bexe"));

    EXPECT_EQ(reg(0), 0x12345678u);
    EXPECT_EQ(reg(1), 0xFFFFFFFBu);
    EXPECT_EQ(reg(2), 0x80000000u);
    EXPECT_EQ(reg(4), 22u);
    EXPECT_EQ(reg(7), 1u);
    EXPECT_EQ(reg(8), 0u);
    EXPECT_EQ(reg(9), 0xFFFFFFFFu);
    EXPECT_EQ(reg(10), 9u);
    EXPECT_EQ(reg(11), 3u);
    EXPECT_EQ(reg(12), 0xFFFFFFFDu);
    EXPECT_EQ(reg(14), 0xFFFFFF81u);
    EXPECT_EQ(reg(15), 0x81u);
    EXPECT_EQ(reg(16), 0xFFFF8081u);
    EXPECT_EQ(reg(17), 0x8081u);
    EXPECT_EQ(reg(18), 30u);
    EXPECT_EQ(reg(19), 0x78563412u);
    EXPECT_EQ(reg(20), 0x34127856u);
}

// A bare metal program that builds its own page tables and turns the MMU on (docs/mmu.md). It is
// placed at physical addresses (@P), so it knows where the tables are.
TEST_F(AssemblerIntegration, a_program_builds_page_tables_and_turns_the_mmu_on)
{
    write_file("mmu.basm", R"(.global _start
.text
_start:
                adrp    x0, l1
                add     x0, x0, :lo12:l1        ; the first level table
                adrp    x1, l2
                add     x1, x1, :lo12:l2        ; its only second level table
                orr     x2, x1, 1               ; l1[0] = l2, valid
                str     x2, [x0]

                mov     x3, 0                   ; identity map the first 16 pages: V | W | X
                mov     x4, 16
fill:           lsl     x5, x3, 12
                orr     x5, x5, 7
                lsl     x6, x3, 2
                str     x5, [x1, x6]
                add     x3, x3, 1
                cmp     x3, x4
                b.ne    fill

                mov     x5, 0xC003               ; virtual page 0x40 -> physical page 12, V | W
                mov     x6, 0x100
                str     x5, [x1, x6]

                msr     ptbr, x0
                mov     x7, 1
                msr     sctlr, x7               ; translation is on, the next fetch is mapped

                ldr     x8, =0x40000
                mov     x9, 123
                str     x9, [x8]                ; through the new mapping
                mov     x11, 0xC000
                ldr     x12, [x11]              ; the same word, through the identity mapping
                ldr     x13, [x1, x6]           ; the entry, with accessed and dirty set
                tlbi
                tlbi    x8
                hlt

.bss
.align 4096
l1:             .advance 4096
l2:             .advance 4096
)");
    write_file("mmu.ld", "ENTRY(_start)\nSECTIONS (\n @P;\n .text = 0x0;\n .bss = 0x8000;\n)\n");
    ASSERT_NO_FATAL_FAILURE(build("-o mmu mmu.basm -ld mmu.ld -outdir ."));
    ASSERT_NO_FATAL_FAILURE(run("mmu.bexe"));

    EXPECT_EQ(reg(12), 123u);
    EXPECT_EQ(reg(13), 0xC003u | 0x10 | 0x20);
}

// The devices (docs/devices.md): a program prints through the console, reads its input, and waits
// for a timer interrupt that its handler claims from the interrupt controller.
TEST_F(AssemblerIntegration, console_timer_and_interrupt_controller)
{
    write_file("devices.basm", R"(.global _start
.text
_start:
                ldr     x20, =0xF0002000         ; console
                ldr     x21, =0xF0000000         ; interrupt controller
                ldr     x22, =0xF0001000         ; timer

                adrp    x1, message
                add     x1, x1, :lo12:message
print:          ldrb    x2, [x1], 1
                cmp     x2, 0
                b.eq    printed
                strb    x2, [x20]
                b       print
printed:
                ldr     x3, [x20]               ; the two bytes of the input
                ldr     x4, [x20]
                ldr     x5, [x20, 4]            ; status: nothing left, ready to send

                adrp    x0, vectors
                add     x0, x0, :lo12:vectors
                msr     vbar, x0
                mov     x1, 1
                str     x1, [x21, 8]            ; enable line 0, the timer
                mov     x1, 200
                str     x1, [x22, 4]            ; interrupt after 200 instructions
                mov     x1, 1
                str     x1, [x22, 8]
                msr     pstate, 0               ; unmask
spin:           cmp     x7, 0
                b.eq    spin
                hlt

irq_handler:
                ldr     x8, [x21]               ; claim
                mov     x7, 1
                eret

.rodata
message:        .asciz "ok\n"

.text
.align 16
vectors:
                hlt
                nop
                nop
                nop
                hlt
                nop
                nop
                nop
                hlt
                nop
                nop
                nop
                hlt
                nop
                nop
                nop
                hlt
                nop
                nop
                nop
                hlt
                nop
                nop
                nop
                b       irq_handler             ; 6: IRQ
)");
    write_file("input.txt", "ab");
    ASSERT_NO_FATAL_FAILURE(build("-o devices devices.basm -outdir ."));
    ASSERT_NO_FATAL_FAILURE(run("devices.bexe", "--console-input input.txt"));

    EXPECT_NE(log_tail("emu32.log").find("ok\n"), std::string::npos) << log_tail("emu32.log");
    EXPECT_EQ(reg(3), word('a'));
    EXPECT_EQ(reg(4), word('b'));
    EXPECT_EQ(reg(5), 2u);
    EXPECT_EQ(reg(7), 1u) << "the handler ran";
    EXPECT_EQ(reg(8), 0u) << "for the timer's line";
}

// The block device with a disk image file: a program writes a sector, polls until it is done,
// reads it back, and the host sees the sector in the file after the run.
TEST_F(AssemblerIntegration, block_device_with_a_disk_image)
{
    write_file("disk.basm", R"(.global _start
.text
_start:
                ldr     x22, =0xF0003000         ; block device
                ldr     x3, [x22, 0x14]          ; capacity
                mov     x1, 1
                str     x1, [x22, 8]            ; sector 1
                mov     x4, 0
                str     x4, [x22, 0x10]          ; cursor 0
                ldr     x5, =0x1234
                str     x5, [x22, 0xC]           ; first word of the buffer
                mov     x1, 2
                str     x1, [x22, 0]            ; write
wait1:          ldr     x6, [x22, 4]
                tst     x6, 1
                b.ne    wait1
                str     xzr, [x22, 4]           ; acknowledge

                mov     x1, 3
                str     x1, [x22, 0]            ; flush
wait2:          ldr     x6, [x22, 4]
                tst     x6, 1
                b.ne    wait2
                str     xzr, [x22, 4]

                str     x4, [x22, 0x10]
                str     x4, [x22, 0xC]           ; scribble over the buffer
                mov     x1, 1
                str     x1, [x22, 0]            ; read sector 1 back
wait3:          ldr     x6, [x22, 4]
                tst     x6, 1
                b.ne    wait3
                ldr     x7, [x22, 0xC]
                hlt
)");
    ASSERT_NO_FATAL_FAILURE(build("-o disk disk.basm -outdir ."));
    ASSERT_NO_FATAL_FAILURE(run("disk.bexe", "--block-file disk.img --block-sectors 4"));

    EXPECT_EQ(reg(3), 4u);
    EXPECT_EQ(reg(7), 0x1234u);

    std::ifstream image(m_dir / "disk.img", std::ios::binary);
    std::vector<char> bytes((std::istreambuf_iterator<char>(image)),
                            std::istreambuf_iterator<char>());
    ASSERT_EQ(bytes.size(), 4u * 512);
    EXPECT_EQ(bytes[512], 0x34);
    EXPECT_EQ(bytes[513], 0x12);
    EXPECT_EQ(bytes[0], 0);
}

// adr is the address of a label in one instruction: a label of the code (the call through a
// register, a jump table of the code itself), and data in another section, the linker works out
// the distance. A number can be added to the symbol.
TEST_F(AssemblerIntegration, adr_gives_the_address_of_a_label_in_one_instruction)
{
    write_file("adr.basm", R"(.global _start

.data
table:          .word 11, 22, 33

.text
_start:
                adr     x1, callee
                blx     x1                      ; x0 = 7
                adr     x2, table
                ldr     x3, [x2, 4]             ; 22
                adr     x4, table + 8
                ldr     x5, [x4]                ; 33
                adr     x6, _start              ; backwards, to the first instruction
                adr     x7, here
here:           hlt

callee:
                mov     x0, 7
                ret
)");
    ASSERT_NO_FATAL_FAILURE(build("-o adr adr.basm -outdir ."));

    ASSERT_NO_FATAL_FAILURE(run("adr.bexe"));
    EXPECT_EQ(reg(0), 7u);
    EXPECT_EQ(reg(1), 9 * 4u) << "callee is after the 9 instructions before it";
    EXPECT_EQ(reg(2), 0x1000u) << "the data starts on the first page after the code";
    EXPECT_EQ(reg(3), 22u);
    EXPECT_EQ(reg(4), 0x1008u);
    EXPECT_EQ(reg(5), 33u);
    EXPECT_EQ(reg(6), 0u);
    EXPECT_EQ(reg(7), 8 * 4u) << "the address of the next instruction, which is the hlt";
}

// The distance is 21 bits of bytes: a megabyte either way.
TEST_F(AssemblerIntegration, adr_to_a_symbol_more_than_a_megabyte_away_fails_to_link)
{
    write_file("far.basm", R"(.global _start

.bss
pad:            .advance 1100000
far:            .advance 4

.text
_start:
                adr     x0, far
                hlt
)");
    EXPECT_NE(basm("-o far far.basm -outdir ."), 0);
    EXPECT_NE(log_tail("basm.log").find("cannot reach"), std::string::npos)
        << log_tail("basm.log");
}

// .section "name", "flags": a section of the program's own is linked, loaded and used like the
// sections of the assembler. A table that is read only, state that is written, and a function in a
// section of code of its own that the program calls.
TEST_F(AssemblerIntegration, sections_of_the_programs_own)
{
    write_file("own.basm", R"(.global _start
.global double_it

.section "table", "r"
values:         .word 11, 22, 33

.section "counter"                              ; "rw"
count:          .word 5

.section "helpers", "rx"
double_it:
                add     x0, x0, x0
                ret

.text
_start:
                adr     x1, values
                ldr     x2, [x1, 4]             ; 22
                adr     x3, count
                ldr     x4, [x3]
                add     x4, x4, 1
                str     x4, [x3]
                ldr     x5, [x3]                ; 6
                mov     x0, 21
                bl      double_it               ; 42
                adr     x6, double_it
                hlt
)");
    ASSERT_NO_FATAL_FAILURE(build("-o own own.basm -outdir ."));

    ASSERT_NO_FATAL_FAILURE(run("own.bexe"));
    EXPECT_EQ(reg(2), 22u);
    EXPECT_EQ(reg(5), 6u);
    EXPECT_EQ(reg(0), 42u);
    EXPECT_NE(reg(1), 0u);
    EXPECT_NE(reg(6), 0u);
    EXPECT_NE(reg(6) >> 12, reg(1) >> 12) << "the code and the read only data are on other pages";
    EXPECT_NE(reg(1) >> 12, reg(3) >> 12) << "and so is the data";
}

TEST_F(AssemblerIntegration, a_read_only_section_of_the_programs_own_cannot_be_written)
{
    write_file("ro.basm", R"(.global _start

.section "table", "r"
values:         .word 1

.text
_start:
                adr     x1, values
                mov     x2, 7
                str     x2, [x1]
                hlt
)");
    ASSERT_NO_FATAL_FAILURE(build("-o ro ro.basm -outdir ."));

    EXPECT_EQ(emu32("-e ro.bexe -l 100"), S32(Emulator32bit::EmuCLIExitCode::EXIT_FAULT))
        << log_tail("emu32.log");
    EXPECT_EQ(state("status"), "fault");
    EXPECT_NE(state("message").find("read-only"), std::string::npos) << state("message");
}

TEST_F(AssemblerIntegration, data_of_the_programs_own_cannot_be_run_and_code_cannot_be_written)
{
    write_file("nx.basm", R"(.global _start

.section "blob"
here:           .word 0

.text
_start:
                adr     x1, here
                blx     x1
                hlt
)");
    ASSERT_NO_FATAL_FAILURE(build("-o nx nx.basm -outdir ."));
    EXPECT_EQ(emu32("-e nx.bexe -l 100"), S32(Emulator32bit::EmuCLIExitCode::EXIT_FAULT))
        << log_tail("emu32.log");
    EXPECT_EQ(state("status"), "fault");

    write_file("wx.basm", R"(.global _start

.section "code", "rx"
body:           nop
                hlt

.text
_start:
                adr     x1, body
                str     x1, [x1]
                hlt
)");
    ASSERT_NO_FATAL_FAILURE(build("-o wx wx.basm -outdir ."));
    EXPECT_EQ(emu32("-e wx.bexe -l 100"), S32(Emulator32bit::EmuCLIExitCode::EXIT_FAULT))
        << log_tail("emu32.log");
    EXPECT_EQ(state("status"), "fault");
}

// A linker script places the sections by name. A table of vectors where the exception machinery
// expects them (VBAR), written as a section of its own.
TEST_F(AssemblerIntegration, a_linker_script_places_a_section_of_the_programs_own)
{
    write_file("place.basm", R"(.global _start
.global handler

.section ".vectors", "rx"
vectors:
                hlt
                hlt
                hlt
                hlt
handler:
                mov     x9, 77
                hlt

.text
_start:
                b       handler
)");
    write_file("place.ld", R"(ENTRY(_start)
SECTIONS (
    .text = 0x0;
    ".vectors" = 0x2000;
    .data;
    .bss;
)
)");
    ASSERT_NO_FATAL_FAILURE(build("-o place place.basm -ld place.ld -outdir ."));

    ASSERT_NO_FATAL_FAILURE(run("place.bexe"));
    EXPECT_EQ(reg(9), 77u);

    ObjectFile exe{File((m_dir / "place.bexe").string())};
    const ObjectFile::SymbolTableEntry *handler_symbol = symbol(exe, "handler");
    ASSERT_NE(handler_symbol, nullptr);
    EXPECT_EQ(handler_symbol->symbol_value, 0x2000u + 16);
}

TEST_F(AssemblerIntegration, a_section_of_the_programs_own_that_the_script_does_not_place_is_an_error)
{
    write_file("unplaced.basm", R"(.global _start

.section "extra"
.word 1

.text
_start:
                hlt
)");
    write_file("unplaced.ld", "SECTIONS (\n.text = 0;\n.data;\n.bss;\n)\n");
    EXPECT_NE(basm("-o unplaced unplaced.basm -ld unplaced.ld -outdir ."), 0);
    EXPECT_NE(log_tail("basm.log").find("The section extra has contents"), std::string::npos)
        << log_tail("basm.log");
}

// The sections of a static library are linked with the program like those of any object file.
TEST_F(AssemblerIntegration, a_section_of_the_programs_own_in_a_library)
{
    write_file("lib.basm", R"(.global lookup

.section "tables", "r"
squares:        .word 0, 1, 4, 9, 16

.text
lookup:                                         ; x0 = squares[x0]
                adr     x1, squares
                lsl     x0, x0, 2
                ldr     x0, [x1, x0]
                ret
)");
    write_file("main.basm", R"(.global _start
.extern lookup

.text
_start:
                mov     x0, 3
                bl      lookup
                hlt
)");
    ASSERT_NO_FATAL_FAILURE(build("-ar -o libsquares lib.basm -outdir ."));
    ASSERT_NO_FATAL_FAILURE(build("-o usesquares main.basm -l libsquares.ba -outdir ."));

    ASSERT_NO_FATAL_FAILURE(run("usesquares.bexe"));
    EXPECT_EQ(reg(0), 9u);
}

// A zero filled section of its own ("nobits") is only a size in the files, and a block of memory
// when the program runs: here the stack of a program, at an address the linker script chooses.
TEST_F(AssemblerIntegration, a_nobits_section_is_memory_the_program_can_use_and_not_bytes_in_the_file)
{
    write_file("stack.basm", R"(.global _start

.section "stack", "rw", "nobits"
.align 16
stack_bottom:   .advance 0x8000                  ; 32 KiB
stack_top:

.text
_start:
                adr     x1, stack_bottom
                adr     x2, stack_top
                sub     x3, x2, x1              ; 0x8000
                and     x4, x1, 15              ; 0, it is aligned
                ldr     x5, [x1]                ; 0
                mov     x6, 99
                str     x6, [x2, -4]            ; the last word
                ldr     x7, [x2, -4]
                hlt
)");
    write_file("stack.ld", R"(ENTRY(_start)
SECTIONS (
    .text = 0x0;
    "stack" = 0x10000;
    .data;
    .bss;
)
)");
    ASSERT_NO_FATAL_FAILURE(build("-o stack stack.basm -ld stack.ld -outdir ."));
    EXPECT_LT(std::filesystem::file_size(m_dir / "stack.bexe"), 4096u)
        << "32 KiB of zeros are not in the file";

    ASSERT_NO_FATAL_FAILURE(run("stack.bexe"));
    EXPECT_EQ(reg(1), 0x10000u);
    EXPECT_EQ(reg(2), 0x10000u + 0x8000);
    EXPECT_EQ(reg(3), 0x8000u);
    EXPECT_EQ(reg(4), 0u);
    EXPECT_EQ(reg(5), 0u);
    EXPECT_EQ(reg(7), 99u);
}

TEST_F(AssemblerIntegration, a_nobits_section_cannot_be_run)
{
    write_file("nobitsx.basm", R"(.global _start

.section "zeros", "rw", "nobits"
here:           .advance 16

.text
_start:
                adr     x1, here
                blx     x1
                hlt
)");
    ASSERT_NO_FATAL_FAILURE(build("-o nobitsx nobitsx.basm -outdir ."));
    EXPECT_EQ(emu32("-e nobitsx.bexe -l 100"), S32(Emulator32bit::EmuCLIExitCode::EXIT_FAULT))
        << log_tail("emu32.log");
    EXPECT_EQ(state("status"), "fault");
}

// .fill repeats a value (here a pattern for memory that is checked later), and .pushsection puts
// a string in .rodata from the middle of the code and .popsection goes on in .text.
TEST_F(AssemblerIntegration, fill_and_pushsection_in_a_program)
{
    write_file("fill.basm", R"(.global _start

.data
pattern:        .fill 4, 4, 0xDEADBEEF
bytes:          .fill 3, 1, 7

.text
_start:
                adr     x1, greeting
                ldrb    x2, [x1]                ; 'h'
.pushsection ".rodata", "r"
greeting:       .asciz "hi"
.popsection
                adr     x5, pattern
                ldr     x6, [x5, 12]            ; the last of the four words
                ldrb    x7, [x5, 18]            ; the last of the three bytes
                mov     x3, 5                   ; still in .text
                hlt
)");
    ASSERT_NO_FATAL_FAILURE(build("-o fill fill.basm -outdir ."));

    ObjectFile obj(File(path("fill.bo")));
    EXPECT_EQ(obj.text_section.size(), 7u) << "everything after .popsection is code";
    EXPECT_EQ(obj.rodata_section.size(), 3u);

    ASSERT_NO_FATAL_FAILURE(run("fill.bexe"));
    EXPECT_EQ(reg(2), U32('h'));
    EXPECT_EQ(reg(6), 0xDEADBEEFu);
    EXPECT_EQ(reg(7), 7u);
    EXPECT_EQ(reg(3), 5u);
}

// The listing of -dump shows them.
TEST_F(AssemblerIntegration, the_dump_lists_sections_of_the_programs_own)
{
    write_file("dump.basm", R"(.global _start

.section "table", "r"
.word 1, 2

.text
_start:
                hlt
)");
    ASSERT_NO_FATAL_FAILURE(build("-o dump dump.basm -outdir . -dump"));
    const std::string log = log_tail("basm.log", 100000);
    EXPECT_NE(log.find("Contents of section table:"), std::string::npos) << log;
}
