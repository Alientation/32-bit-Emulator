#include "util/File.h"
#include "util/Directory.h"

#include <map>
#include <set>
#include <vector>


#ifndef BUILD_H
#define BUILD_H

class Process;
static const std::string ASSEMBLER_VERSION = "0.0.1";
static const int MAX_OPTIMIZATION_LEVEL = 3;

static const std::string SOURCE_EXTENSION = "basm";
static const std::string INCLUDE_EXTENSION = "binc";
static const std::string PROCESSED_EXTENSION = "bi";
static const std::string OBJECT_EXTENSION = "bo";
static const std::string EXECUTABLE_EXTENSION = "bexe";

static const std::set<std::string> WARNINGS = {
	"error",
};
static const std::string DEFAULT_OUTPUT_FILE = "a";

class Process {
	public:
		static bool valid_src_file(const File& file) {
			return file.get_extension() == SOURCE_EXTENSION || file.get_extension() == INCLUDE_EXTENSION;
		}

		static bool valid_processed_file(const File& file) {
			return file.get_extension() == PROCESSED_EXTENSION;
		}

		static bool valid_obj_file(const File& file) {
			return file.get_extension() == OBJECT_EXTENSION;
		}

		static bool valid_exe_file(const File& file) {
			return file.get_extension() == EXECUTABLE_EXTENSION;
		}


		Process(std::string assembler_args = "");

		void build();

        bool do_only_compile();
        std::string get_output_file();
        int get_optimization_level();
        std::set<std::string> get_enabled_warnings();
        std::map<std::string,std::string> get_preprocessor_flags();

        std::vector<std::string> get_linked_lib_names();
        std::vector<Directory> get_lib_dirs();
        std::vector<Directory> get_system_dirs();
        std::vector<File> get_src_files();

        std::vector<File> get_processed_files();
        std::vector<File> get_obj_files();
        File get_exe_file();

	private:
		bool m_only_compile = false;
		std::string m_output_file = DEFAULT_OUTPUT_FILE;
		int m_optimization_level;
		std::set<std::string> m_enabled_warnings;
		std::map<std::string,std::string> m_preprocessor_flags;

		std::vector<std::string> m_linked_lib_names;
		std::vector<Directory> m_library_dirs;
		std::vector<Directory> m_system_dirs;
		std::vector<File> m_src_files;

		std::vector<File> m_processed_files;
		std::vector<File> m_obj_files;
		File exe_file;

		void parse_args(std::string assembler_args, std::vector<std::string>& args_list);
		void evaluate_args(std::vector<std::string>& args_list);

		void preprocess();
		void assemble();
		void link();

        void _ignore(std::vector<std::string>& args, int& index);
		void _version(std::vector<std::string>& args, int& index);
		void _compile(std::vector<std::string>& args, int& index);
		void _output(std::vector<std::string>& args, int& index);
		void _optimize(std::vector<std::string>& args, int& index);
		void _optimize_all(std::vector<std::string>& args, int& index);
		void _warn(std::vector<std::string>& args, int& index);
		void _warn_all(std::vector<std::string>& args, int& index);
		void _include(std::vector<std::string>& args, int& index);
		void _library(std::vector<std::string>& args, int& index);
		void _library_directory(std::vector<std::string>& args, int& index);
		void _preprocessor_flag(std::vector<std::string>& args, int& index);

		typedef void (Process::*FlagFunction)(std::vector<std::string>& args, int& index);
		std::map<std::string, FlagFunction> flags = {
            {"--", &Process::_ignore},

			// prints out the version of the assembler
			{"-v", &Process::_version},
			{"-version", &Process::_version},

			// compiles the source code files to object files and stops
			{"-c", &Process::_compile},
			{"-compile", &Process::_compile},

			// specifies the name of the output file (the executable file)
			{"-o", &Process::_output},
			{"-out", &Process::_output},
			{"-output", &Process::_output},

			// turns on optimization
			{"-O", &Process::_optimize},
			{"-optimize", &Process::_optimize},

			// turns on all optimization
			{"-oall", &Process::_optimize_all},

			// turns on warning messages
			{"-W", &Process::_warn},
			{"-warning", &Process::_warn},

			// turns on all warning messages
			{"-wall", &Process::_warn_all},

			// use given directory for system files
			{"-I", &Process::_include},
			{"-inc", &Process::_include},
			{"-include", &Process::_include},

			// links given library into program
			{"-l", &Process::_library},
			{"-lib", &Process::_library},
			{"-library", &Process::_library},

			// searches for linked libraries in given directory
			{"-L", &Process::_library_directory},
			{"-libdir", &Process::_library_directory},
			{"-librarydir", &Process::_library_directory},

			// pass preprocessor flags
			{"-D", &Process::_preprocessor_flag},
		};

};


#endif