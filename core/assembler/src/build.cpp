#include "assembler/build.h"

#include "assembler/assembler.h"
#include "assembler/linker.h"
#include "assembler/object_file.h"
#include "assembler/preprocessor.h"
#include "assembler/static_library.h"
#include "util/directory.h"
#include "util/logger.h"
#include "util/string_util.h"

#include <filesystem>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <vector>

#define UNUSED(x) (void)(x)

bool Process::valid_src_file(const File& file)
{
    return file.get_extension() == SOURCE_EXTENSION || file.get_extension() == INCLUDE_EXTENSION;
}

bool Process::valid_processed_file(const File& file)
{
    return file.get_extension() == PROCESSED_EXTENSION;
}

bool Process::valid_obj_file(const File& file)
{
    return file.get_extension() == OBJECT_EXTENSION;
}

bool Process::valid_exe_file(const File& file)
{
    return file.get_extension() == EXECUTABLE_EXTENSION;
}


/**
 * @brief Constructs a build process from the specified arguments.
 *
 * @param compilerArgs the arguments to construct the build process from
 */
Process::Process(const std::string& assembler_args)
{
    INFO("Building Process: args(%s).", assembler_args.c_str());
    INFO("Current Working Directory: %s", std::filesystem::current_path().string().c_str());

    flags =
    {
        {"--", &Process::_ignore},                                      /* Treats everything after as a regular argument */

        {"-v", &Process::_version},                                     /* Prints version of assembler */
        {"-version", &Process::_version},

        {"-makelib", &Process::_makelib},                               /* Instead of building an executable, create a collection of
                                                                            object files and package into a single library file (.ba) */

        {"-c", &Process::_compile},                                     /* Only compiles the src code files into object files */
        {"-compile", &Process::_compile},

        {"-o", &Process::_output},                                      /* Path to output file (executable for builds, library files for makelib) */
        {"-out", &Process::_output},
        {"-output", &Process::_output},

        {"-outdir", &Process::_outdir},                                 /* Directory where all object files will be stored */

        {"-O", &Process::_optimize},                                    /* Turns on optimization level *unimplemented* */
        {"-optimize", &Process::_optimize},

        {"-oall", &Process::_optimize_all},                             /* Highest optimization level *unimplemented* */

        {"-W", &Process::_warn},                                        /* Turns on warning level *unimplemented* */
        {"-warning", &Process::_warn},

        {"-wall", &Process::_warn_all},                                 /* Highest warning level *unimplemented* */

        {"-I", &Process::_include},                                     /* Adds directory to search for system files */
        {"-inc", &Process::_include},
        {"-include", &Process::_include},

        {"-l", &Process::_library},                                     /* Links given library file to program */
        {"-lib", &Process::_library},
        {"-library", &Process::_library},

        {"-L", &Process::_library_directory},                           /* Searches for all libraries in given directory and links */
        {"-libdir", &Process::_library_directory},
        {"-librarydir", &Process::_library_directory},

        {"-D", &Process::_preprocessor_flag},                           /* Passes preprocessor flags into the program */

        {"-kp", &Process::_keep_processed_files},                       /* Don't delete intermediate files */

        {"-h", &Process::_help},                                        /* Display options */
        {"-help", &Process::_help},
    };

    // split command args by whitespace unless surrounded by quotes
    std::vector<std::string> args_list;
    parse_args(assembler_args, args_list);

    DEBUG("Process::Process() - args_list.size(): %llu.", args_list.size());
    for (size_t i = 0; i < args_list.size(); i++)
    {
        DEBUG("Process::Process() - args_list[%llu]: %s", i, args_list[i].c_str());
    }

    evaluate_args(args_list);
    build();
}

/**
 * @brief Parses the arguments into a list of arguments.
 *
 * @param compilerArgs the arguments to parse
 * @param args_list the list of arguments to add to
 */
