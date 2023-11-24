#include "Build.h"
#include "StringUtil.h"
#include "Logger.h"

#include <iostream>
#include <vector>
#include <sstream>
#include <filesystem>

/**
 * 
 */
Process::Process(std::string arg) {
	log(LOG, std::stringstream() << "Building Process \t args(" << arg << ")\n" 
			<< "Current Working Directory: " << std::filesystem::current_path().string());

	// split command args by whitespace unless surrounded by quotes
	std::vector<std::string> args;
	bool isEscaped = false;
	bool isQuoted = false;
	std::string curArg = "";
	for (int i = 0; i < arg.length(); i++) {
		char c = arg[i];
		if (c == '\"' && !isEscaped) {
			// this is a quote that is not escaped
			isQuoted = !isQuoted;
		} else if (std::isspace(c) && !isQuoted) {
			// only add argument if it's not empty
			if (curArg.length() > 0) {
				args.push_back(curArg);
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

	// parse arguments
	for (int i = 0; i < args.size(); i++) {
		log(LOG, std::stringstream() << "arg" << i << ": " << arg);

		std::string& arg = args[i];
		if (arg[0] == '-') {
			// this is a flag
			if (flags.find(arg) == flags.end()) {
				log(ERROR, std::stringstream() << "Process::Process() - Invalid flag: " << arg);
			}

			(this->*flags[arg])(args, i);
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
 * 
 */
Process::~Process() {

}



void Process::build() {

}

void Process::preprocess() {

}

void Process::assemble() {

}

void Process::link() {

}



void Process::_version(std::vector<std::string>& args, int& index) {

}

void Process::_compile(std::vector<std::string>& args, int& index) {

}

void Process::_output(std::vector<std::string>& args, int& index) {

}

void Process::_optimize(std::vector<std::string>& args, int& index) {

}

void Process::_optimizeAll(std::vector<std::string>& args, int& index) {

}

void Process::_debug(std::vector<std::string>& args, int& index) {

}

void Process::_warn(std::vector<std::string>& args, int& index) {

}

void Process::_warnAll(std::vector<std::string>& args, int& index) {

}

void Process::_obj(std::vector<std::string>& args, int& index) {

}

void Process::_include(std::vector<std::string>& args, int& index) {

}

void Process::_library(std::vector<std::string>& args, int& index) {

}

void Process::_libraryDirectory(std::vector<std::string>& args, int& index) {

}
