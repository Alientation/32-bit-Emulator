#include "assembler/Build.h"
#include "util/Directory.h"
#include "util/StringUtil.h"
#include "util/Logger.h"

#include <iostream>
#include <filesystem>
#include <sstream>
#include <vector>

/**
 * Constructs a build process from the specified arguments.
 * 
 * @param compilerArgs the arguments to construct the build process from
 */
Process::Process(std::string assemblerArgs) {
	lgr::log(lgr::Logger::LogType::LOG, std::stringstream() << "Building Process: args(" << assemblerArgs << ")\n" 
			<< "Current Working Directory: " << std::filesystem::current_path().string());

	// split command args by whitespace unless surrounded by quotes
	std::vector<std::string> argsList;
	parseArgs(assemblerArgs, argsList);

	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Process::Process() - argsList.size(): " << argsList.size());
	for (int i = 0; i < argsList.size(); i++) {
		lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Process::Process() - argsList[" << i << "]: " << argsList[i]);
	}

	evaluateArgs(argsList);
}

/**
 * Parses the arguments into a list of arguments. This is an internal function.
 * 
 * @param compilerArgs the arguments to parse
 * @param argsList the list of arguments to add to
 */
void Process::parseArgs(std::string assemblerArgs, std::vector<std::string>& argsList) {
	bool isEscaped = false;
	bool isQuoted = false;
	std::string curArg = "";
	for (int i = 0; i < assemblerArgs.length(); i++) {
		char c = assemblerArgs[i];
		if (c == '\"' && !isEscaped) {
			// this is a quote that is not escaped
			isQuoted = !isQuoted;
		} else if (std::isspace(c) && !isQuoted) {
			// only add argument if it's not empty
			if (curArg.length() > 0) {
				argsList.push_back(curArg);
				curArg = "";
			}
		} else {
			// check if escape character
			if (c == '\\') {
				isEscaped = !isEscaped;
			} else {
				isEscaped = false;
			}
			curArg += c;
		}
	}

	// check if there are any dangling quotes or escape characters
	lgr::EXPECT_FALSE(isQuoted, lgr::Logger::LogType::ERROR, std::stringstream("Process::Process() - Missing end quotes: ") << assemblerArgs);
	lgr::EXPECT_FALSE(isEscaped, lgr::Logger::LogType::ERROR, std::stringstream("Process::Process() - Dangling escape character: ") << assemblerArgs);

	// add the last argument if it's not empty
	if (curArg.length() > 0) {
		argsList.push_back(curArg);
	}
}

/**
 * Processes the arguments. This is an internal function.
 * 
 * @param argsList the list of arguments to process
 */
void Process::evaluateArgs(std::vector<std::string>& argsList) {
	// evaluate arguments
	for (int i = 0; i < argsList.size(); i++) {
		lgr::log(lgr::Logger::LogType::LOG, std::stringstream() << "arg" << i << ": " << argsList[i]);

		std::string& arg = argsList[i];
		if (arg[0] == '-') {
			// this is a flag
			lgr::EXPECT_TRUE(flags.find(arg) != flags.end(), lgr::Logger::LogType::ERROR, std::stringstream("Process::Process() - Invalid flag: ") << arg);

			(this->*flags[arg])(argsList, i);
		} else {
			// this should be a file
			File* file = new File(arg);

			// check the extension
			lgr::EXPECT_TRUE(file->getExtension() == SOURCE_EXTENSION, lgr::Logger::LogType::ERROR, std::stringstream("Process::Process() - Invalid file extension: ") << file->getExtension());

			sourceFiles.push_back(file);
		}
	}
}

/**
 * Destructs a build process.
 */
Process::~Process() {
    // delete all the files
    for (File* file : sourceFiles) {
        delete file;
    }
    for (File* file : processedFiles) {
        delete file;
    }
    for (File* file : objectFiles) {
        delete file;
    }

    // elete executableFile;

    // delete all the directories
    for (Directory* directory : libraryDirectories) {
        delete directory;
    }
    for (Directory* directory : systemDirectories) {
        delete directory;
    }
}


