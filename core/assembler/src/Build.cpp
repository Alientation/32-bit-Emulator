#include "assembler/Build.h"

#include "assembler/Assembler.h"
#include "assembler/Linker.h"
#include "assembler/ObjectFile.h"
#include "assembler/Preprocessor.h"
#include "assembler/StaticLibrary.h"
#include "util/Directory.h"
#include "util/Logger.h"
#include "util/StringUtil.h"

#include <filesystem>
#include <iostream>
#include <sstream>
#include <vector>

bool Process::valid_src_file(const File& file) {
	return file.get_extension() == SOURCE_EXTENSION || file.get_extension() == INCLUDE_EXTENSION;
}

bool Process::valid_processed_file(const File& file) {
	return file.get_extension() == PROCESSED_EXTENSION;
}

bool Process::valid_obj_file(const File& file) {
	return file.get_extension() == OBJECT_EXTENSION;
}

bool Process::valid_exe_file(const File& file) {
	return file.get_extension() == EXECUTABLE_EXTENSION;
}


/**
 * @brief Constructs a build process from the specified arguments.
 *
 * @param compilerArgs the arguments to construct the build process from
 */
Process::Process(const std::string& assembler_args) {
	lgr::log(lgr::Logger::LogType::LOG, std::stringstream() << "Building Process: args(" << assembler_args << ")\n"
			<< "Current Working Directory: " << std::filesystem::current_path().string());

	flags = {
		{"--", &Process::_ignore},

		{"-v", &Process::_version},										/* Prints version of assembler */
		{"-version", &Process::_version},

		{"-makelib", &Process::_makelib},								/* Instead of building an executable, create a collection of
																			object files and package into a single library file (.ba) */

		{"-c", &Process::_compile},										/* Only compiles the src code files into object files */
		{"-compile", &Process::_compile},

		{"-o", &Process::_output},										/* Path to output file (executable for builds, library files for makelib) */
		{"-out", &Process::_output},
		{"-output", &Process::_output},

		{"-outdir", &Process::_outdir},									/* Directory where all object files will be stored */

		{"-O", &Process::_optimize},									/* Turns on optimization level *unimplemented* */
		{"-optimize", &Process::_optimize},

		{"-oall", &Process::_optimize_all},								/* Highest optimization level *unimplemented* */

		{"-W", &Process::_warn},										/* Turns on warning level *unimplemented* */
		{"-warning", &Process::_warn},

		{"-wall", &Process::_warn_all},									/* Highest warning level *unimplemented* */

		{"-I", &Process::_include},										/* Adds directory to search for system files */
		{"-inc", &Process::_include},
		{"-include", &Process::_include},

		{"-l", &Process::_library},										/* Links given library file to program */
		{"-lib", &Process::_library},
		{"-library", &Process::_library},

		{"-L", &Process::_library_directory},							/* Searches for all libraries in given directory and links */
		{"-libdir", &Process::_library_directory},
		{"-librarydir", &Process::_library_directory},

		{"-D", &Process::_preprocessor_flag},							/* Passes preprocessor flags into the program */

		{"-kp", &Process::_keep_processed_files},						/* Don't delete intermediate files */
	};

	// split command args by whitespace unless surrounded by quotes
	std::vector<std::string> args_list;
	parse_args(assembler_args, args_list);

	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Process::Process() - args_list.size(): " << args_list.size());
	for (int i = 0; i < args_list.size(); i++) {
		lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Process::Process() - args_list[" << i << "]: " << args_list[i]);
	}

	evaluate_args(args_list);
	build();
}

/**
 * @brief Parses the arguments into a list of arguments.
 *
 * @param compilerArgs the arguments to parse
 * @param args_list the list of arguments to add to
 */
void Process::parse_args(std::string assembler_args, std::vector<std::string>& args_list) {
	bool is_escaped = false;
	bool is_quoted = false;
	std::string cur_arg = "";
	for (int i = 0; i < assembler_args.length(); i++) {
		char c = assembler_args[i];
		if (c == '\"' && !is_escaped) {
			// this is a quote that is not escaped
			is_quoted = !is_quoted;
		} else if (std::isspace(c) && !is_quoted) {
			// only add argument if it's not empty
			if (cur_arg.length() > 0) {
				args_list.push_back(cur_arg);
				cur_arg = "";
			}
		} else {
			// check if escape character
			if (c == '\\') {
				is_escaped = !is_escaped;
			} else {
				is_escaped = false;
			}
			cur_arg += c;
		}
	}

	// check if there are any dangling quotes or escape characters
	lgr::EXPECT_FALSE(is_quoted, lgr::Logger::LogType::ERROR, std::stringstream() << "Process::Process() - Missing end quotes: " << assembler_args);
	lgr::EXPECT_FALSE(is_escaped, lgr::Logger::LogType::ERROR, std::stringstream() << "Process::Process() - Dangling escape character: " << assembler_args);

	// add the last argument if it's not empty
	if (cur_arg.length() > 0) {
		args_list.push_back(cur_arg);
	}
}

