#include "assembler/assembler.h"
#include "assembler/build.h"
#include "assembler/linker.h"
#include "assembler/load_executable.h"
#include "assembler/object_file.h"
#include "assembler/preprocessor.h"
#include "emulator32bit/emulator32bit.h"
#include "emulator32bit/disk.h"
#include "util/directory.h"
#include "util/file.h"
#include "util/loggerv2.h"

#include <filesystem>
#include <fstream>
#include <sstream>

/*
TODO

* CLEAN UP CODE
*	- Split long functions into smaller unit sized ones, especially if they can be reused elsewhere
*	- Macro preprocessors should follow the standard ALL_CAPS_WITH_UNDERSCORE naming conventions
		(DEFINITIONS SHOULD COME WITH THE PROJECT NAME PREPENDED BEFORE)
*	- Comment functions and complex logic
*	- Make variables that have larger scopes have more meaningful names
*	- FIX TODO (emulator32bit library)
*		- ////Disk.h + Disk.cpp
*		- ////FreeBlockList.h + FreeBlockList.cpp
*		- VirtualMemory.h + VirtualMemory.cpp
*		- SystemBus.h + SystemBus.cpp
		- Memory.h + Memory.cpp
		- Emulator32bit.h + Emulator32bit.cpp + Emulator32bitUtil.h
		- Instructions.cpp + SoftwareInterrupt.cpp
*	- Trying out new style, classes begin with and functions braces are on their own line,
		everything else has braces right after
* improve vscode extension for basm language like autocomplete, syntax highlighting, etc
* 		- File icon theme in vscode extension
Figure shared object files/dynamically linked libraries
* Create tests with the assembler to run longer pieces of code
Figure out IO, Disk, ports, etc
Add flag to set source dirs
Add more relocation types, directives, preprocessors and build flags as needed
Implement .section directive and fix much of the hardcodedness of the assembler/object file/linker
// * Rework logger to not throw on errors, improve the way it is called to reduce length of the log
* Update all logs to use new logger
* Rework exception handling, try to safely except as much as possible, use error codes whenever
Add syscalls for virtual memory management, but for now, they will be controlled in c++ land
*/

const static std::string build_test = "-I .\\tests\\include -o .\\tests\\build\\test "
		".\\tests\\src\\main.basm .\\tests\\src\\other.basm -outdir .\\tests\\build";
const static std::string build_fibonacci = "-I .\\programs\\include "
		"-o .\\programs\\build\\fibonacci .\\programs\\src\\fibonacci.basm "
		"-outdir .\\programs\\build";
const static std::string build_palindrome = "-I .\\programs\\include "
		"-o .\\programs\\build\\palindrome .\\programs\\src\\palindrome.basm "
		"-outdir .\\programs\\build";
const static std::string build_library = "-o .\\programs\\build\\libtest "
		"-makelib .\\programs\\src\\palindrome.basm -outdir .\\programs\\build";
const static std::string build_exe_from_library = "-lib .\\programs\\build\\libtest.ba "
		"-o .\\programs\\build\\palindrome_list -outdir .\\programs\\build";
const static std::string build_exe_from_library_dir = "-libdir .\\programs\\build "
		"-o .\\programs\\build\\palindrome_list -outdir .\\programs\\build";

#define AEMU_MAX_EXEC_INSTR 1000

int main(int argc, char* argv[])
{
    INFO("Running Build");

	std::string build_command = build_exe_from_library_dir;
	if (argc > 1) {
   		INFO("Parsing command arguments");
		build_command = "";
		for (int i = 1; i < argc; i++) {
			build_command += std::string(argv[i]);
			if (i+1 < argc) {
				build_command += " ";
			}
		}
	}

	Process process = Process(build_command);

	byte data[PAGE_SIZE] = {1, 2, 3, 4};
	if (process.does_create_exe()) {
		Disk *disk = new Disk(File("..\\tests\\disk.bin", true), 4);
		Emulator32bit emulator(RAM(16, 0), ROM(data, 1, 16), disk);
		LoadExecutable loader(emulator, process.get_exe_file());

		emulator.run(AEMU_MAX_EXEC_INSTR);
		emulator.print();
	}
}