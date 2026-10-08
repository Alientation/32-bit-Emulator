#include <assembler_test/toolchain_fixture.h>

#include <assembler/static_library.h>

// The command line driver. Build is in-process here, so these tests also show that it can be used
// as a library: it reports errors by throwing and never ends the process.
class BuildProcess : public ToolchainFixture
{
  protected:
    static constexpr const char *kProgram = ".global _start\n.text\n_start:\n  mov x0, 7\n  hlt\n";

    std::string out(const std::string &name) const
    {
        return (m_dir / "out" / name).string();
    }

    /// The arguments to build `main.basm` into `out/prog.bexe`.
    std::string build_args(const std::string &extra = "")
    {
        return extra + " -o " + out("prog") + " " + write("main.basm", kProgram) + " -outdir "
               + (m_dir / "out").string();
    }
};

TEST_F(BuildProcess, the_constructor_only_parses_the_arguments)
{
    Build process(build_args());
    EXPECT_TRUE(process.has_work());
    EXPECT_FALSE(fs::exists(out("prog.bexe")));
    EXPECT_FALSE(fs::exists(out("main.bo")));

    process.run();
    EXPECT_TRUE(fs::exists(out("prog.bexe")));
    EXPECT_TRUE(fs::exists(out("main.bo")));
    EXPECT_EQ(process.get_exe_file().get_path(), out("prog.bexe"));
}

TEST_F(BuildProcess, compile_only_stops_before_linking)
{
    Build process(build_args("-c"));
    process.run();
    EXPECT_TRUE(fs::exists(out("main.bo")));
    EXPECT_FALSE(fs::exists(out("prog.bexe")));
    EXPECT_FALSE(process.does_create_exe());
}

TEST_F(BuildProcess, help_and_version_only_print)
{
    // No source file is needed, and nothing after the flag is looked at.
    testing::internal::CaptureStdout();
    Build help("--help --not-a-flag");
    EXPECT_FALSE(help.has_work());
    EXPECT_NO_THROW(help.run());
    const std::string help_text = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(contains(help_text, "basm [options] file..."));

    testing::internal::CaptureStdout();
    Build version("-v");
    EXPECT_FALSE(version.has_work());
    EXPECT_NO_THROW(version.run());
    EXPECT_TRUE(contains(testing::internal::GetCapturedStdout(), "Assembler Version"));
}

TEST_F(BuildProcess, errors_are_thrown)
{
    EXPECT_TRUE(contains(error_of([&] { Build process("--not-a-flag"); }), "Invalid flag"));
    EXPECT_TRUE(contains(error_of(
                             [&]
                             {
                                 Build process("");
                                 process.run();
                             }),
                         "missing source files"));

    const std::string bad = write("bad.basm", ".text\n  bogus x0\n");
    EXPECT_FALSE(error_of(
                     [&]
                     {
                         Build process(bad + " -outdir " + (m_dir / "out").string());
                         process.run();
                     })
                     .empty());
}

TEST_F(BuildProcess, include_directories_reach_the_preprocessor)
{
    write("include/value.binc", "#define VALUE 7\n");
    const std::string main = write("inc.basm", "#include <\"value.binc\">\n.global _start\n.text\n"
                                               "_start:\n  mov x0, VALUE\n  hlt\n");
    Build process("-I " + (m_dir / "include").string() + " -o " + out("inc") + " " + main
                  + " -outdir " + (m_dir / "out").string());
    EXPECT_NO_THROW(process.run());
    EXPECT_TRUE(fs::exists(out("inc.bexe")));
}

// ---------------------------------------------------------------------------------------------
// The arguments
// ---------------------------------------------------------------------------------------------

TEST_F(BuildProcess, a_line_is_split_on_whitespace)
{
    using Args = std::vector<std::string>;
    EXPECT_EQ(Build::split_args("a b  c"), (Args{"a", "b", "c"}));
    EXPECT_EQ(Build::split_args("  a\tb\n"), (Args{"a", "b"}));
    EXPECT_EQ(Build::split_args(""), Args{});
    EXPECT_EQ(Build::split_args("   "), Args{});
}

