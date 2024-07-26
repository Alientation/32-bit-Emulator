#include "assembler/Assembler.h"
#include "assembler/Build.h"
#include "assembler/Linker.h"
#include "assembler/ObjectFile.h"
#include "assembler/Preprocessor.h"
#include "assembler/LoadExecutable.h"
#include "emulator32bit/Emulator32bit.h"
#include "emulator32bit/Disk.h"
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
Figure shared object files/dynamically linked libraries
* Create tests with the assembler to run longer pieces of code
* Implement software interrupts
*		- Print, logdump (print processor state)
Figure out IO, Disk, ports, etc
Add flag to set source dirs
Add more relocation types, directives, preprocessors and build flags as needed
Implement .section directive and fix much of the hardcodedness of the assembler/object file/linker
Rework logger to not throw on errors, improve the way it is called to reduce length of the log
Rework exception handling, try to safely except as much as possible, use error codes whenever
*/

const std::string build_test = "-I .\\tests\\include -o .\\tests\\build\\test .\\tests\\src\\main.basm .\\tests\\src\\other.basm -outdir .\\tests\\build";
const std::string build_fibonacci = "-I .\\programs\\include -o .\\programs\\build\\fibonacci .\\programs\\src\\fibonacci.basm -outdir .\\programs\\build";
const std::string build_palindrome = "-I .\\programs\\include -o .\\programs\\build\\palindrome .\\programs\\src\\palindrome.basm -outdir .\\programs\\build";
const std::string build_library = "-o .\\programs\\build\\libtest -makelib .\\programs\\src\\palindrome.basm -outdir .\\programs\\build";
const std::string build_exe_from_library = "-lib .\\programs\\build\\libtest.ba -o .\\programs\\build\\palindrome_list -outdir .\\programs\\build";
const std::string build_exe_from_library_dir = "-libdir .\\programs\\build -o .\\programs\\build\\palindrome_list -outdir .\\programs\\build";

#define MAX_EXECUTED_INSTRUCTIONS 1000

int main(int argc, char* argv[]) {
    lgr::log(lgr::Logger::LogType::INFO, "Running Build");

	std::string build_command = build_exe_from_library_dir;
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

	// Process process = Process(build_command);

	byte data[PAGE_SIZE] = {1, 2, 3, 4};
	// if (process.does_create_exe()) {
		Disk *disk = new Disk(File("..\\tests\\disk.bin", true), 4);
		// Emulator32bit emulator(RAM(16, 0), ROM(data, 1, 16), disk);
		// Emulator32bit emulator(16, 0, data, 1, 16);
		// LoadExecutable loader(emulator, process.get_exe_file());

		// emulator.run(MAX_EXECUTED_INSTRUCTIONS);
		// emulator.print();

		VirtualMemory vm(0, 0, *disk);
		SystemBus bus(RAM(1, 0), ROM(data, 1, 1), vm);
		vm.begin_process(0, 0, 1*PAGE_SIZE-1);

		SystemBus::Exception bus_exception;
		Memory::WriteException mem_write_exception;
		Memory::ReadException mem_read_exception;

		printf("!!!!WRITING BYTE 0xF0 TO ADDRESS 0 OF PID=0\n");
		bus.write_byte(0, 0xF0, bus_exception, mem_write_exception);

		printf("!!!!READING BYTE FROM ADDRESS 0 OF PID=0\n");
		EXPECT_TRUE(bus.read_byte(0, bus_exception, mem_read_exception) == 0xF0,
				lgr::Logger::LogType::ERROR, std::stringstream() << "FAILED READ PAGE 0 FROM PROCESS 0");

		vm.begin_process(1, 0, PAGE_SIZE-1);
		printf("!!!!WRITING BYTE 0x01 TO ADDRESS 0 OF PID=1\n");
		bus.write_byte(0, 0x01, bus_exception, mem_write_exception);

		printf("!!!!READING BYTE FROM ADDRESS 0 OF PID=1\n");
		EXPECT_TRUE(bus.read_byte(0, bus_exception, mem_read_exception) == 0x01,
				lgr::Logger::LogType::ERROR, std::stringstream() << "FAILED READ PAGE 0 FROM PROCESS 1");

		disk->write_all();

		vm.set_process(0);

		printf("!!!!READING BYTE FROM ADDRESS 0 OF PID=0\n");
		EXPECT_TRUE(bus.read_byte(0, bus_exception, mem_read_exception) == 0xF0,
				lgr::Logger::LogType::ERROR, std::stringstream() << "FAILED READ PAGE 0 FROM PROCESS 0");

		vm.set_process(1);

		printf("!!!!READING BYTE FROM ADDRESS 0 OF PID=1\n");
		EXPECT_TRUE(bus.read_byte(0, bus_exception, mem_read_exception) == 0x01,
				lgr::Logger::LogType::ERROR, std::stringstream() << "FAILED READ PAGE 0 FROM PROCESS 1");

		if (bus_exception.type != SystemBus::Exception::Type::AOK) {
			lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Bus Exception");
		}

		if (mem_write_exception.type != Memory::WriteException::Type::AOK) {
			lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Memory Write Exception");
		}

		if (mem_read_exception.type != Memory::ReadException::Type::AOK) {
			lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Memory Read Exception");
		}

		vm.end_process(1);
		vm.end_process(0);

		disk->write_all();
		delete disk;
	// }
}