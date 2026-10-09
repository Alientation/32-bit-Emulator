#include "assembler/build.h"

#include "assembler/assembler.h"
#include "assembler/linker.h"
#include "assembler/object_file.h"
#include "assembler/preprocessor.h"
#include "assembler/static_library.h"
#include "util/common.h"
#include "util/directory.h"
#include "util/logger.h"
#include "util/string_util.h"

#include <charconv>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

bool Build::valid_src_file(const File &file)
{
    return file.get_extension() == SOURCE_EXTENSION || file.get_extension() == INCLUDE_EXTENSION;
}

bool Build::valid_processed_file(const File &file)
{
    return file.get_extension() == PROCESSED_EXTENSION;
}

bool Build::valid_obj_file(const File &file)
{
    return file.get_extension() == OBJECT_EXTENSION;
}

bool Build::valid_exe_file(const File &file)
{
    return file.get_extension() == EXECUTABLE_EXTENSION;
}

Build::Build(const std::string &assembler_args) :
    Build(split_args(assembler_args))
{
}

Build::Build(const std::vector<std::string> &args)
{
    AEMU_INFO("Build: {} argument(s).", args.size());
    AEMU_INFO("Current Working Directory: {}", std::filesystem::current_path().string());

    flags = {
        // Treats everything after as a regular argument.
        {"--", &Build::_ignore},

        // Prints the version of the assembler.
        {"-v", &Build::_version},
        {"--version", &Build::_version},

        // Instead of building an executable, packages the object files into a library (.ba).
        {"-ar", &Build::_ar},
        {"--archive", &Build::_ar},

        // Only compiles the source files into object files.
        {"-c", &Build::_compile},
        {"--compile", &Build::_compile},

        // Path to the output file (the executable, or the library with -ar).
        {"-o", &Build::_output},
        {"--output", &Build::_output},

        // Directory where all object files are stored.
        {"-outdir", &Build::_outdir},

        // Makes a warning type an error (-W error).
        {"-W", &Build::_warn},
        {"--warning", &Build::_warn},

        // Turns on every warning type.
        {"-wall", &Build::_warn_all},

        // Adds a directory to search for included files.
        {"-I", &Build::_include},
        {"--include", &Build::_include},

        // Links the given library file into the program.
        {"-l", &Build::_library},
        {"--library", &Build::_library},

        // Links every library in the given directory.
        {"-L", &Build::_library_directory},
        {"--librarydir", &Build::_library_directory},

        // Defines a preprocessor symbol, as if the source began with `#define name value`.
        {"-D", &Build::_preprocessor_flag},

        // Keeps the preprocessed text of each source in a .bi file.
        {"-kp", &Build::_keep_preprocessor_output},

        // Linker script to use instead of the default layout.
        {"-ld", &Build::_ld},
        {"--linker-script", &Build::_ld},

        // Prints a listing of every object file and the executable.
        {"-dump", &Build::_dump},

        // Displays the options.
        {"-h", &Build::_help},
        {"--help", &Build::_help},

        // TODO: -E only runs the preprocessor.
        // TODO: -C keeps comments in the preprocessed output.
    };

    std::vector<std::string> args_list = args;
    for (size_t i = 0; i < args_list.size(); i++)
    {
        AEMU_DEBUG("Build::Build() - args_list[{}]: {}", i, args_list[i]);
    }

    evaluate_args(args_list);
}

void Build::run()
{
    if (m_has_work) build();
}

bool Build::has_work() const
{
    return m_has_work;
}

PreprocessorOptions Build::preprocessor_options() const
{
    return {.system_dirs = m_system_dirs,
            .defines = m_preprocessor_flags,
            .write_output = keep_proccessed_files};
}