TEST_F(BuildProcess, quotes_and_backslashes_keep_whitespace_in_an_argument)
{
    using Args = std::vector<std::string>;
    EXPECT_EQ(Build::split_args("-o \"my dir/prog\" x"), (Args{"-o", "my dir/prog", "x"}));
    EXPECT_EQ(Build::split_args("my\\ dir/prog"), Args{"my dir/prog"});
    EXPECT_EQ(Build::split_args("a\\\"b"), Args{"a\"b"}) << "an escaped quote is a quote";
    EXPECT_EQ(Build::split_args("a\"b c\"d"), Args{"ab cd"});
    EXPECT_EQ(Build::split_args("a \"\" b"), (Args{"a", "", "b"})) << "an empty argument";
    EXPECT_TRUE(contains(error_of([] { Build::split_args("\"open"); }), "Missing end quotes"));
    EXPECT_TRUE(
        contains(error_of([] { Build::split_args("end\\"); }), "Dangling escape character"));
}

// Argv is not split again, so nothing in a path can be taken for something else.
TEST_F(BuildProcess, a_path_with_spaces_is_one_argument)
{
    const std::string source = write("my sources/main file.basm", kProgram);
    Build process(std::vector<std::string>{"-o", out("my program"), source, "-outdir",
                                           (m_dir / "out").string()});
    EXPECT_NO_THROW(process.run());
    EXPECT_TRUE(fs::exists(out("my program.bexe")));
    EXPECT_TRUE(fs::exists(out("main file.bo")));
}

TEST_F(BuildProcess, an_empty_argument_is_not_a_flag_nor_a_source_file)
{
    EXPECT_TRUE(contains(error_of([] { Build process(std::vector<std::string>{""}); }),
                         "File path does not contain an extension"));
}

TEST_F(BuildProcess, the_optimization_level_is_checked)
{
    EXPECT_NO_THROW(Build("-O 0 " + build_args()));
    EXPECT_NO_THROW(Build("-O 3 " + build_args()));
    EXPECT_EQ(Build("-O 2 " + build_args()).get_optimization_level(), 2);
    EXPECT_TRUE(contains(error_of([&] { Build("-O 4 " + build_args()); }),
                         "Invalid optimization level: '4'"));
    EXPECT_TRUE(contains(error_of([&] { Build("-O fast " + build_args()); }),
                         "Invalid optimization level: 'fast'"));
    EXPECT_TRUE(contains(error_of([&] { Build("-O 2x " + build_args()); }), "'2x'"));
    EXPECT_TRUE(contains(error_of([] { Build("-O"); }), "Missing optimization level"));
}

// ---------------------------------------------------------------------------------------------
// Warnings
// ---------------------------------------------------------------------------------------------

// A zero offset makes the write back of a pre indexed access pointless, which is a warning.
TEST_F(BuildProcess, a_warning_does_not_stop_the_build)
{
    const std::string source = write("warn.basm", ".global _start\n.text\n_start:\n"
                                                  "  ldr x1, [x2, 0]!\n  hlt\n");
    Build process("-o " + out("warn") + " " + source + " -outdir " + (m_dir / "out").string());
    EXPECT_NO_THROW(process.run());
    EXPECT_TRUE(fs::exists(out("warn.bexe")));
}

TEST_F(BuildProcess, warnings_are_errors_with_the_error_warning)
{
    const std::string source = write("warn.basm", ".global _start\n.text\n_start:\n"
                                                  "  ldr x1, [x2, 0]!\n  hlt\n");
    const std::string rest =
        " -o " + out("warn") + " " + source + " -outdir " + (m_dir / "out").string();

    for (const char *flag : {"-W error", "--warning error", "-wall"})
    {
        const std::string message = error_of([&] { Build(std::string(flag) + rest).run(); });
        EXPECT_TRUE(contains(message, "the pre-index offset is zero")) << flag;
        EXPECT_TRUE(contains(message, "warnings are errors")) << flag;
        EXPECT_TRUE(contains(message, "warn.basm:4:3")) << flag << ": " << message;
    }
    EXPECT_FALSE(fs::exists(out("warn.bexe")));
}

