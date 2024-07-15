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

int main() {
    lgr::log(lgr::Logger::LogType::TEST, "Testing Preprocessor");

	Process process = Process("-lib library1 -L .\\tests\\libs -I .\\tests\\include -o .\\tests\\preprocessorTest .\\tests\\src\\preprocessorTest.basm .\\tests\\src\\otherFile.basm");
	process.build();

	Emulator32bit emulator;
	LoadExecutable loader(emulator, process.get_exe_file());

	emulator.run(100);
	emulator.print();
}