/**
 * Builds the executable from the compile arguments
 */
void Process::build() {

}

/**
 * Only preprocesses any source files
 */
void Process::preprocess() {

}

/**
 * Only assembles any processed files
 */
void Process::assemble() {

}

/**
 * Only links any object files
 */
void Process::link() {

}


void Process::_ignore(std::vector<std::string>& args, int& index) {
    // jumps index to the end of args
    index = args.size();
}

/**
 * Prints out the version of the assembler
 * 
 * USAGE: -v, -version
 * 
 * @param args the arguments passed to the build process
 * @param index the index of the flag in the arguments list
 */
void Process::_version(std::vector<std::string>& args, int& index) {
	std::cout << "Assembler Version: " << ASSEMBLER_VERSION << std::endl;
}

/**
 * Compiles the source code files to object files and stops
 * 
 * USAGE: -c, -compile
 * 
 * @param args the arguments passed to the build process
 * @param index the index of the flag in the arguments list
 */
void Process::_compile(std::vector<std::string>& args, int& index) {
	onlyCompile = true;
}

/**
 * Sets the output file
 * 
 * USAGE: -o, -output [filename]
 * 
 * @param args the arguments passed to the build process
 * @param index the index of the flag in the arguments list
 */
void Process::_output(std::vector<std::string>& args, int& index) {
	lgr::EXPECT_TRUE(index + 1 < args.size(), lgr::Logger::LogType::ERROR, std::stringstream("Process::_output() - Missing output file name"));
	outputFile = args[++index];

	// check if the output file is valid
	lgr::EXPECT_TRUE(File::isValidFileName(outputFile), lgr::Logger::LogType::ERROR, std::stringstream("Process::_output() - Invalid output file name: ") << outputFile);
}

/**
 * Sets the optimization level
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
void Process::_optimize(std::vector<std::string>& args, int& index) {
	lgr::EXPECT_TRUE(index + 1 < args.size(), lgr::Logger::LogType::ERROR, std::stringstream("Process::_optimize() - Missing optimization level"));
	optimizationLevel = std::stoi(args[++index]);

	// check if the optimization level is valid
	lgr::EXPECT_TRUE(0 <= optimizationLevel && optimizationLevel <= 3, lgr::Logger::LogType::ERROR, std::stringstream("Process::_optimize() - Invalid optimization level: ") << optimizationLevel);
}

/**
 * Sets the highest optimization level
 * 
 * USAGE: -O, -oall
 * 
 * @param args the arguments passed to the build process
 * @param index the index of the flag in the arguments list
 */
void Process::_optimizeAll(std::vector<std::string>& args, int& index) {
	optimizationLevel = MAX_OPTIMIZATION_LEVEL;
}

/**
 * Turns on warning messages
 * 
 * USAGE: -w, -warning [type]
 * 
 * Warning Types
 * error - turns warnings into errors
 * 
 * @param args the arguments passed to the build process
 * @param index the index of the flag in the arguments list
 */
void Process::_warn(std::vector<std::string>& args, int& index) {
	lgr::EXPECT_TRUE(index + 1 < args.size(), lgr::Logger::LogType::ERROR, std::stringstream("Process::_warn() - Missing warning type"));
	std::string warningType = args[++index];

	// check if the warning type is valid
	lgr::EXPECT_TRUE(WARNINGS.find(warningType) != WARNINGS.end(), lgr::Logger::LogType::ERROR, std::stringstream("Process::_warn() - Invalid warning type: ") << warningType);
	enabledWarnings.insert(warningType);
}

/**
 * Turns on all warning messages
 * 
 * USAGE: -W, -wall
 * 
 * @param args the arguments passed to the build process
 * @param index the index of the flag in the arguments list
 */
void Process::_warnAll(std::vector<std::string>& args, int& index) {
	for (std::string warningType : WARNINGS) {
		enabledWarnings.insert(warningType);
	}
}

/**
 * Adds directory to the list of system directories to search for included files
 * 
 * USAGE: -I, -inc, -include [directory path]
 * 
 * @param args the arguments passed to the build process
 * @param index the index of the flag in the arguments list
 */