TEST_F(BuildProcess, only_known_warnings_can_be_turned_on)
{
    EXPECT_TRUE(contains(error_of([&] { Build("-W everything " + build_args()); }),
                         "Invalid warning type: 'everything'"));
    EXPECT_EQ(Build("-W error " + build_args()).get_enabled_warnings(),
              std::set<std::string>{"error"});
}

// ---------------------------------------------------------------------------------------------
// The preprocessed file
// ---------------------------------------------------------------------------------------------

// The assembler is given the tokens, so there is no .bi to leave behind (or to fail to write in a
// directory that cannot be written to).
TEST_F(BuildProcess, the_preprocessed_file_is_not_written_by_default)
{
    Build process(build_args());
    process.run();
    EXPECT_TRUE(fs::exists(out("prog.bexe")));
    EXPECT_FALSE(fs::exists(out("main.bi")));
    EXPECT_FALSE(fs::exists(m_dir / "main.bi"));

    // And without an output directory it would have been next to the source.
    Build next_to_source("-o " + out("again") + " " + (m_dir / "main.basm").string());
    next_to_source.run();
    EXPECT_FALSE(fs::exists(m_dir / "main.bi"));
}

TEST_F(BuildProcess, kp_keeps_the_preprocessed_file)
{
    Build process(build_args("-kp"));
    process.run();
    ASSERT_TRUE(fs::exists(out("main.bi")));
    EXPECT_TRUE(contains(read(out("main.bi")), "mov x0, 7"));
}

// ---------------------------------------------------------------------------------------------
// Static libraries
// ---------------------------------------------------------------------------------------------

class StaticLibraryBuild : public BuildProcess
{
  protected:
    std::vector<std::string> m_library_sources;

    /// Builds the library `<name>.ba` of the sources, written as `<file>.basm`.
    std::string library(const std::string &name,
                        const std::vector<std::pair<std::string, std::string>> &sources)
    {
        std::string files;
        for (const auto &[file, text] : sources)
        {
            files += " " + write("lib/" + file + ".basm", text);
        }
        Build("-ar -o " + out(name) + files + " -outdir " + (m_dir / "out").string()).run();
        return out(name + ".ba");
    }

    /// Links `main.basm` with the libraries and returns the executable.
    ObjectFile link(const std::string &main_source, const std::vector<std::string> &libraries)
    {
        std::string flags;
        for (const std::string &lib : libraries)
        {
            flags += " -l " + lib;
        }
        Build("-o " + out("linked") + flags + " " + write("main.basm", main_source) + " -outdir "
              + (m_dir / "out").string())
            .run();
        return ObjectFile(File(out("linked.bexe")));
    }

    static bool has_symbol(const ObjectFile &exe, const std::string &name)
    {
        return exe.string_table.count(name) != 0;
    }
};

TEST_F(StaticLibraryBuild, a_member_that_nothing_uses_is_not_linked)
{
    const std::string lib = library("libtwo", {{"used", ".global used\n.text\nused: ret\n"},
                                               {"unused", ".global unused\n.text\nunused: ret\n"}});
    const ObjectFile exe = link(".global _start\n.text\n_start: bl used\nhlt\n", {lib});

    EXPECT_TRUE(has_symbol(exe, "used"));
    EXPECT_FALSE(has_symbol(exe, "unused"));
    EXPECT_EQ(exe.text_section.size(), 3u) << "_start (2) and used (1), and not unused";
}

