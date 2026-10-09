#pragma once

#include "assembler/object_file.h"
#include "assembler/options.h"
#include "assembler/tokenizer.h"
#include "util/directory.h"
#include "util/file.h"

#include <map>
#include <set>
#include <vector>

inline const std::string ASSEMBLER_VERSION = "0.0.1";

/// The kinds of warning that -W accepts.
inline const std::set<std::string> WARNINGS = {
    "error",
};
/// The name of the output (without the extension) when -o is not given.
inline const std::string DEFAULT_OUTPUT_FILE = "a";

/// The command line driver: preprocess -> assemble -> link.
///
/// The constructor only parses the arguments. run() does the build. Errors in either are fatal
/// (AEMU_FATAL), which the caller chooses how to handle (see util/logger.h, FatalAction).
class Build
{
  public:
    /// @param file a file
    /// @return whether it is a source (.basm) or header (.binc) file
    static bool valid_src_file(const File &file);

    /// @param file a file
    /// @return whether it is a preprocessed file (.bi)
    static bool valid_processed_file(const File &file);

    /// @param file a file
    /// @return whether it is an object file (.bo)
    static bool valid_obj_file(const File &file);

    /// @param file a file
    /// @return whether it is an executable (.bexe)
    static bool valid_exe_file(const File &file);

    /// The arguments as they are in argv (without the program name). A path can have spaces.
    ///
    /// @param args the arguments, one for each element
    explicit Build(const std::vector<std::string> &args);

    /// The arguments as one line, which is split on whitespace (unless it is in quotes).
    ///
    /// @param assembler_args the arguments
    explicit Build(const std::string &assembler_args = "");

    /// Splits a line of arguments on whitespace. Whitespace in "quotes" does not split, and a
    /// backslash makes the next character part of the argument.
    ///
    /// @param line the arguments in one line
    /// @return the arguments
    static std::vector<std::string> split_args(const std::string &line);

    /// Builds what the arguments asked for. Does nothing for --help and --version, which only
    /// print. Call once.
    void run();

    /// @return whether the arguments asked to build something, as opposed to --help or --version
    bool has_work() const;

    /// @return whether the build links an executable (not -c or -ar)
    bool does_create_exe() const;

    /// @return the kinds of warning that were turned on (-W, -wall)
    std::set<std::string> get_enabled_warnings() const;

    /// @return the preprocessor symbols and their values (-D)
    std::map<std::string, std::string> get_preprocessor_flags() const;

    /// @return the directories that `#include <"file">` is searched in (-I)
    std::vector<Directory> get_system_dirs() const;

    /// @return the preprocessed files (.bi), which exist after run() only with -kp
    std::vector<File> get_processed_files() const;

    /// @return the object files (.bo) that run() wrote
    std::vector<File> get_obj_files() const;

    /// @return the executable (.bexe) that run() linked
    File get_exe_file() const;

    /// @return the linker script (-ld)
    File get_ld_file() const;

    /// @return whether a linker script was given
    bool has_ld_file() const;

  private:
    // process flags
    bool m_parse_options = true;
    bool m_make_lib = false;
    bool m_only_compile = false;
    std::string m_output_file = DEFAULT_OUTPUT_FILE;
    std::set<std::string> m_enabled_warnings;
    std::map<std::string, std::string> m_preprocessor_flags;

    std::vector<File> m_linked_lib;
    std::vector<Directory> m_library_dirs;
    std::vector<Directory> m_system_dirs;
    std::vector<File> m_src_files;
    std::string m_output_dir = "";
    bool m_has_output_dir = false;
    bool keep_proccessed_files = false;
    bool m_dump = false;

    /// Cleared by --help and --version, which only print.
    bool m_has_work = true;

    File m_ld_file;
    bool m_has_ld_file = false;

    // process files
    std::vector<File> m_processed_files;

    /// Tokens of m_processed_files (same order), handed to the assembler so it does not lex the
    /// .bi files again.
    std::vector<basm::PreprocessedSource> m_preprocessed;
    std::vector<File> m_obj_files;

    /// The object files as the assembler made them, which are the ones that get linked. The files
    /// in m_obj_files are for the user (-c, -outdir), they are not read again.
    std::vector<ObjectFile> m_objects;
    File m_exe_file;

    /// Processes the arguments: each flag calls its handler, anything else is a source file.
    ///
    /// @param args_list the list of arguments to process
    void evaluate_args(std::vector<std::string> &args_list);

    /// Runs the stages that the flags asked for: preprocess, assemble, link.
    void build();

    /// @return the options for the preprocessor (-I, -D, -kp)
    PreprocessorOptions preprocessor_options() const;

    /// Preprocesses the source files.
    void preprocess();

    /// Assembles the preprocessed files into object files.
    void assemble();

    /// Links the object files, and the libraries, into the executable (or the library with -ar).
    void link();

    /// Prints an objdump-style listing of the files.
    ///
    /// @param files the object files and the executable
    static void dump(const std::vector<File> &files);

    // The handlers of the flags. Each one takes all the arguments and the index of its flag,
    // and leaves the index at the last argument it used, so that a value (`-o a.out`) is not read
    // as a source file.

    /// `--`: everything after it is a source file, even if it starts with a dash.
    void _ignore(std::vector<std::string> &args, size_t &index);

    /// `-v`, `--version`: prints the version and builds nothing.
    void _version(std::vector<std::string> &args, size_t &index);

    /// `-c`, `--compile`: stops after the object files.
    void _compile(std::vector<std::string> &args, size_t &index);

    /// `-ar`, `--archive`: packages the object files into a static library (.ba) instead of
    /// linking an executable.
    void _ar(std::vector<std::string> &args, size_t &index);

    /// `-o`, `--output <name>`: the name of the output, without the extension.
    void _output(std::vector<std::string> &args, size_t &index);

    /// `-outdir <directory>`: where the object files go.
    void _outdir(std::vector<std::string> &args, size_t &index);

    /// `-W`, `--warning <type>`: turns on a kind of warning. The only one is `error`, which turns
    /// warnings into errors.
    void _warn(std::vector<std::string> &args, size_t &index);

    /// `-wall`: turns on every kind of warning.
    void _warn_all(std::vector<std::string> &args, size_t &index);

    /// `-I`, `--include <directory>`: adds a directory to search for the files of
    /// `#include <"file">`.
    void _include(std::vector<std::string> &args, size_t &index);

    /// `-l`, `--library <file>`: links a static library (.ba) with the object files.
    void _library(std::vector<std::string> &args, size_t &index);

    /// `-L`, `--librarydir <directory>`: links every static library in the directory.
    void _library_directory(std::vector<std::string> &args, size_t &index);

    /// `-D <name[=value]>`: defines a preprocessor symbol, as if the source began with
    /// `#define name value`.
    void _preprocessor_flag(std::vector<std::string> &args, size_t &index);

    /// `-kp`: writes the preprocessed text of each source to a .bi file and keeps it.
    void _keep_preprocessor_output(std::vector<std::string> &args, size_t &index);

    /// `-ld`, `--linker-script <file>`: the linker script to use instead of the default layout.
    void _ld(std::vector<std::string> &args, size_t &index);

    /// `-dump`: prints a listing of every object file and the executable.
    void _dump(std::vector<std::string> &args, size_t &index);

    /// `-h`, `--help`: prints the options and builds nothing.
    void _help(std::vector<std::string> &args, size_t &index);

    /// A flag handler.
    typedef void (Build::*FlagFunction)(std::vector<std::string> &args, size_t &index);
    std::map<std::string, FlagFunction> flags;
};