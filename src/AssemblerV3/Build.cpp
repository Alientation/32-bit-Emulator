#include "Build.h"
#include "StringUtil.h"
#include "Logger.h"

#include <iostream>
#include <vector>
#include <sstream>
#include <filesystem>

/**
 * Constructs a build process from the specified arguments.
 * 
 * @param compilerArgs the arguments to construct the build process from
 */
Process::Process(std::string assemblerArgs) {
	log(LOG, std::stringstream() << "Building Process: args(" << assemblerArgs << ")\n" 
			<< "Current Working Directory: " << std::filesystem::current_path().string());

	// split command args by whitespace unless surrounded by quotes
	std::vector<std::string> argsList;
	parseArgs(assemblerArgs, argsList);

	log(DEBUG, std::stringstream() << "Process::Process() - argsList.size(): " << argsList.size());
	for (int i = 0; i < argsList.size(); i++) {
		log(DEBUG, std::stringstream() << "Process::Process() - argsList[" << i << "]: " << argsList[i]);
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
	if (isQuoted) {
		log(ERROR, std::stringstream() << "Process::Process() - Missing end quotes: " << assemblerArgs);
	} else if (isEscaped) {
		log(ERROR, std::stringstream() << "Process::Process() - Dangling escape character: " << assemblerArgs);
	}

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
		log(LOG, std::stringstream() << "arg" << i << ": " << argsList[i]);

		std::string& arg = argsList[i];
		if (arg[0] == '-') {
			// this is a flag
			if (flags.find(arg) == flags.end()) {
				log(ERROR, std::stringstream() << "Process::Process() - Invalid flag: " << arg);
			}

			(this->*flags[arg])(argsList, i);
		} else {
			// this should be a file
			File* file = new File(arg);

			// check the extension
			if (file->getExtension() != SOURCE_EXTENSION) {
				log(ERROR, std::stringstream() << "Process::Process() - Invalid file extension: " << file->getExtension());
			}

			sourceFiles.push_back(file);
		}
	}
}

/**
 * Destructs a build process.
 */
Process::~Process() {

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
	if (index + 1 >= args.size()) {
		log(ERROR, std::stringstream() << "Process::_output() - Missing output file name");
		return;
	}

	outputFile = args[++index];

	// check if the output file is valid
	if (!File::isValidFileName(outputFile)) {
		log(ERROR, std::stringstream() << "Process::_output() - Invalid output file name: " << outputFile);
		return;
	}
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
	if (index + 1 >= args.size()) {
		log(ERROR, std::stringstream() << "Process::_optimize() - Missing optimization level");
		return;
	}

	optimizationLevel = std::stoi(args[++index]);

	// check if the optimization level is valid
	if (optimizationLevel < 0 || optimizationLevel > 3) {
		log(ERROR, std::stringstream() << "Process::_optimize() - Invalid optimization level: " << optimizationLevel);
		return;
	}
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
	if (index + 1 >= args.size()) {
		log(ERROR, std::stringstream() << "Process::_warn() - Missing warning type");
		return;
	}

	std::string warningType = args[++index];
	if (WARNINGS.find(warningType) == WARNINGS.end()) {
		log(ERROR, std::stringstream() << "Process::_warn() - Invalid warning type: " << warningType);
		return;
	}

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
	if (index + 1 >= args.size()) {
		log(ERROR, std::stringstream() << "Process::_include() - Missing include directory path");
		return;
	}

	std::string includeDir = args[++index];
	if (!Directory::isValidDirectoryPath(includeDir)) {
		log(ERROR, std::stringstream() << "Process::_include() - Invalid include directory path: " << includeDir);
		return;
	}

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
	if (index + 1 >= args.size()) {
		log(ERROR, std::stringstream() << "Process::_library() - Missing library name");
		return;
	}

	std::string libraryName = args[++index];
	if (!File::isValidFileName(libraryName)) {
		log(ERROR, std::stringstream() << "Process::_library() - Invalid library name: " << libraryName);
		return;
	}

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
	if (index + 1 >= args.size()) {
		log(ERROR, std::stringstream() << "Process::_libraryDirectory() - Missing library directory path");
		return;
	}

	std::string libraryDir = args[++index];
	if (!Directory::isValidDirectoryPath(libraryDir)) {
		log(ERROR, std::stringstream() << "Process::_libraryDirectory() - Invalid library directory path: " << libraryDir);
		return;
	}

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
	if (index + 1 >= args.size()) {
		log(ERROR, std::stringstream() << "Process::_preprocessorFlag() - Missing preprocessor flag");
		return;
	}

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