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

int main(int argc, char* argv[]) {
    lgr::log(lgr::Logger::LogType::INFO, "Running Build");

	std::string build_command = "-lib library1 -L .\\tests\\libs -I .\\tests\\include -o .\\build\\test .\\tests\\src\\main.basm .\\tests\\src\\other.basm -outdir .\\tests\\build";
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

	// TODO, there is some weird segfault that happened here, but now it disappeared
	Process process = Process(build_command);

	Emulator32bit emulator;
	LoadExecutable loader(emulator, process.get_exe_file());

	emulator.run(100);
	emulator.print();
}