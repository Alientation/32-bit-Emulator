#include <map>
#include <vector>
#include "File.h"

#ifndef BUILD_H
#define BUILD_H

class Process;
static const std::string SOURCE_EXTENSION = "basm";
static const std::string PROCESSED_EXTENSION = "bi";
static const std::string OBJECT_EXTENSION = "bo";
static const std::string EXECUTABLE_EXTENSION = "bexe";

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

		void preprocess();
		void assemble();
		void link();

	private:
		std::vector<File*> systemFiles;
		std::vector<File*> sourceFiles;

		std::vector<File*> objectFiles;

		void _version();
		void _compile();
		void _output();
		void _optimize();
		void _optimizeAll();
		void _debug();
		void _warn();
		void _warnAll();
		void _obj();
		void _include();
		void _library();
		void _libraryDirectory();

		typedef void (Process::*FlagFunction)();
		std::map<std::string, FlagFunction> flags = {
			// prints out the version of the assembler
			{"-v", &Process::_version},
			{"-version", &Process::_version},

			// assembles the source code files to object files and stops
			{"-a", &Process::_compile},
			{"-assemble", &Process::_compile},

			// specifies the name of the output file
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
			{"-warnings", &Process::_warn},

			// turns on all warning messages
			{"-W", &Process::_warnAll},
			{"-wall", &Process::_warnAll},

			// use given directory for system files
			{"-i", &Process::_include},
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
		};

};


#endif