#pragma once

#include "util/directory.h"
#include "util/file.h"

#include <map>
#include <set>
#include <vector>

class Process;
static const std::string ASSEMBLER_VERSION = "0.0.1";
static const int MAX_OPTIMIZATION_LEVEL = 3;

static const std::string SOURCE_EXTENSION = "basm";
static const std::string INCLUDE_EXTENSION = "binc";
static const std::string PROCESSED_EXTENSION = "bi";
static const std::string OBJECT_EXTENSION = "bo";
static const std::string EXECUTABLE_EXTENSION = "bexe";
static const std::string STATIC_LIBRARY_EXTENSION = "ba";

static const std::set<std::string> WARNINGS = {
    "error",
};
static const std::string DEFAULT_OUTPUT_FILE = "a";

class Process
{
  public:
    static bool valid_src_file (const File &file);
    static bool valid_processed_file (const File &file);
    static bool valid_obj_file (const File &file);
    static bool valid_exe_file (const File &file);

    Process (const std::string &assembler_args = "");

    bool does_create_exe () const;
    int get_optimization_level () const;
    std::set<std::string> get_enabled_warnings () const;
    std::map<std::string, std::string> get_preprocessor_flags () const;

    std::vector<Directory> get_system_dirs () const;

    std::vector<File> get_processed_files () const;
    std::vector<File> get_obj_files () const;
    File get_exe_file () const;
    File get_ld_file () const;
    bool has_ld_file () const;

  private:
    /* process flags */
    bool m_parse_options = true;
    bool m_make_lib = false;
    bool m_only_compile = false;
    std::string m_output_file = DEFAULT_OUTPUT_FILE;
    int m_optimization_level = 0;
    std::set<std::string> m_enabled_warnings;
    std::map<std::string, std::string> m_preprocessor_flags;

    std::vector<File> m_linked_lib;
    std::vector<Directory> m_library_dirs;
    std::vector<Directory> m_system_dirs;
    std::vector<File> m_src_files;
    std::string m_output_dir = "";
    bool m_has_output_dir = false;
    bool keep_proccessed_files = false;

    File m_ld_file;
    bool m_has_ld_file = false;

    /* process files */
    std::vector<File> m_processed_files;
    std::vector<File> m_obj_files;
    File m_exe_file;

    void parse_args (std::string assembler_args, std::vector<std::string> &args_list);
    void evaluate_args (std::vector<std::string> &args_list);
    void build ();

    void preprocess ();
    void assemble ();
    void link ();

    void _ignore (std::vector<std::string> &args, size_t &index);
    void _version (std::vector<std::string> &args, size_t &index);
    void _compile (std::vector<std::string> &args, size_t &index);
    void _ar (std::vector<std::string> &args, size_t &index);
    void _output (std::vector<std::string> &args, size_t &index);
    void _outdir (std::vector<std::string> &args, size_t &index);
    void _optimize (std::vector<std::string> &args, size_t &index);
    void _optimize_all (std::vector<std::string> &args, size_t &index);
    void _warn (std::vector<std::string> &args, size_t &index);
    void _warn_all (std::vector<std::string> &args, size_t &index);
    void _include (std::vector<std::string> &args, size_t &index);
    void _library (std::vector<std::string> &args, size_t &index);
    void _library_directory (std::vector<std::string> &args, size_t &index);
    void _preprocessor_flag (std::vector<std::string> &args, size_t &index);
    void _keep_preprocessor_output (std::vector<std::string> &args, size_t &index);
    void _ld (std::vector<std::string> &args, size_t &index);
    void _help (std::vector<std::string> &args, size_t &index);

    typedef void (Process::*FlagFunction) (std::vector<std::string> &args, size_t &index);
    std::map<std::string, FlagFunction> flags;
};