void Process::parse_args(std::string assembler_args, std::vector<std::string>& args_list)
{
    bool is_escaped = false;
    bool is_quoted = false;
    std::string cur_arg = "";
    for (size_t i = 0; i < assembler_args.length(); i++)
    {
        char c = assembler_args[i];
        if (c == '\"' && !is_escaped)
        {
            // this is a quote that is not escaped
            is_quoted = !is_quoted;
        }
        else if (std::isspace(c) && !is_quoted)
        {
            // only add argument if it's not empty
            if (cur_arg.length() > 0)
            {
                args_list.push_back(cur_arg);
                cur_arg = "";
            }
        }
        else
        {
            // check if escape character
            if (c == '\\')
            {
                is_escaped = !is_escaped;
            }
            else
            {
                is_escaped = false;
            }
            cur_arg += c;
        }
    }

    // check if there are any dangling quotes or escape characters
    EXPECT_FALSE_SS(is_quoted, std::stringstream() << "Process::Process() - Missing end quotes: " << assembler_args);
    EXPECT_FALSE_SS(is_escaped, std::stringstream() << "Process::Process() - Dangling escape character: " << assembler_args);

    // add the last argument if it's not empty
    if (cur_arg.length() > 0)
    {
        args_list.push_back(cur_arg);
    }
}

/**
 * @brief Processes the arguments. This is an internal function.
 *
 * @param args_list the list of arguments to process
 */
void Process::evaluate_args(std::vector<std::string>& args_list)
{
    // evaluate arguments
    for (size_t i = 0; i < args_list.size(); i++)
    {
        DEBUG("arg %llu: %s", i, args_list[i].c_str());

        std::string& arg = args_list[i];
        if (m_parse_options && arg[0] == '-')
        {
            // this is a flag
            EXPECT_TRUE_SS(flags.find(arg) != flags.end(), std::stringstream("Process::evaluate_args() - Invalid flag: ") << arg);

            (this->*flags[arg])(args_list, i);
        }
        else
        {
            // this should be a file
            File file(arg);

            DEBUG("Process::evaluate_args() - Adding file %s", file.get_path().c_str());

            // check the extension
            EXPECT_TRUE_SS(file.get_extension() == SOURCE_EXTENSION,
                    std::stringstream("Process::evaluate_args() - Invalid file extension: ") << file.get_extension());

            m_src_files.push_back(file);
        }
    }
}


/**
 * @brief
 *
 */
void Process::build()
{
    preprocess();
    assemble();

    if (m_make_lib)
    {
        WriteStaticLibrary(m_obj_files, File(m_output_file + "." + STATIC_LIBRARY_EXTENSION, true));
        return;
    }

    /* Only compiles object files */
    if (m_only_compile)
    {
        return;
    }
    link();
}

/**
 * @brief
 *
 */
void Process::preprocess()
{
    m_processed_files.clear();
    for (File file : m_src_files)
    {
        if (!file.exists())
        {
            WARN("File %s does not exist.", file.get_path().c_str());
            Directory dir(file.get_dir());
            if (dir.exists())
            {
                DEBUG("But it's parent directory exists at %s with files", dir.get_abs_path().c_str());
                for (File f : dir.get_subfiles())
                {
                    DEBUG("%s", f.get_name().c_str());
                }
            }
        }

        if (m_has_output_dir)
        {
            Preprocessor preprocessor(this, file, m_output_dir + File::SEPARATOR + file.get_name() + "." + PROCESSED_EXTENSION);
            m_processed_files.push_back(preprocessor.preprocess());
        }
        else
        {
            Preprocessor preprocessor(this, file);
            m_processed_files.push_back(preprocessor.preprocess());
        }
    }
}

/**
 * @brief
 *
 */
void Process::assemble()
{
    m_obj_files.clear();
    for (File file : m_processed_files)
    {
        if (m_has_output_dir)
        {
            Assembler assembler(this, file, m_output_dir + File::SEPARATOR + file.get_name() + "." + OBJECT_EXTENSION);
            m_obj_files.push_back(assembler.assemble());
        }
        else
        {
            Assembler assembler(this, file);
            m_obj_files.push_back(assembler.assemble());
        }

        if (!keep_proccessed_files)
        {
            try
            {
                if (!std::filesystem::remove(file.get_path()))
                {
                    std::cout << "file " << file.get_path() << " not found.\n";
                }
            }
            catch (const std::filesystem::filesystem_error& err)
            {
                std::cout << "filesystem error: " << err.what() << "\n";
            }
        }
    }
}