/**
 * @brief Processes the arguments. This is an internal function.
 *
 * @param args_list the list of arguments to process
 */
void Process::evaluate_args(std::vector<std::string>& args_list) {
	// evaluate arguments
	for (int i = 0; i < args_list.size(); i++) {
		lgr::log(lgr::Logger::LogType::LOG, std::stringstream() << "arg" << i << ": " << args_list[i]);

		std::string& arg = args_list[i];
		if (arg[0] == '-') {
			// this is a flag
			lgr::EXPECT_TRUE(flags.find(arg) != flags.end(), lgr::Logger::LogType::ERROR, std::stringstream("Process::Process() - Invalid flag: ") << arg);

			(this->*flags[arg])(args_list, i);
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
 * @brief
 *
 */
void Process::build() {
	preprocess();
	assemble();

	if (m_make_lib) {
		WriteStaticLibrary(m_obj_files, File(m_output_file + "." + STATIC_LIBRARY_EXTENSION, true));
		return;
	}

	if (m_only_compile) {											/* Only compiles object files */
		return;
	}
	link();
}

/**
 * @brief
 *
 */
void Process::preprocess() {
	m_processed_files.clear();
	for (File file : m_src_files) {
		Preprocessor preprocessor(this, file);
		m_processed_files.push_back(preprocessor.preprocess());
	}
}

/**
 * @brief
 *
 */
void Process::assemble() {
	m_obj_files.clear();
	for (File file : m_processed_files) {
		if (m_has_output_dir) {
    		lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "FILE NAME = " << file.get_name());
			Assembler assembler(this, file, m_output_dir + File::SEPARATOR + file.get_name() + "." + OBJECT_EXTENSION);
			m_obj_files.push_back(assembler.assemble());
		} else {
			Assembler assembler(this, file);
			m_obj_files.push_back(assembler.assemble());
		}

		try {
			if (!std::filesystem::remove(file.get_path())) {
				std::cout << "file " << file.get_path() << " not found.\n";
			}
		} catch (const std::filesystem::filesystem_error& err) {
			std::cout << "filesystem error: " << err.what() << "\n";
		}
	}
}

/**
 * @brief
 *
 */
void Process::link() {
	std::vector<ObjectFile> objects;
	for (File file : m_obj_files) {
		objects.push_back(ObjectFile(file));
	}

	for (File lib : m_linked_lib) {
		ReadStaticLibrary(objects, lib);
	}

	m_exe_file = File(m_output_file + "." + EXECUTABLE_EXTENSION);
	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Process::link() - output file name: " << m_exe_file.get_path());
	Linker linker(objects, m_exe_file);
}

/**
 * @brief
 *
 * @param args
 * @param index
 */
void Process::_ignore(std::vector<std::string>& args, int& index) {
    // jumps index to the end of args
    index = args.size();
}

/**
 * @brief Prints out the version of the assembler
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
 * @brief
 *
 * @param args
 * @param index
 */
void Process::_makelib(std::vector<std::string>& args, int& index) {
	m_make_lib = true;
}

/**
 * @brief Compiles the source code files to object files and stops
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
 * @brief Sets the output file
 *
 * USAGE: -o, -output [filename]
 *
 * @param args the arguments passed to the build process
 * @param index the index of the flag in the arguments list
 */
void Process::_output(std::vector<std::string>& args, int& index) {
	lgr::EXPECT_TRUE(index + 1 < args.size(), lgr::Logger::LogType::ERROR, std::stringstream()
			<< "Process::_output() - Missing output file name");
	m_output_file = args[++index];

	// check if the output file is valid
	lgr::EXPECT_TRUE(File::valid_path(m_output_file), lgr::Logger::LogType::ERROR, std::stringstream()
			<< "Process::_output() - Invalid output file path: " << m_output_file);
}

/**
 * @brief Sets the output directory for all object files generated
 *
 * USAGE: -outdir [filename]
 *
 * @param args the arguments passed to the build process
 * @param index the index of the flag in the arguments list
 */
void Process::_outdir(std::vector<std::string>& args, int& index) {
	lgr::EXPECT_TRUE(index + 1 < args.size(), lgr::Logger::LogType::ERROR, std::stringstream() << "Process::_outdir() - Missing output file name");
	m_output_dir = args[++index];
	m_has_output_dir = true;
	// check if the output file is valid
	lgr::EXPECT_TRUE(Directory::valid_path(m_output_dir), lgr::Logger::LogType::ERROR, std::stringstream()
			<< "Process::_outdir() - Invalid output directory path: " << m_output_file);
}

/**
 * @brief Sets the optimization level
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
	lgr::EXPECT_TRUE(index + 1 < args.size(), lgr::Logger::LogType::ERROR, std::stringstream() << "Process::_optimize() - Missing optimization level");
	m_optimization_level = std::stoi(args[++index]);

	// check if the optimization level is valid
	lgr::EXPECT_TRUE(0 <= m_optimization_level && m_optimization_level <= 3, lgr::Logger::LogType::ERROR, std::stringstream()
			<< "Process::_optimize() - Invalid optimization level: " << m_optimization_level);
}

/**
 * @brief Sets the highest optimization level
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
 * @brief Turns on warning messages
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
	lgr::EXPECT_TRUE(index + 1 < args.size(), lgr::Logger::LogType::ERROR, std::stringstream() << "Process::_warn() - Missing warning type");
	std::string warning_type = args[++index];

	// check if the warning type is valid
	lgr::EXPECT_TRUE(WARNINGS.find(warning_type) != WARNINGS.end(), lgr::Logger::LogType::ERROR, std::stringstream()
			<< "Process::_warn() - Invalid warning type: " << warning_type);
	m_enabled_warnings.insert(warning_type);
}

/**
 * @brief Turns on all warning messages
 *
 * USAGE: -W, -wall
 *
 * @param args the arguments passed to the build process
 * @param index the index of the flag in the arguments list
 */
void Process::_warn_all(std::vector<std::string>& args, int& index) {
	for (std::string warning_type : WARNINGS) {
		m_enabled_warnings.insert(warning_type);
	}
}

/**
 * @brief Adds directory to the list of system directories to search for included files
 *
 * USAGE: -I, -inc, -include [directory path]
 *
 * @param args the arguments passed to the build process
 * @param index the index of the flag in the arguments list
 */
void Process::_include(std::vector<std::string>& args, int& index) {
	lgr::EXPECT_TRUE(index + 1 < args.size(), lgr::Logger::LogType::ERROR, std::stringstream()
			<< "Process::_include() - Missing include directory path");
	std::string include_dir = args[++index];

	// check if the include directory is valid
	lgr::EXPECT_TRUE(Directory::valid_path(include_dir), lgr::Logger::LogType::ERROR, std::stringstream()
			<< "Process::_include() - Invalid include directory path: " << include_dir);
	m_system_dirs.push_back(Directory(include_dir));
}

/**
 * @brief Adds shared library to be linked with the compiled object files
 *
 * USAGE: -l, -lib, -library [library name]
 *
 * Specifically, it links to the shared library [library name].so
 *
 * @param args the arguments passed to the build process
 * @param index the index of the flag in the arguments list
 */
void Process::_library(std::vector<std::string>& args, int& index) {
	lgr::EXPECT_TRUE(index + 1 < args.size(), lgr::Logger::LogType::ERROR, std::stringstream()
			<< "Process::_library() - Missing library name");
	std::string lib_path = args[++index];

	// check if the library name is valid
	lgr::EXPECT_TRUE(File::valid_path(lib_path), lgr::Logger::LogType::ERROR, std::stringstream()
			<< "Process::_library() - Invalid library path: " << lib_path);
	m_linked_lib.push_back(File(lib_path));
}

/**
 * @brief Adds directory to the list of directories to search for shared libraries
 *
 * USAGE: -L, -libdir, -librarydir [directory path]
 *
 * @param args the arguments passed to the build process
 * @param index the index of the flag in the arguments list
 */
void Process::_library_directory(std::vector<std::string>& args, int& index) {
	lgr::EXPECT_TRUE(index + 1 < args.size(), lgr::Logger::LogType::ERROR, std::stringstream()
			<< "Process::_libraryDirectory() - Missing library directory path");
	std::string lib_dir = args[++index];

	// check if the library directory is valid
	lgr::EXPECT_TRUE(Directory::valid_path(lib_dir), lgr::Logger::LogType::ERROR, std::stringstream()
			<< "Process::_libraryDirectory() - Invalid library directory path: " << lib_dir);
	m_library_dirs.push_back(Directory(lib_dir));
}

/**
 * @brief Adds a preprocessor flag
 *
 * USAGE: -D [flag name?=value]
 *
 * @param args the arguments passed to the build process
 * @param index the index of the flag in the arguments list
 */
void Process::_preprocessor_flag(std::vector<std::string>& args, int& index) {
	lgr::EXPECT_TRUE(index + 1 < args.size(), lgr::Logger::LogType::ERROR, std::stringstream()
			<< "Process::_preprocessorFlag() - Missing preprocessor flag");
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

/**
 * @brief Don't delete processed files after preprocessing
 *
 * USAGE: -kp
 *
 * @param args the arguments passed to the build process
 * @param index the index of the flag in the arguments list
 */
void Process::_keep_processed_files(std::vector<std::string>& args, int& index) {
	keep_proccessed_files = true;
}


// FOR NOW getters
bool Process::does_create_exe() const {
	return !m_make_lib && !m_only_compile;
}

int Process::get_optimization_level() const {
    return m_optimization_level;
}

std::set<std::string> Process::get_enabled_warnings() const {
    return m_enabled_warnings;
}

std::map<std::string,std::string> Process::get_preprocessor_flags() const {
    return m_preprocessor_flags;
}

std::vector<Directory> Process::get_system_dirs() const {
    return m_system_dirs;
}

std::vector<File> Process::get_processed_files() const {
    return m_processed_files;
}

std::vector<File> Process::get_obj_files() const {
    return m_obj_files;
}

File Process::get_exe_file() const {
    return m_exe_file;
}