std::vector<std::string> Build::split_args(const std::string &line)
{
    std::vector<std::string> args;
    bool is_escaped = false;
    bool is_quoted = false;
    bool in_arg = false; // an empty "" is an argument too
    std::string cur_arg;
    for (const char c : line)
    {
        if (is_escaped)
        {
            // whatever it is, it is part of the argument
            cur_arg += c;
            is_escaped = false;
        }
        else if (c == '\\')
        {
            is_escaped = true;
            in_arg = true;
        }
        else if (c == '"')
        {
            is_quoted = !is_quoted;
            in_arg = true;
        }
        else if (std::isspace(static_cast<unsigned char>(c)) && !is_quoted)
        {
            if (in_arg)
            {
                args.push_back(cur_arg);
                cur_arg.clear();
                in_arg = false;
            }
        }
        else
        {
            cur_arg += c;
            in_arg = true;
        }
    }

    // check if there are any dangling quotes or escape characters
    AEMU_CHECK(!is_quoted, "Build::split_args() - Missing end quotes: {}", line);
    AEMU_CHECK(!is_escaped, "Build::split_args() - Dangling escape character: {}", line);

    if (in_arg)
    {
        args.push_back(cur_arg);
    }
    return args;
}

void Build::evaluate_args(std::vector<std::string> &args_list)
{
    // evaluate arguments
    for (size_t i = 0; i < args_list.size(); i++)
    {
        AEMU_DEBUG("arg {}: {}", i, args_list[i]);

        std::string &arg = args_list[i];
        if (m_parse_options && !arg.empty() && arg[0] == '-')
        {
            // this is a flag
            AEMU_CHECK(flags.find(arg) != flags.end(), "Build::evaluate_args() - Invalid flag: {}",
                       arg);

            (this->*flags[arg])(args_list, i);

            // --help and --version print and stop, whatever follows is not looked at
            if (!m_has_work) return;
        }
        else
        {
            // this should be a file
            File file(arg);

            AEMU_DEBUG("Build::evaluate_args() - Adding file {}", file.get_path());

            // check the extension
            AEMU_CHECK(file.get_extension() == SOURCE_EXTENSION,
                       "Build::evaluate_args() - Invalid file extension: {}", file.get_extension());

            m_src_files.push_back(file);
        }
    }
}

void Build::build()
{
    if (m_src_files.size() == 0) AEMU_FATAL("ERROR: missing source files to assemble");

    preprocess();
    assemble();

    if (m_dump) dump(m_obj_files);

    if (m_make_lib)
    {
        write_static_library(m_obj_files,
                             File(m_output_file + "." + STATIC_LIBRARY_EXTENSION, true));
        return;
    }

    // Only compiles object files
    if (m_only_compile) return;
    link();

    if (m_dump) dump({m_exe_file});
}

void Build::dump(const std::vector<File> &files)
{
    for (const File &file : files) ObjectFile(file).print();
}

void Build::preprocess()
{
    m_processed_files.clear();
    m_preprocessed.clear();
    for (const File &file : m_src_files)
    {
        if (!file.exists())
        {
            AEMU_WARN("File {} does not exist.", file.get_path());
        }

        const std::string output_path =
            m_has_output_dir
                ? m_output_dir + File::SEPARATOR + file.get_name() + "." + PROCESSED_EXTENSION
                : "";
        Preprocessor preprocessor(file, output_path, preprocessor_options());
        m_processed_files.push_back(preprocessor.preprocess());
        m_preprocessed.push_back(preprocessor.take_result());
    }
}

void Build::assemble()
{
    m_obj_files.clear();
    m_objects.clear();
    for (std::size_t i = 0; i < m_processed_files.size(); i++)
    {
        const File &file = m_processed_files[i];
        const std::string output_path =
            m_has_output_dir
                ? m_output_dir + File::SEPARATOR + file.get_name() + "." + OBJECT_EXTENSION
                : "";

        // The tokens come straight from the preprocessor, so errors point at the original source.
        Assembler assembler(file, std::move(m_preprocessed[i]), output_path);
        assembler.set_warnings_as_errors(m_enabled_warnings.count("error") != 0);
        assembler.assemble();
        m_obj_files.push_back(assembler.get_output_file());
        m_objects.push_back(assembler.object());
    }
}