/**
 * @brief
 *
 */
void Process::link()
{
    std::vector<ObjectFile> objects;
    for (File file : m_obj_files)
    {
        objects.push_back(ObjectFile(file));
    }

    /* Link all included libraries */
    for (File lib : m_linked_lib)
    {
        ReadStaticLibrary(objects, lib);
    }

    /* Link all libraries found in the provided directories */
    for (Directory lib_dir : m_library_dirs)
    {
        for (File lib : lib_dir.get_all_subfiles())
        {
            if (lib.get_extension() == STATIC_LIBRARY_EXTENSION)
            {
                ReadStaticLibrary(objects, lib);
            }
        }
    }

    m_exe_file = File(m_output_file + "." + EXECUTABLE_EXTENSION);
    DEBUG("Process::link() - output file name: %s", m_exe_file.get_path().c_str());

    if (m_has_ld_file)
    {
        Linker linker(objects, m_exe_file, m_ld_file);
    }
    else
    {
        Linker linker(objects, m_exe_file);
    }
}

/**
 * @brief
 *
 * @param args
 * @param index
 */
void Process::_ignore(std::vector<std::string>& args, size_t& index)
{
    UNUSED(args);
    UNUSED(index);
    m_parse_options = false;
}

/**
 * @brief Prints out the version of the assembler
 *
 * USAGE: -v, -version
 *
 * @param args the arguments passed to the build process
 * @param index the index of the flag in the arguments list
 */
void Process::_version(std::vector<std::string>& args, size_t& index)
{
    UNUSED(args);
    UNUSED(index);

    std::cout << "Assembler Version: " << ASSEMBLER_VERSION << "." << std::endl;
    exit (EXIT_SUCCESS);
}

/**
 * @brief
 *
 * @param args
 * @param index
 */
void Process::_makelib(std::vector<std::string>& args, size_t& index)
{
    UNUSED(args);
    UNUSED(index);

    m_make_lib = true;
}

/**
 * @brief Compiles the source code files to object files and stops
 *
 * USAGE: -c, -compile
 *
 * @param args the arguments passed to the build process
 * @param index the index of the flag in the arguments list
 */
void Process::_compile(std::vector<std::string>& args, size_t& index)
{
    UNUSED(args);
    UNUSED(index);

    m_only_compile = true;
}

/**
 * @brief Sets the output file
 *
 * USAGE: -o, -output [filename]
 *
 * @param args the arguments passed to the build process
 * @param index the index of the flag in the arguments list
 */
void Process::_output(std::vector<std::string>& args, size_t& index)
{
    EXPECT_TRUE_SS(index + 1 < args.size(), std::stringstream()
            << "Process::_output() - Missing output file path.");
    m_output_file = args[++index];

    // check if the output file is valid
    EXPECT_TRUE_SS(File::valid_path(m_output_file), std::stringstream()
            << "Process::_output() - Invalid output file path: " << m_output_file << ".");
}

/**
 * @brief Sets the output directory for all object files generated
 *
 * USAGE: -outdir [filename]
 *
 * @param args the arguments passed to the build process
 * @param index the index of the flag in the arguments list
 */
void Process::_outdir(std::vector<std::string>& args, size_t& index)
{
    EXPECT_TRUE_SS(index + 1 < args.size(), std::stringstream()
            << "Process::_outdir() - Missing output directory path.");
    m_output_dir = args[++index];
    m_has_output_dir = true;
    // check if the output file is valid
    EXPECT_TRUE_SS(Directory::valid_path(m_output_dir), std::stringstream()
            << "Process::_outdir() - Invalid output directory path: " << m_output_file << ".");
}

/**
 * @brief Sets the optimization level
 *
 * USAGE: -o, -optimize [level]
 *
 * Optimization Levels
 * 0 - no optimization (DEFAULT)
 * 1 - basic optimization
 * 2 - advanced optimization
 * 3 - full optimization
 *
 * @param args the arguments passed to the build process
 * @param index the index of the flag in the arguments list
 */
