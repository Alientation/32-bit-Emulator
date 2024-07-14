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

void clearFile(std::string filePath) {
	// don't clear nonexistent files
	if (!std::filesystem::exists(filePath)) {
		return;
	}

	std::ofstream file(filePath, std::ofstream::out | std::ofstream::trunc);
	file.close();
}

int main() {
    lgr::log(lgr::Logger::LogType::TEST, "Testing Preprocessor");

    // test creating a file and its attributes
	Process process = Process("-lib library1 -L .\\tests\\libs -I .\\tests\\include -o .\\tests\\preprocessorTest .\\tests\\src\\preprocessorTest.basm .\\tests\\src\\otherFile.basm");
	process.build();
	// File src_file_1 = File(".\\tests\\src\\preprocessorTest.basm");
	// File src_file_2 = File(".\\tests\\src\\otherFile.basm");

	// test preprocessing
	// Preprocessor preprocessor_1 = Preprocessor(&process, src_file_1);
	// File processed_file_1 = preprocessor_1.preprocess();

	// Preprocessor preprocessor_2 = Preprocessor(&process, src_file_2);
	// File processed_file_2 = preprocessor_2.preprocess();

	// Assembler assembler1 = Assembler(&process, processed_file_1);
	// File output_file_1 = assembler1.assemble();

	// Assembler assembler_2 = Assembler(&process, processed_file_2);
	// File output_file_2 = assembler_2.assemble();

	// ObjectFile obj_file_1(output_file_1);
	// ObjectFile obj_file_2(output_file_2);

	// File exe_file = File(".\\tests\\preprocessorTest.bexe");
	// Linker linker({obj_file_1, obj_file_2}, exe_file);

	Emulator32bit emulator;
	LoadExecutable loader(emulator, process.get_exe_file());

	emulator.run(100);
	emulator.print();
}