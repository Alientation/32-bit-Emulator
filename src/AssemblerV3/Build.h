#include <map>
#include <set>
#include <vector>
#include "File.h"
#include "Directory.h"

#ifndef BUILD_H
#define BUILD_H

class Process;
static const std::string ASSEMBLER_VERSION = "0.0.1";
static const int MAX_OPTIMIZATION_LEVEL = 3;

static const std::string SOURCE_EXTENSION = "basm";
static const std::string PROCESSED_EXTENSION = "bi";
static const std::string OBJECT_EXTENSION = "bo";
static const std::string EXECUTABLE_EXTENSION = "bexe";

static const std::set<std::string> WARNINGS = {
	"error",
};
static const std::string DEFAULT_OUTPUT_FILE = "a";

class Process {
	public:
		static bool isValidSourceFile(File* file) {
			return file->getExtension() == SOURCE_EXTENSION;
		}

		static bool isValidProcessedFile(File* file) {
			return file->getExtension() == PROCESSED_EXTENSION;
		}

		static bool isValidObjectFile(File* file) {
			return file->getExtension() == OBJECT_EXTENSION;
		}

		static bool isValidExecutableFile(File* file) {
			return file->getExtension() == EXECUTABLE_EXTENSION;
		}


		Process(std::string arg = "");
		~Process();

		void build();

	private:
		bool onlyCompile = false;
		std::string outputFile = DEFAULT_OUTPUT_FILE;
		int optimizationLevel;
		bool enableDebugMode;
		std::set<std::string> enabledWarnings;

		std::vector<Directory*> systemDirectories;
		std::vector<File*> sourceFiles;

		std::vector<File*> processedFiles;
		std::vector<File*> objectFiles;
		File* executableFile;

		void preprocess();
		void assemble();
		void link();

		void _version(std::vector<std::string>& args, int& index);
		void _compile(std::vector<std::string>& args, int& index);
		void _output(std::vector<std::string>& args, int& index);
		void _optimize(std::vector<std::string>& args, int& index);
		void _optimizeAll(std::vector<std::string>& args, int& index);
		void _debug(std::vector<std::string>& args, int& index);
		void _warn(std::vector<std::string>& args, int& index);
		void _warnAll(std::vector<std::string>& args, int& index);
		void _include(std::vector<std::string>& args, int& index);
		void _library(std::vector<std::string>& args, int& index);
		void _libraryDirectory(std::vector<std::string>& args, int& index);
		void _preprocessorFlag(std::vector<std::string>& args, int& index);

		typedef void (Process::*FlagFunction)(std::vector<std::string>& args, int& index);
		std::map<std::string, FlagFunction> flags = {
			// prints out the version of the assembler
			{"-v", &Process::_version},
			{"-version", &Process::_version},

			// compiles the source code files to object files and stops
			{"-c", &Process::_compile},
			{"-compile", &Process::_compile},

			// specifies the name of the output file (the executable file)
			{"-out", &Process::_output},
			{"-output", &Process::_output},

			// turns on optimization
			{"-o", &Process::_optimize},
			{"-optimize", &Process::_optimize},

			// turns on all optimization
			{"-O", &Process::_optimizeAll},
			{"-oall", &Process::_optimizeAll},

			// turns on code generating options for debugging
			{"-g", &Process::_debug},
			{"-debug", &Process::_debug},
			
			// turns on warning messages
			{"-w", &Process::_warn},
			{"-warning", &Process::_warn},

			// turns on all warning messages
			{"-W", &Process::_warnAll},
			{"-wall", &Process::_warnAll},

			// use given directory for system files
			{"-I", &Process::_include},
			{"-inc", &Process::_include},
			{"-include", &Process::_include},

			// links given library into program
			{"-l", &Process::_library},
			{"-lib", &Process::_library},
			{"-library", &Process::_library},

			// searches for additional libraries in given directory
			{"-L", &Process::_libraryDirectory},
			{"-libdir", &Process::_libraryDirectory},
			{"-librarydir", &Process::_libraryDirectory},

			// pass preprocessor flags
			{"-D", &Process::_preprocessorFlag},
		};

};


#endif