void Process::_include(std::vector<std::string>& args, int& index) {
	lgr::EXPECT_TRUE(index + 1 < args.size(), lgr::Logger::LogType::ERROR, std::stringstream("Process::_include() - Missing include directory path"));
	std::string includeDir = args[++index];

	// check if the include directory is valid
	lgr::EXPECT_TRUE(Directory::isValidDirectoryPath(includeDir), lgr::Logger::LogType::ERROR, std::stringstream("Process::_include() - Invalid include directory path: ") << includeDir);
	systemDirectories.push_back(new Directory(includeDir));
}

/**
 * Adds shared library to be linked with the compiled object files
 * 
 * USAGE: -l, -lib, -library [library name]
 * 
 * Specifically, it links to the shared library [library name].so
 * 
 * @param args the arguments passed to the build process
 * @param index the index of the flag in the arguments list
 */
void Process::_library(std::vector<std::string>& args, int& index) {
	lgr::EXPECT_TRUE(index + 1 < args.size(), lgr::Logger::LogType::ERROR, std::stringstream("Process::_library() - Missing library name"));
	std::string libraryName = args[++index];

	// check if the library name is valid
	lgr::EXPECT_TRUE(File::isValidFileName(libraryName), lgr::Logger::LogType::ERROR, std::stringstream("Process::_library() - Invalid library name: ") << libraryName);
	linkedLibraryNames.push_back(libraryName);
}

/**
 * Adds directory to the list of directories to search for shared libraries
 * 
 * USAGE: -L, -libdir, -librarydir [directory path]
 * 
 * @param args the arguments passed to the build process
 * @param index the index of the flag in the arguments list
 */
void Process::_libraryDirectory(std::vector<std::string>& args, int& index) {
	lgr::EXPECT_TRUE(index + 1 < args.size(), lgr::Logger::LogType::ERROR, std::stringstream("Process::_libraryDirectory() - Missing library directory path"));
	std::string libraryDir = args[++index];

	// check if the library directory is valid
	lgr::EXPECT_TRUE(Directory::isValidDirectoryPath(libraryDir), lgr::Logger::LogType::ERROR, std::stringstream("Process::_libraryDirectory() - Invalid library directory path: ") << libraryDir);
	libraryDirectories.push_back(new Directory(libraryDir));
}

/**
 * Adds a preprocessor flag
 * 
 * USAGE: -D [flag name?=value]
 * 
 * @param args the arguments passed to the build process
 * @param index the index of the flag in the arguments list
 */
void Process::_preprocessorFlag(std::vector<std::string>& args, int& index) {
	lgr::EXPECT_TRUE(index + 1 < args.size(), lgr::Logger::LogType::ERROR, std::stringstream("Process::_preprocessorFlag() - Missing preprocessor flag"));
	std::string flag = args[++index];

	// check if there is a value
	std::string value = "";
	if (flag.find('=') != std::string::npos) {
		// there is a value
		value = flag.substr(flag.find('=') + 1);
		flag = flag.substr(0, flag.find('='));
	}

	preprocessorFlags[flag] = value;
}


// FOR NOW getters
bool Process::doOnlyCompile() {
    return onlyCompile;
}

std::string Process::getOutputFile() {
    return outputFile;
}

int Process::getOptimizationLevel() {
    return optimizationLevel;
}

std::set<std::string> Process::getEnabledWarnings() {
    return enabledWarnings;
}

std::map<std::string,std::string> Process::getPreprocessorFlags() {
    return preprocessorFlags;
}

std::vector<std::string> Process::getLinkedLibraryNames() {
    return linkedLibraryNames;
}

std::vector<Directory*> Process::getLibraryDirectories() {
    return libraryDirectories;
}

std::vector<Directory*> Process::getSystemDirectories() {
    return systemDirectories;
}

std::vector<File*> Process::getSourceFiles() {
    return sourceFiles;
}

std::vector<File*> Process::getProcessedFiles() {
    return processedFiles;
}

std::vector<File*> Process::getObjectFiles() {
    return objectFiles;
}

File* Process::getExecutableFile() {
    return executableFile;
}
