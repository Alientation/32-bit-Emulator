#include "Build.h"
#include "StringUtil.h"
#include "Logger.h"

#include <iostream>
#include <vector>
#include <sstream>

/**
 * 
 */
Process::Process(std::string arg) {
	log(LOG, std::stringstream() << "Building Process \t args(" << arg << ")");

	std::vector<std::string> args = split(arg, "\\s+");
	for (int i = 0; i < args.size(); i++) {
		log(LOG, std::stringstream() << "arg" << i << ": " << arg);

		std::string& arg = args[i];
		if (arg[0] == '-') {
			// this is a flag

		} else {
			// this should be a file
			File* file = new File(arg);

			// check the extension
			if (file->getExtension() == SOURCE_EXTENSION) {
				sourceFiles.push_back(file);
			}else {
				log(ERROR, std::stringstream() << "Process::Process() - Invalid file extension: " << file->getExtension());
			}
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



void Process::_version() {

}

void Process::_compile() {

}

void Process::_output() {

}

void Process::_optimize() {

}

void Process::_optimizeAll() {

}

void Process::_debug() {

}

void Process::_warn() {

}

void Process::_warnAll() {

}

void Process::_obj() {

}

void Process::_include() {

}

void Process::_library() {

}

void Process::_libraryDirectory() {

}