void Build::link()
{
    // The members of the libraries that are listed, and of the ones found in the directories. A
    //       library is read once, however many times it is listed.
    std::vector<LibraryMember> members;
    std::set<std::string> read;
    const auto add_library = [&](const File &lib)
    {
        const std::string key = std::filesystem::weakly_canonical(lib.get_path()).string();
        if (!read.insert(key).second)
        {
            AEMU_DEBUG("Build::link() - The library {} is already read.", lib.get_path());
            return;
        }
        for (LibraryMember &member : read_static_library(lib))
        {
            members.push_back(std::move(member));
        }
    };
    for (const File &lib : m_linked_lib) add_library(lib);
    for (Directory lib_dir : m_library_dirs)
    {
        for (File lib : lib_dir.get_all_subfiles())
        {
            if (lib.get_extension() == STATIC_LIBRARY_EXTENSION) add_library(lib);
        }
    }

    // Of those, the ones that the program uses.
    std::vector<ObjectFile> objects = select_library_members(m_objects, members);

    m_exe_file = File(m_output_file + "." + EXECUTABLE_EXTENSION);
    AEMU_DEBUG("Build::link() - output file name: {}", m_exe_file.get_path());

    Linker linker = m_has_ld_file ? Linker(std::move(objects), m_exe_file, m_ld_file)
                                  : Linker(std::move(objects), m_exe_file);
    linker.link();
}

void Build::_ignore(std::vector<std::string> &args, size_t &index)
{
    UNUSED(args);
    UNUSED(index);
    m_parse_options = false;
}

void Build::_version(std::vector<std::string> &args, size_t &index)
{
    UNUSED(args);
    UNUSED(index);

    std::cout << "Assembler Version: " << ASSEMBLER_VERSION << "." << std::endl;
    m_has_work = false;
}

void Build::_ar(std::vector<std::string> &args, size_t &index)
{
    UNUSED(args);
    UNUSED(index);

    m_make_lib = true;
}

void Build::_compile(std::vector<std::string> &args, size_t &index)
{
    UNUSED(args);
    UNUSED(index);

    m_only_compile = true;
}

void Build::_output(std::vector<std::string> &args, size_t &index)
{
    AEMU_CHECK(index + 1 < args.size(), "Build::_output() - Missing output file path.");
    m_output_file = args[++index];

    // check if the output file is valid
    AEMU_CHECK(File::valid_path(m_output_file),
               "Build::_output() - Invalid output file path: '{}'.", m_output_file);
}

void Build::_outdir(std::vector<std::string> &args, size_t &index)
{
    AEMU_CHECK(index + 1 < args.size(), "Build::_outdir() - Missing output directory path.");
    m_output_dir = args[++index];
    m_has_output_dir = true;
    // check if the output file is valid
    AEMU_CHECK(Directory::valid_path(m_output_dir),
               "Build::_outdir() - Invalid output directory path: '{}'.", m_output_dir);
}

void Build::_warn(std::vector<std::string> &args, size_t &index)
{
    AEMU_CHECK(index + 1 < args.size(), "Build::_warn() - Missing warning type.");
    std::string warning_type = args[++index];

    // check if the warning type is valid
    AEMU_CHECK(WARNINGS.find(warning_type) != WARNINGS.end(),
               "Build::_warn() - Invalid warning type: '{}'.", warning_type);
    m_enabled_warnings.insert(warning_type);
}

void Build::_warn_all(std::vector<std::string> &args, size_t &index)
{
    UNUSED(args);
    UNUSED(index);

    for (std::string warning_type : WARNINGS) m_enabled_warnings.insert(warning_type);
}

void Build::_include(std::vector<std::string> &args, size_t &index)
{
    AEMU_CHECK(index + 1 < args.size(), "Build::_include() - Missing include directory path.");
    std::string dpath = args[++index];

    // check if the include directory is valid
    AEMU_CHECK(Directory::valid_path(dpath),
               "Build::_include() - Invalid include directory path: '{}'.", dpath);
    m_system_dirs.push_back(Directory(dpath));
}

void Build::_library(std::vector<std::string> &args, size_t &index)
{
    AEMU_CHECK(index + 1 < args.size(), "Build::_library() - Missing library file path.");
    std::string fpath = args[++index];

    // check if the library name is valid
    AEMU_CHECK(File::valid_path(fpath), "Build::_library() - Invalid library file path: '{}'.",
               fpath);
    m_linked_lib.push_back(File(fpath));
}

