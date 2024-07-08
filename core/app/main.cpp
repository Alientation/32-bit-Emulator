#include <assembler/Build.h>
#include <assembler/Preprocessor.h>
#include <assembler/Assembler.h>
#include <util/Logger.h>
#include <util/File.h>
#include <util/Directory.h>

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
	Process process = Process("-lib library1 -L .\\tests\\libs -I .\\tests\\include -o preprocessorTest .\\tests\\src\\preprocessorTest." + SOURCE_EXTENSION);
	File file = File(".\\tests\\src\\preprocessorTest." + SOURCE_EXTENSION);
	clearFile(".\\tests\\src\\preprocessorTest." + PROCESSED_EXTENSION);

	// test preprocessing
	Preprocessor preprocessor = Preprocessor(&process, &file);
	File* processed_file = preprocessor.preprocess();

	Assembler assembler = Assembler(&process, processed_file);
	File* output_file = assembler.assemble();
}