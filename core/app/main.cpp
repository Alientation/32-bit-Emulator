#include <assembler/Build.h>
#include <assembler/Preprocessor.h>
#include <assembler/util/Logger.h>
#include <assembler/util/File.h>
#include <assembler/util/Directory.h>

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
    log(TEST, std::stringstream() << "Testing Preprocessor");

    // test creating a file and its attributes
	Process process = Process("-lib library1 -L ..\\tests\\AssemblerV3Tests\\Files\\libs -I ..\\tests\\AssemblerV3Tests\\Files\\include -o preprocessorTest ..\\tests\\AssemblerV3Tests\\Files\\preprocessorTest.basm");
	File* file = new File("..\\tests\\AssemblerV3Tests\\Files\\preprocessorTest." + SOURCE_EXTENSION);
	clearFile("..\\tests\\AssemblerV3Tests\\Files\\preprocessorTest." + PROCESSED_EXTENSION);

	// test preprocessing
	Preprocessor* preprocessor = new Preprocessor(&process, file);
	preprocessor->preprocess();
}