void Build::_library_directory(std::vector<std::string> &args, size_t &index)
{
    AEMU_CHECK(index + 1 < args.size(),
               "Build::_libraryDirectory() - Missing library directory path.");
    std::string dpath = args[++index];

    // check if the library directory is valid
    AEMU_CHECK(Directory::valid_path(dpath),
               "Build::_libraryDirectory() - Invalid library directory path: '{}'.", dpath);
    m_library_dirs.push_back(Directory(dpath));
}

void Build::_preprocessor_flag(std::vector<std::string> &args, size_t &index)
{
    AEMU_CHECK(index + 1 < args.size(), "Build::_preprocessorFlag() - Missing preprocessor flag.");
    std::string flag = args[++index];

    // check if there is a value
    std::string value = "";
    if (flag.find('=') != std::string::npos)
    {
        // there is a value
        value = flag.substr(flag.find('=') + 1);
        flag = flag.substr(0, flag.find('='));
    }

    m_preprocessor_flags[flag] = value;
}

void Build::_keep_preprocessor_output(std::vector<std::string> &args, size_t &index)
{
    UNUSED(args);
    UNUSED(index);

    keep_proccessed_files = true;
}

void Build::_ld(std::vector<std::string> &args, size_t &index)
{
    AEMU_CHECK(index + 1 < args.size(), "Build::_ld() - Missing linker script file path.");
    std::string fpath = args[++index];

    // check if the library name is valid
    AEMU_CHECK(File::valid_path(fpath), "Build::_ld() - Invalid linker script file path: '{}'.",
               fpath);
    m_ld_file = File(fpath);
    m_has_ld_file = true;
}

void Build::_dump(std::vector<std::string> &args, size_t &index)
{
    UNUSED(args);
    UNUSED(index);

    m_dump = true;
}

void Build::_help(std::vector<std::string> &args, size_t &index)
{
    UNUSED(args);
    UNUSED(index);

    std::cout << "This is the BASM assembler. Usage:\n\n";

    std::cout << "basm [options] file...\n";
    std::cout << "Options:\n";

    auto print_option =
        [](const std::string &option, const std::string &description, int width = 26)
    { std::cout << std::left << std::setw(width) << option << " " << description << '\n'; };

    print_option("--", "End of options.");
    print_option("-v, --version", "Display assembler version information.");
    print_option("-ar", "Build a library file (.ba).");
    print_option("-c, --compile", "Only compiles the source files into object files.");
    print_option("-o, --output <file>", "Path to output file.");
    print_option("-outdir <dir>", "Path to directory where object files will be stored.");
    print_option("-W, --warning <type>", "Turns on a kind of warning. The only one is 'error':");
    print_option("", "a warning ends the build like an error.");
    print_option("-wall", "Turns on every kind of warning.");
    print_option("-I, --include <dir>",
                 "Add directory to search for system files from include macros.");
    print_option("-l, --library <file>", "Links given library file to program.");
    print_option("-L, --librarydir <dir>",
                 "Links all library files in the given directory recursively to program.");
    print_option("-D <name[=value]>",
                 "Define a preprocessor symbol, as if the source started with");
    print_option("", "#define name value.");
    print_option("-kp", "Write the preprocessed source of each file to a .bi file and keep it.");
    print_option("-ld, --linker-script <file>",
                 "Use the linker script (.ld) instead of the default.");
    print_option("-dump", "Print a listing of each object file and the executable.");
    print_option("-h, --help", "Display options help.");
    m_has_work = false;
}

// FOR NOW getters
bool Build::does_create_exe() const
{
    return !m_make_lib && !m_only_compile;
}

std::set<std::string> Build::get_enabled_warnings() const
{
    return m_enabled_warnings;
}

std::map<std::string, std::string> Build::get_preprocessor_flags() const
{
    return m_preprocessor_flags;
}

std::vector<Directory> Build::get_system_dirs() const
{
    return m_system_dirs;
}

std::vector<File> Build::get_processed_files() const
{
    return m_processed_files;
}

std::vector<File> Build::get_obj_files() const
{
    return m_obj_files;
}

File Build::get_exe_file() const
{
    return m_exe_file;
}

File Build::get_ld_file() const
{
    return m_ld_file;
}

bool Build::has_ld_file() const
{
    return m_has_ld_file;
}