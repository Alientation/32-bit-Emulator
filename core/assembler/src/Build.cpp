#include "assembler/Build.h"
#include "assembler/Preprocessor.h"
#include "assembler/Assembler.h"
#include "assembler/ObjectFile.h"
#include "assembler/Linker.h"
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
	parse_args(assemblerArgs, argsList);

	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Process::Process() - argsList.size(): " << argsList.size());
	for (int i = 0; i < argsList.size(); i++) {
		lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Process::Process() - argsList[" << i << "]: " << argsList[i]);
	}

	evaluate_args(argsList);
}

/**
 * Parses the arguments into a list of arguments. This is an internal function.
 *
 * @param compilerArgs the arguments to parse
 * @param argsList the list of arguments to add to
 */
void Process::parse_args(std::string assemblerArgs, std::vector<std::string>& argsList) {
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
	lgr::EXPECT_FALSE(isQuoted, lgr::Logger::LogType::ERROR, std::stringstream() << "Process::Process() - Missing end quotes: " << assemblerArgs);
	lgr::EXPECT_FALSE(isEscaped, lgr::Logger::LogType::ERROR, std::stringstream() << "Process::Process() - Dangling escape character: " << assemblerArgs);

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
void Process::evaluate_args(std::vector<std::string>& argsList) {
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
			File file(arg);

			// check the extension
			lgr::EXPECT_TRUE(file.get_extension() == SOURCE_EXTENSION, lgr::Logger::LogType::ERROR,
					std::stringstream("Process::Process() - Invalid file extension: ") << file.get_extension());

			m_src_files.push_back(file);
		}
	}
}


/**
 * Builds the executable from the compile arguments
 */
void Process::build() {
	preprocess();
	assemble();
	link();
}

/**
 * Only preprocesses any source files
 */
void Process::preprocess() {
	m_processed_files.clear();
	for (File file : m_src_files) {
		Preprocessor preprocessor(this, file);
		m_processed_files.push_back(preprocessor.preprocess());
	}
}

/**
 * Only assembles any processed files
 */
void Process::assemble() {
	m_obj_files.clear();
	for (File file : m_processed_files) {
		Assembler assembler(this, file);
		m_obj_files.push_back(assembler.assemble());
	}
}

/**
 * Only links any object files
 */
void Process::link() {
	std::vector<ObjectFile> objects;
	for (File file : m_obj_files) {
		objects.push_back(ObjectFile(file));
	}
	exe_file = File(m_output_file + "." + EXECUTABLE_EXTENSION);
	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Process::link() - output file name: " << exe_file.get_path());
	Linker linker(objects, exe_file);
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
	m_only_compile = true;
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
	m_output_file = args[++index];

	// check if the output file is valid
	lgr::EXPECT_TRUE(File::valid_path(m_output_file), lgr::Logger::LogType::ERROR, std::stringstream("Process::_output() - Invalid output file path: ") << m_output_file);
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
	m_optimization_level = std::stoi(args[++index]);

	// check if the optimization level is valid
	lgr::EXPECT_TRUE(0 <= m_optimization_level && m_optimization_level <= 3, lgr::Logger::LogType::ERROR, std::stringstream("Process::_optimize() - Invalid optimization level: ") << m_optimization_level);
}

/**
 * Sets the highest optimization level
 *
 * USAGE: -O, -oall
 *
 * @param args the arguments passed to the build process
 * @param index the index of the flag in the arguments list
 */
void Process::_optimize_all(std::vector<std::string>& args, int& index) {
	m_optimization_level = MAX_OPTIMIZATION_LEVEL;
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
	m_enabled_warnings.insert(warningType);
}

/**
 * Turns on all warning messages
 *
 * USAGE: -W, -wall
 *
 * @param args the arguments passed to the build process
 * @param index the index of the flag in the arguments list
 */
void Process::_warn_all(std::vector<std::string>& args, int& index) {
	for (std::string warningType : WARNINGS) {
		m_enabled_warnings.insert(warningType);
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
	lgr::EXPECT_TRUE(Directory::valid_path(includeDir), lgr::Logger::LogType::ERROR, std::stringstream("Process::_include() - Invalid include directory path: ") << includeDir);
	m_system_dirs.push_back(Directory(includeDir));
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
	lgr::EXPECT_TRUE(File::valid_name(libraryName), lgr::Logger::LogType::ERROR, std::stringstream("Process::_library() - Invalid library name: ") << libraryName);
	m_linked_lib_names.push_back(libraryName);
}

/**
 * Adds directory to the list of directories to search for shared libraries
 *
 * USAGE: -L, -libdir, -librarydir [directory path]
 *
 * @param args the arguments passed to the build process
 * @param index the index of the flag in the arguments list
 */
void Process::_library_directory(std::vector<std::string>& args, int& index) {
	lgr::EXPECT_TRUE(index + 1 < args.size(), lgr::Logger::LogType::ERROR, std::stringstream("Process::_libraryDirectory() - Missing library directory path"));
	std::string libraryDir = args[++index];

	// check if the library directory is valid
	lgr::EXPECT_TRUE(Directory::valid_path(libraryDir), lgr::Logger::LogType::ERROR, std::stringstream("Process::_libraryDirectory() - Invalid library directory path: ") << libraryDir);
	m_library_dirs.push_back(Directory(libraryDir));
}

/**
 * Adds a preprocessor flag
 *
 * USAGE: -D [flag name?=value]
 *
 * @param args the arguments passed to the build process
 * @param index the index of the flag in the arguments list
 */
void Process::_preprocessor_flag(std::vector<std::string>& args, int& index) {
	lgr::EXPECT_TRUE(index + 1 < args.size(), lgr::Logger::LogType::ERROR, std::stringstream("Process::_preprocessorFlag() - Missing preprocessor flag"));
	std::string flag = args[++index];

	// check if there is a value
	std::string value = "";
	if (flag.find('=') != std::string::npos) {
		// there is a value
		value = flag.substr(flag.find('=') + 1);
		flag = flag.substr(0, flag.find('='));
	}

	m_preprocessor_flags[flag] = value;
}


// FOR NOW getters
bool Process::do_only_compile() {
    return m_only_compile;
}

std::string Process::get_output_file() {
    return m_output_file;
}

int Process::get_optimization_level() {
    return m_optimization_level;
}

std::set<std::string> Process::get_enabled_warnings() {
    return m_enabled_warnings;
}

std::map<std::string,std::string> Process::get_preprocessor_flags() {
    return m_preprocessor_flags;
}

std::vector<std::string> Process::get_linked_lib_names() {
    return m_linked_lib_names;
}

std::vector<Directory> Process::get_lib_dirs() {
    return m_library_dirs;
}

std::vector<Directory> Process::get_system_dirs() {
    return m_system_dirs;
}

std::vector<File> Process::get_src_files() {
    return m_src_files;
}

std::vector<File> Process::get_processed_files() {
    return m_processed_files;
}

std::vector<File> Process::get_obj_files() {
    return m_obj_files;
}

File Process::get_exe_file() {
    return exe_file;
}
