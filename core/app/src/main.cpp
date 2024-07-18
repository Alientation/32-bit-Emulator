#include "assembler/Assembler.h"
#include "assembler/Build.h"
#include "assembler/Linker.h"
#include "assembler/ObjectFile.h"
#include "assembler/Preprocessor.h"
#include "assembler/LoadExecutable.h"
#include "emulator32bit/Emulator32bit.h"
#include "util/Logger.h"
#include "util/File.h"
#include "util/Directory.h"

#include <filesystem>
#include <sstream>
#include <fstream>

/*
TODO

* improve vscode extension for basm language like autocomplete, syntax highlighting, etc
* 		- File icon theme in vscode extension
Figure share object files/dynamically linked libraries
* Create tests with the assembler to run longer pieces of code
* Implement software interrupts
*		- Print, logdump (print processor state)
Figure out IO, Disk, ports, etc
Add more relocation types, directives, preprocessors and build flags as needed
Implement .section directive and fix much of the hardcodedness of the assembler/object file/linker
*/

const std::string build_test = "-I .\\tests\\include -o .\\tests\\build\\test .\\tests\\src\\main.basm .\\tests\\src\\other.basm -outdir .\\tests\\build";
const std::string build_fibonacci = "-I .\\programs\\include -o .\\programs\\build\\fibonacci .\\programs\\src\\fibonacci.basm -outdir .\\programs\\build";
const std::string build_palindrome = "-I .\\programs\\include -o .\\programs\\build\\palindrome .\\programs\\src\\palindrome.basm -outdir .\\programs\\build";
const std::string build_library = "-o .\\programs\\build\\libtest -makelib .\\programs\\src\\palindrome.basm -outdir .\\programs\\build";
const std::string build_exe_from_library = "todo";

#define MAX_EXECUTED_INSTRUCTIONS 1000

int main(int argc, char* argv[]) {
    lgr::log(lgr::Logger::LogType::INFO, "Running Build");

	std::string build_command = build_exe_from_library;
	if (argc > 1) {
   	 lgr::log(lgr::Logger::LogType::INFO, "Parsing command arguments");
		build_command = "";
		for (int i = 1; i < argc; i++) {
			build_command += std::string(argv[i]);
			if (i+1 < argc) {
				build_command += " ";
			}
		}
	}

	Process process = Process(build_command);

	if (process.does_create_exe()) {
		Emulator32bit emulator;
		LoadExecutable loader(emulator, process.get_exe_file());

		emulator.run(MAX_EXECUTED_INSTRUCTIONS);
		emulator.print();
	}
}