void Process::_optimize(std::vector<std::string>& args, size_t& index)
{
    EXPECT_TRUE_SS(index + 1 < args.size(), std::stringstream()
            << "Process::_optimize() - Missing optimization level.");
    m_optimization_level = std::stoi(args[++index]);

    // check if the optimization level is valid
    EXPECT_TRUE_SS(0 <= m_optimization_level && m_optimization_level <= 3, std::stringstream()
            << "Process::_optimize() - Invalid optimization level: " << m_optimization_level
            << ".");
}

/**
 * @brief Sets the highest optimization level
 *
 * USAGE: -O, -oall
 *
 * @param args the arguments passed to the build process
 * @param index the index of the flag in the arguments list
 */
void Process::_optimize_all(std::vector<std::string>& args, size_t& index)
{
    UNUSED(args);
    UNUSED(index);

    m_optimization_level = MAX_OPTIMIZATION_LEVEL;
}

/**
 * @brief Turns on warning messages
 *
 * USAGE: -w, -warning [type]
 *
 * Warning Types
 * error - turns warnings into errors
 *
 * @param args the arguments passed to the build process
 * @param index the index of the flag in the arguments list
 */
void Process::_warn(std::vector<std::string>& args, size_t& index)
{
    EXPECT_TRUE_SS(index + 1 < args.size(), std::stringstream()
            << "Process::_warn() - Missing warning type.");
    std::string warning_type = args[++index];

    // check if the warning type is valid
    EXPECT_TRUE_SS(WARNINGS.find(warning_type) != WARNINGS.end(), std::stringstream()
            << "Process::_warn() - Invalid warning type: " << warning_type << ".");
    m_enabled_warnings.insert(warning_type);
}

/**
 * @brief Turns on all warning messages
 *
 * USAGE: -W, -wall
 *
 * @param args the arguments passed to the build process
 * @param index the index of the flag in the arguments list
 */
void Process::_warn_all(std::vector<std::string>& args, size_t& index)
{
    UNUSED(args);
    UNUSED(index);

    for (std::string warning_type : WARNINGS)
    {
        m_enabled_warnings.insert(warning_type);
    }
}

/**
 * @brief Adds directory to the list of system directories to search for included files
 *
 * USAGE: -I, -inc, -include [directory path]
 *
 * @param args the arguments passed to the build process
 * @param index the index of the flag in the arguments list
 */
void Process::_include(std::vector<std::string>& args, size_t& index)
{
    EXPECT_TRUE_SS(index + 1 < args.size(), std::stringstream()
            << "Process::_include() - Missing include directory path.");
    std::string dpath = args[++index];

    // check if the include directory is valid
    EXPECT_TRUE_SS(Directory::valid_path(dpath), std::stringstream()
            << "Process::_include() - Invalid include directory path: " << dpath << ".");
    m_system_dirs.push_back(Directory(dpath));
}

/**
 * @brief Adds library to be linked with the compiled object files
 *
 * USAGE: -l, -lib, -library [library name].ba
 *
 * Specifically, it links to the static library [library name].ba
 *
 * @param args the arguments passed to the build process
 * @param index the index of the flag in the arguments list
 */
void Process::_library(std::vector<std::string>& args, size_t& index)
{
    EXPECT_TRUE_SS(index + 1 < args.size(), std::stringstream()
            << "Process::_library() - Missing library file path.");
    std::string fpath = args[++index];

    // check if the library name is valid
    EXPECT_TRUE_SS(File::valid_path(fpath), std::stringstream()
            << "Process::_library() - Invalid library file path: " << fpath << ".");
    m_linked_lib.push_back(File(fpath));
}

/**
 * @brief Adds directory to the list of directories to search for static libraries
 *
 * USAGE: -L, -libdir, -librarydir [directory path]
 *
 * @param args the arguments passed to the build process
 * @param index the index of the flag in the arguments list
 */
void Process::_library_directory(std::vector<std::string>& args, size_t& index)
{
    EXPECT_TRUE_SS(index + 1 < args.size(), std::stringstream()
            << "Process::_libraryDirectory() - Missing library directory path.");
    std::string dpath = args[++index];

    // check if the library directory is valid
    EXPECT_TRUE_SS(Directory::valid_path(dpath), std::stringstream()
            << "Process::_libraryDirectory() - Invalid library directory path: " << dpath << ".");
    m_library_dirs.push_back(Directory(dpath));
}

