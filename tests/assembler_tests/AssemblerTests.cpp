#include <util/logger.h>
#include <util/file.h>
#include <util/directory.h>
#include <assembler/preprocessor.h>

#include <filesystem>
#include <sstream>
#include <fstream>

// quick work around to the issue with the current working directory being the binary directory and not the source directory
static const std::string REL_PATH_TO_FILES_FOLDER = "..\\..\\assembler_tests\\files\\";

void fileTests();
void fileReaderAll();
void fileReaderByte();
void fileReaderBytes();

void preprocessorTests();

void directoryTests();

void clearFile(std::string filePath) {
	// don't clear nonexistent files
	if (!std::filesystem::exists(filePath)) {
		return;
	}

	std::ofstream file(filePath, std::ofstream::out | std::ofstream::trunc);
	file.close();
}


int main() {
	log(lgr::Logger::LogType::TEST, std::stringstream() << "Running Assembler Tests");

	// fileTests();
	// directoryTests();
	preprocessorTests();





	return 0;
}


/**
 * Test the PreprocessorV3 class
 */
void preprocessorTests() {
	log(lgr::Logger::LogType::TEST, std::stringstream() << "Running Preprocessor Tests");

	// test creating a file and its attributes
	Process process = Process("-lib library1 -L " + REL_PATH_TO_FILES_FOLDER + "libs -I " + REL_PATH_TO_FILES_FOLDER + "include -o preprocessorTest " + REL_PATH_TO_FILES_FOLDER + "preprocessorTest.basm");
	File* file = new File(REL_PATH_TO_FILES_FOLDER + "preprocessorTest." + SOURCE_EXTENSION);
	clearFile(REL_PATH_TO_FILES_FOLDER + "preprocessorTest." + PROCESSED_EXTENSION);

	// test preprocessing
	Preprocessor* preprocessor = new Preprocessor(&process, file);
	preprocessor->preprocess();
}


/**
 * Test the Directory class implementation
 */
void directoryTests() {
	log(lgr::Logger::LogType::TEST, std::stringstream() << "Running Directory Tests");

	// test creating a file and its attributes
	Directory* directory = new Directory(REL_PATH_TO_FILES_FOLDER);
	log(lgr::Logger::LogType::TEST, std::stringstream() << "Directory Name: " << directory->get_name());
	log(lgr::Logger::LogType::TEST, std::stringstream() << "Directory Path: " << directory->get_path());
	log(lgr::Logger::LogType::TEST, std::stringstream() << "Directory Size: " << directory->get_size());

	// test getting files
	std::vector<File*> files = directory->get_subfiles();
	for (int i = 0; i < files.size(); i++) {
		log(lgr::Logger::LogType::TEST, std::stringstream() << "File " << i << ": " << files[i]->get_name());
	}

	// test getting directories
	std::vector<Directory*> directories = directory->get_subdirs();
	for (int i = 0; i < directories.size(); i++) {
		log(lgr::Logger::LogType::TEST, std::stringstream() << "Directory " << i << ": " << directories[i]->get_name());
	}
}


/**
 * Test the File class implementation
 */
void fileTests() {
	// test creating a file and its attributes
	File* file = new File(REL_PATH_TO_FILES_FOLDER + "empty.txt");
	log(lgr::Logger::LogType::TEST, std::stringstream() << "File Name: " << file->get_name());
	log(lgr::Logger::LogType::TEST, std::stringstream() << "File Extension: " << file->get_extension());
	log(lgr::Logger::LogType::TEST, std::stringstream() << "File Path: " << file->get_path());
	log(lgr::Logger::LogType::TEST, std::stringstream() << "File Size: " << file->get_size());

	fileReaderAll();
	// fileReaderByte();
	// fileReaderBytes();
}

/**
 * Test reading all bytes from file
 */
void fileReaderAll() {
	log(lgr::Logger::LogType::TEST, std::stringstream() << "Running File:READ_ALL Tests");
	// clear write to file
	clearFile(REL_PATH_TO_FILES_FOLDER + "writeToFile.txt");

	// read from test file
	FileReader reader = FileReader(new File(REL_PATH_TO_FILES_FOLDER + "readFromFile.txt"));
	std::string fileContents = reader.read_all();
	reader.close();
	log(lgr::Logger::LogType::TEST, std::stringstream() << "File Contents: \n" << fileContents);

	// write to test file
	FileWriter writer = FileWriter(new File(REL_PATH_TO_FILES_FOLDER + "writeToFile.txt"));
	writer.write(fileContents);
	writer.close();
}

/**
 * Test reading individual bytes at a time
 */
void fileReaderByte() {
	log(lgr::Logger::LogType::TEST, std::stringstream() << "Running File:READ_BYTE Tests");
	// clear write to file
	clearFile(REL_PATH_TO_FILES_FOLDER + "writeToFile.txt");

	// read byte one by one from test file
	FileReader reader = FileReader(new File(REL_PATH_TO_FILES_FOLDER + "readFromFile.txt"));
	std::string fileContents;
	while (reader.has_next_byte()) {
		fileContents += reader.read_byte();
	}
	reader.close();
	log(lgr::Logger::LogType::TEST, std::stringstream() << "File Contents: \n" << fileContents);

	// write bye one by one to test file
	FileWriter writer = FileWriter(new File(REL_PATH_TO_FILES_FOLDER + "writeToFile.txt"));
	for (int i = 0; i < fileContents.length(); i++) {
		writer.write(fileContents[i]);
	}
	writer.close();
}

/**
 * Test reading chunks of bytes at a time
 */
void fileReaderBytes() {
	log(lgr::Logger::LogType::TEST, std::stringstream() << "Running File:READ_BYTES Tests");
	// clear write to file
	clearFile(REL_PATH_TO_FILES_FOLDER + "writeToFile.txt");

	// read byte one by one from test file
	File* file = new File(REL_PATH_TO_FILES_FOLDER + "readFromFile.txt");
	FileReader reader = FileReader(file);
	std::string fileContents;

	// first reach in chunks of 8 bytes
	int sizeLeft = file->get_size();
	while (sizeLeft >= 8) {
		fileContents += reader.read_bytes(8);
	}

	// read the remaining bytes in the file
	while (reader.has_next_byte()) {
		fileContents += reader.read_byte();
	}
	reader.close();
	log(lgr::Logger::LogType::TEST, std::stringstream() << "File Contents: \n" << fileContents);

	// write bye one by one to test file
	FileWriter writer = FileWriter(new File(REL_PATH_TO_FILES_FOLDER + "writeToFile.txt"));
	for (int i = 0; i < fileContents.length(); i++) {
		writer.write(fileContents[i]);
	}
	writer.close();
}