#include "assembler/assembler.h"
#include "assembler/build.h"
#include "assembler/linker.h"
#include "assembler/load_executable.h"
#include "assembler/object_file.h"
#include "assembler/preprocessor.h"
#include "emulator32bit/emulator32bit.h"
#include "emulator32bit/disk.h"
#include "util/file.h"
#include "util/loggerv2.h"

/*
TODO

* CLEAN UP CODE
*	- Standardized braces placement (always on new line)
*	- Split long functions into smaller unit sized ones, especially if they can be reused elsewhere
*	- Macro preprocessors should follow the standard ALL_CAPS_WITH_UNDERSCORE naming conventions
		(DEFINITIONS SHOULD COME WITH THE PROJECT NAME PREPENDED BEFORE)
*	- Comment functions and complex logic
*	- Make variables that have larger scopes have more meaningful names
*	- FIX TODO (emulator32bit library)
*		- Disk.h + Disk.cpp
*		- FreeBlockList.h + FreeBlockList.cpp
*		- VirtualMemory.h + VirtualMemory.cpp
*		- SystemBus.h + SystemBus.cpp
		- Memory.h + Memory.cpp
		- Emulator32bit.h + Emulator32bit.cpp + Emulator32bitUtil.h
		- Instructions.cpp + SoftwareInterrupt.cpp

* improve vscode extension for basm language like autocomplete, syntax highlighting, etc
// * 		- File icon theme in vscode extension

Figure shared object files/dynamically linked libraries

// Update Memory to operate in terms of pages. This will also mean that when we use
// memory mapped I/O and ports that they will operate in terms of pages.
Enforce a zeroing of the first couple pages of memory to catch null pointers <- likely handled by the kernel
Work out how the stack and heap will work (likely operates in virtual memory as opposed to physical memory)
Work out how context switching will work (likely the only state that needs to be saved will be registers and process id)

* Create tests with the assembler to run longer pieces of code

Benchmark system for each component of the project (tokenizer, preprocessor, assembler, linker, emulator)
Idea for optimization, add macros to specify whether we want a large amount of invalid state checking,
idea is to have the ability to reduce branch mispredictions
instead of passing exceptions into functions, have the class that contains the functions maintain its
exception state.

// Figure out IO, Disk, ports, etc
	- We will use memory mapped IO that can be accessed through syscalls to the kernel
	- Disk is implemented as a file stored on the host computer that represents disk memory
	- Ports will be memory mapped that will be accessed through syscalls to the kernel

Change disk to extend Memory, add it to system bus
This means we will have to make the Memory methods virtual, which means to allow the compiler to inline those functions,
we need to access these functions with object itself and not a reference/pointer to it. This means our current
way of storing all memory in a list and iterating the list to find a match won't work. We will have to settle for
hardcoded conditional checks.
Change virtual memory to not automatically map more virtual memory pages to physical pages when they are accessed. This should instead through a segfault
exception in the emulator.
We instead will have to manually request the virtual memory to map a specfic set of virtual memory pages to a *type* of memory (by suppling the memory object?)
or giving a range of physical page addresses. We also already have a way to map a specific physical page.
Next, allow executable loader to write to a specific physical address so we can write the kernel/os to disk for the BIOS to load

Add flag to set source dirs for build
Add more relocation types, directives, preprocessors and build flags as needed
Rework the build process to be more like the tokenizer
Implement .section directive and fix much of the hardcodedness of the assembler/object file/linker
Implement linker scripts that the linker will process to create the final BELF file
	- https://users.informatik.haw-hamburg.de/~krabat/FH-Labor/gnupro/5_GNUPro_Utilities/c_Using_LD/ldLinker_scripts.html
Figure out executable linking/loading

// Make ROM take in a file so that it reads from it at the start and writes to it at the end to save.

Rework how byte reader works, don't like how we have to manually extra all bytes from file and then
pass it to the the byte reader. Maybe change that to ByteParser and add a ByteReader that uses it
and the file reader to parse the bytes.

Visualizer with raylib to visually show the state of the processor

Bootloader that loads the kernel

// * Rework logger to not throw on errors, improve the way it is called to reduce length of the log
// * Update all logs to use new logger
// * Add profiler to logger

// Rework exception handling, try to safely except as much as possible. Throw exceptions and catch them
// where appropriate... Avoid using the current practice of error codes since they introduce a lot of
// overhead when not necessary.

Add syscalls for virtual memory management, but for now, they will be controlled in c++ land
Maybe have a toggle for virtual memory instead??, like using the mocks.
*/

/*
KNOWN BUGS

the assembler will not check the bit lengths of any value that is passed as an immediate (problem likely extends beyond that too)
very few exceptions are thrown when encountering invalid states (invalid states that can produce unintended consequences should throw an exception)
leading to incorrect programs being runnable when it should not have been.
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

#define AEMU_MAX_EXEC_INSTR 0x0

int main(int argc, char* argv[])
{
	PROFILE_START

	CLOCK_START("Parsing command arguments")
	std::string build_command = build_palindrome;
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
	CLOCK_END

	if (process.does_create_exe()) {
		CLOCK_START("Loading program into emulator")
		Disk *disk = new Disk(File("..\\tests\\disk.bin", true), 4);
		Emulator32bit emulator(RAM(16, 0), ROM(File("..\\tests\\rom.bin", true), 1, 16), disk);
		emulator.system_bus.mmu.begin_process(0, 0, 4);
		LoadExecutable loader(emulator, process.get_exe_file());
		CLOCK_END

		DEBUG("Running emulator");
		CLOCK_START("Running emulator")
		emulator.run(AEMU_MAX_EXEC_INSTR);
		CLOCK_END
		emulator.print();
		emulator.system_bus.mmu.end_process(0);
	}

	PROFILE_STOP
}