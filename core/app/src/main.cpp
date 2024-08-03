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
// *	- Macro preprocessors should follow the standard ALL_CAPS_WITH_UNDERSCORE naming conventions
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
// *	- Trying out new style, classes begin with and functions braces are on their own line,
		// everything else has braces right after

* improve vscode extension for basm language like autocomplete, syntax highlighting, etc
// * 		- File icon theme in vscode extension

Figure shared object files/dynamically linked libraries

// Update ROM memory to allow special access to flash it.
// Update Memory to operate in terms of pages. This will also mean that when we use
// memory mapped I/O and ports that they will operate in terms of pages.
Enforce a zeroing of the first couple pages of memory to catch null pointers <- likely handled by the kernel
Work out how the stack and heap will work (likely operates in virtual memory as opposed to physical memory)
Work out how context switching will work (likely the only state that needs to be saved will be registers and process id)

* Create tests with the assembler to run longer pieces of code

Benchmark system for each component of the project (tokenizer, preprocessor, assembler, linker, emulator)

// Figure out IO, Disk, ports, etc
	- We will use memory mapped IO that can be accessed through syscalls to the kernel
	- Disk is implemented as a file stored on the host computer that represents disk memory
	- Ports will be memory mapped that will be accessed through syscalls to the kernel

Add flag to set source dirs

Add more relocation types, directives, preprocessors and build flags as needed

Rework the build process to be more like the tokenizer

Implement .section directive and fix much of the hardcodedness of the assembler/object file/linker

Implement linker scripts that the linker will process to create the final BELF file
	- https://users.informatik.haw-hamburg.de/~krabat/FH-Labor/gnupro/5_GNUPro_Utilities/c_Using_LD/ldLinker_scripts.html

Visualizer with raylib to visually show the state of the processor

Bootloader that loads the kernel

// * Rework logger to not throw on errors, improve the way it is called to reduce length of the log
// * Update all logs to use new logger
// * Add profiler to logger

Rework exception handling, try to safely except as much as possible, use error codes whenever possible
Go through all the code and try to add more safeguards to catch invalid states and throw exceptions/error codes

Add syscalls for virtual memory management, but for now, they will be controlled in c++ land
*/

/*
KNOWN BUGS

the assembler will not check the bit lengths of any value that is passed as an immediate (likely extends beyond that too)

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

#define AEMU_MAX_EXEC_INSTR 0xFFFFFF

int main(int argc, char* argv[])
{
	PROFILE_START

	CLOCK_START("Parsing command arguments")
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
	CLOCK_END
	CLOCK_START("Building program")

	Process process = Process(build_command);
	CLOCK_STOP

	CLOCK_START("Loading program into emulator")
	byte data[PAGE_SIZE] = {1, 2, 3, 4};
	if (process.does_create_exe()) {
		Disk *disk = new Disk(File("..\\tests\\disk.bin", true), 4);
		Emulator32bit emulator(RAM(16, 0), ROM(data, 1, 16), disk);
		LoadExecutable loader(emulator, process.get_exe_file());
		CLOCK_STOP

		CLOCK_START("Running emulator")
		emulator.run(AEMU_MAX_EXEC_INSTR);
		// emulator.print();
	}
	CLOCK_END
	PROFILE_STOP
}