/**
 * @brief Adds a preprocessor flag
 *
 * USAGE: -D [flag name?=value]
 *
 * @param args the arguments passed to the build process
 * @param index the index of the flag in the arguments list
 */
void Process::_preprocessor_flag(std::vector<std::string>& args, size_t& index)
{
    EXPECT_TRUE_SS(index + 1 < args.size(), std::stringstream()
            << "Process::_preprocessorFlag() - Missing preprocessor flag.");
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

/**
 * @brief Don't delete processed files after preprocessing
 *
 * USAGE: -kp
 *
 * @param args the arguments passed to the build process
 * @param index the index of the flag in the arguments list
 */
void Process::_keep_processed_files(std::vector<std::string>& args, size_t& index)
{
    UNUSED(args);
    UNUSED(index);

    keep_proccessed_files = true;
}

void Process::_ld(std::vector<std::string>& args, size_t& index)
{
    EXPECT_TRUE_SS(index + 1 < args.size(), std::stringstream()
            << "Process::_ld() - Missing linker script file path.");
    std::string fpath = args[++index];

    // check if the library name is valid
    EXPECT_TRUE_SS(File::valid_path(fpath), std::stringstream()
            << "Process::_ld() - Invalid linker script file path: " << fpath << ".");
    m_ld_file = File(fpath);
}

void Process::_help(std::vector<std::string>& args, size_t& index)
{
    UNUSED (args);
    UNUSED (index);

    std::cout << "This is the BASM assembler. Usage:\n\n";

    std::cout << "basm [options] file...\n";
    std::cout << "Options:\n";

    auto print_option = [](const std::string& option, const std::string& description, int width = 20) {
        std::cout << std::left << std::setw(width) << option << description << '\n';
    };

    print_option ("--", "End of options.");
    print_option ("-v, -version", "Display assembler version information.");
    print_option ("-makelib", "Build a single library file (.ba).");
    print_option ("-c, -compile", "Only compiles the source files into object files.");
    print_option ("-o, -out, -output <file>", "Path to output file.");
    print_option ("-outdir <dir>", "Path to directory where object files will be stored.");
    print_option ("-O, -optimize=<level>", "Turns on optimization level. *UNIMPLEMENTED*");
    print_option ("-oall", "Turns on highest optimization level. *UNIMPLEMENTED*");
    print_option ("-W, -warning=<level>", "Turns on warning level. *UNIMPLEMENTED*");
    print_option ("-wall", "Turns on highest warning level. *UNIMPLEMENTED*");
    print_option ("-I, -inc, -include <dir>", "Add directory to search for system files from include macros.");
    print_option ("-l, -lib, -library <file>", "Links given library file to program.");
    print_option ("-L, -libdir, -librarydir <dir>", "Links all library files in the given directory recursively to program.");
    print_option ("-D <flag>", "Pass preprocessor flag to program.");
    print_option ("-kp", "Keep intermediate files (.bi).");
    print_option ("-h, -help", "Display options help.");
    exit (EXIT_SUCCESS);
}


// FOR NOW getters
bool Process::does_create_exe() const
{
    return !m_make_lib && !m_only_compile;
}

int Process::get_optimization_level() const
{
    return m_optimization_level;
}

std::set<std::string> Process::get_enabled_warnings() const
{
    return m_enabled_warnings;
}

std::map<std::string,std::string> Process::get_preprocessor_flags() const
{
    return m_preprocessor_flags;
}

std::vector<Directory> Process::get_system_dirs() const
{
    return m_system_dirs;
}

std::vector<File> Process::get_processed_files() const
{
    return m_processed_files;
}

std::vector<File> Process::get_obj_files() const
{
    return m_obj_files;
}

File Process::get_exe_file() const
{
    return m_exe_file;
}

File Process::get_ld_file() const
{
    return m_ld_file;
}

bool Process::has_ld_file() const
{
    return m_has_ld_file;
}