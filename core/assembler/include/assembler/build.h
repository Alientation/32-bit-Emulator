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

inline const std::set<std::string> WARNINGS = {
    "error",
};
inline const std::string DEFAULT_OUTPUT_FILE = "a";

/// The command line driver: preprocess -> assemble -> link.
///
/// The constructor only parses the arguments. run() does the build. Errors in either are fatal
/// (AEMU_FATAL), which the caller chooses how to handle (see util/logger.h, FatalAction).
class Build
{
  public:
    static bool valid_src_file(const File &file);
    static bool valid_processed_file(const File &file);
    static bool valid_obj_file(const File &file);
    static bool valid_exe_file(const File &file);

    /// The arguments as they are in argv (without the program name). A path can have spaces.
    explicit Build(const std::vector<std::string> &args);

    /// The arguments as one line, which is split on whitespace (unless it is in quotes).
    explicit Build(const std::string &assembler_args = "");

    /// Splits a line of arguments on whitespace. Whitespace in "quotes" does not split, and a
    /// backslash makes the next character part of the argument.
    static std::vector<std::string> split_args(const std::string &line);

    /// Builds what the arguments asked for. Does nothing for --help and --version, which only
    /// print. Call once.
    void run();

    /// Whether the arguments asked to build something, as opposed to --help or --version.
    bool has_work() const;

    bool does_create_exe() const;
    std::set<std::string> get_enabled_warnings() const;
    std::map<std::string, std::string> get_preprocessor_flags() const;

    std::vector<Directory> get_system_dirs() const;

    std::vector<File> get_processed_files() const;
    std::vector<File> get_obj_files() const;
    File get_exe_file() const;
    File get_ld_file() const;
    bool has_ld_file() const;

  private:
    /* process flags */
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

    /* process files */
    std::vector<File> m_processed_files;

    /// Tokens of m_processed_files (same order), handed to the assembler so it does not lex the
    /// .bi files again.
    std::vector<basm::PreprocessedSource> m_preprocessed;
    std::vector<File> m_obj_files;

    /// The object files as the assembler made them, which are the ones that get linked. The files
    /// in m_obj_files are for the user (-c, -outdir), they are not read again.
    std::vector<ObjectFile> m_objects;
    File m_exe_file;

    void evaluate_args(std::vector<std::string> &args_list);
    void build();
    PreprocessorOptions preprocessor_options() const;

    void preprocess();
    void assemble();
    void link();
    static void dump(const std::vector<File> &files);

    void _ignore(std::vector<std::string> &args, size_t &index);
    void _version(std::vector<std::string> &args, size_t &index);
    void _compile(std::vector<std::string> &args, size_t &index);
    void _ar(std::vector<std::string> &args, size_t &index);
    void _output(std::vector<std::string> &args, size_t &index);
    void _outdir(std::vector<std::string> &args, size_t &index);
    void _warn(std::vector<std::string> &args, size_t &index);
    void _warn_all(std::vector<std::string> &args, size_t &index);
    void _include(std::vector<std::string> &args, size_t &index);
    void _library(std::vector<std::string> &args, size_t &index);
    void _library_directory(std::vector<std::string> &args, size_t &index);
    void _preprocessor_flag(std::vector<std::string> &args, size_t &index);
    void _keep_preprocessor_output(std::vector<std::string> &args, size_t &index);
    void _ld(std::vector<std::string> &args, size_t &index);
    void _dump(std::vector<std::string> &args, size_t &index);
    void _help(std::vector<std::string> &args, size_t &index);

    typedef void (Build::*FlagFunction)(std::vector<std::string> &args, size_t &index);
    std::map<std::string, FlagFunction> flags;
};