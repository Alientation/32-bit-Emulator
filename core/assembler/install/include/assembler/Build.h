#include "assembler/util/File.h"
#include "assembler/util/Directory.h"

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
		static bool isValidSourceFile(File* file) {
			return file->getExtension() == SOURCE_EXTENSION || file->getExtension() == INCLUDE_EXTENSION;
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


		Process(std::string assemblerArgs = "");
		~Process();

		void build();

        bool doOnlyCompile();
        std::string getOutputFile();
        int getOptimizationLevel();
        std::set<std::string> getEnabledWarnings();
        std::map<std::string,std::string> getPreprocessorFlags();

        std::vector<std::string> getLinkedLibraryNames();
        std::vector<Directory*> getLibraryDirectories();
        std::vector<Directory*> getSystemDirectories();
        std::vector<File*> getSourceFiles();

        std::vector<File*> getProcessedFiles();
        std::vector<File*> getObjectFiles();
        File* getExecutableFile();
        
	private:
		bool onlyCompile = false;
		std::string outputFile = DEFAULT_OUTPUT_FILE;
		int optimizationLevel;
		std::set<std::string> enabledWarnings;
		std::map<std::string,std::string> preprocessorFlags;

		std::vector<std::string> linkedLibraryNames;
		std::vector<Directory*> libraryDirectories;
		std::vector<Directory*> systemDirectories;
		std::vector<File*> sourceFiles;

		std::vector<File*> processedFiles;
		std::vector<File*> objectFiles;
		File* executableFile;

		void parseArgs(std::string assemblerArgs, std::vector<std::string>& argsList);
		void evaluateArgs(std::vector<std::string>& argsList);

		void preprocess();
		void assemble();
		void link();

        void _ignore(std::vector<std::string>& args, int& index);
		void _version(std::vector<std::string>& args, int& index);
		void _compile(std::vector<std::string>& args, int& index);
		void _output(std::vector<std::string>& args, int& index);
		void _optimize(std::vector<std::string>& args, int& index);
		void _optimizeAll(std::vector<std::string>& args, int& index);
		void _warn(std::vector<std::string>& args, int& index);
		void _warnAll(std::vector<std::string>& args, int& index);
		void _include(std::vector<std::string>& args, int& index);
		void _library(std::vector<std::string>& args, int& index);
		void _libraryDirectory(std::vector<std::string>& args, int& index);
		void _preprocessorFlag(std::vector<std::string>& args, int& index);

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
			{"-oall", &Process::_optimizeAll},
			
			// turns on warning messages
			{"-W", &Process::_warn},
			{"-warning", &Process::_warn},

			// turns on all warning messages
			{"-wall", &Process::_warnAll},

			// use given directory for system files
			{"-I", &Process::_include},
			{"-inc", &Process::_include},
			{"-include", &Process::_include},

			// links given library into program
			{"-l", &Process::_library},
			{"-lib", &Process::_library},
			{"-library", &Process::_library},

			// searches for linked libraries in given directory
			{"-L", &Process::_libraryDirectory},
			{"-libdir", &Process::_libraryDirectory},
			{"-librarydir", &Process::_libraryDirectory},

			// pass preprocessor flags
			{"-D", &Process::_preprocessorFlag},
		};

};


#endif