TEST_F(StaticLibraryBuild, members_that_the_member_needs_are_linked_too)
{
    const std::string lib =
        library("libchain", {{"top", ".global top\n.text\ntop: bl middle\nret\n"},
                             {"middle", ".global middle\n.text\nmiddle: bl bottom\nret\n"},
                             {"bottom", ".global bottom\n.text\nbottom: ret\n"},
                             {"other", ".global other\n.text\nother: ret\n"}});
    const ObjectFile exe = link(".global _start\n.text\n_start: bl top\nhlt\n", {lib});

    EXPECT_TRUE(has_symbol(exe, "top"));
    EXPECT_TRUE(has_symbol(exe, "middle"));
    EXPECT_TRUE(has_symbol(exe, "bottom"));
    EXPECT_FALSE(has_symbol(exe, "other"));
}

TEST_F(StaticLibraryBuild, a_library_listed_twice_is_linked_once)
{
    const std::string lib = library("libonce", {{"once", ".global once\n.text\nonce: ret\n"}});
    EXPECT_NO_THROW(link(".global _start\n.text\n_start: bl once\nhlt\n", {lib, lib}));
}

TEST_F(StaticLibraryBuild, a_symbol_in_two_libraries_comes_from_the_first)
{
    const std::string first = library("libfirst", {{"f", ".global same\n.text\nsame: nop\nret\n"}});
    const std::string second = library("libsecond", {{"s", ".global same\n.text\nsame: ret\n"}});
    const ObjectFile exe = link(".global _start\n.text\n_start: bl same\nhlt\n", {first, second});

    EXPECT_EQ(exe.text_section.size(), 4u) << "_start (2), and the two instructions of the first";
}

TEST_F(StaticLibraryBuild, the_objects_of_the_program_win_over_a_member)
{
    const std::string lib = library("libshadow", {{"s", ".global util\n.text\nutil: ret\n"}});
    const ObjectFile exe = link(".global _start\n.global util\n.text\n_start: bl util\nhlt\n"
                                "util: nop\nret\n",
                                {lib});
    EXPECT_EQ(exe.text_section.size(), 4u) << "the library member is not linked";
}

TEST_F(StaticLibraryBuild, an_unknown_symbol_is_still_an_error)
{
    const std::string lib = library("libnone", {{"x", ".global x\n.text\nx: ret\n"}});
    EXPECT_TRUE(
        contains(error_of([&] { link(".global _start\n.text\n_start: bl y\nhlt\n", {lib}); }),
                 "undefined reference to 'y'"));
}

TEST_F(StaticLibraryBuild, a_library_is_read_back_with_the_names_of_its_members)
{
    const std::string lib = library("libnames", {{"alpha", ".global func_a\n.text\nfunc_a: ret\n"},
                                                 {"beta", ".global func_b\n.text\nfunc_b: ret\n"}});
    const std::vector<LibraryMember> members = read_static_library(File(lib));

    ASSERT_EQ(members.size(), 2u);
    EXPECT_EQ(members[0].name, "alpha.bo");
    EXPECT_EQ(members[1].name, "beta.bo");
    EXPECT_TRUE(members[0].object.string_table.count("func_a") != 0);
}

TEST_F(StaticLibraryBuild, a_file_that_is_not_a_library_is_refused)
{
    write("out/garbage.ba", "this is not a library at all, not even close");
    EXPECT_TRUE(contains(error_of([&] { read_static_library(File(out("garbage.ba"))); }),
                         "is not a static library"));

    write("out/empty.ba", "");
    EXPECT_TRUE(contains(error_of([&] { read_static_library(File(out("empty.ba"))); }),
                         "is not a static library"));
}

TEST_F(StaticLibraryBuild, a_library_that_is_cut_short_is_refused)
{
    const std::string lib = library("libcut", {{"x", ".global x\n.text\nx: ret\n"}});
    const std::string whole = read(lib);
    write("out/short.ba", whole.substr(0, whole.size() - 20));
    EXPECT_FALSE(error_of([&] { read_static_library(File(out("short.ba"))); }).empty());
}

TEST_F(StaticLibraryBuild, only_object_files_can_be_put_in_a_library)
{
    write("out/fake.bo", std::string(200, 'z'));
    EXPECT_FALSE(
        error_of([&] { write_static_library({File(out("fake.bo"))}, File(out("bad.ba"), true)); })
            .empty());
}
