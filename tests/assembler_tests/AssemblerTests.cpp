#include <util/Logger.h>
#include <util/File.h>
#include <util/Directory.h>
#include <assembler/Preprocessor.h>

#include <filesystem>
#include <sstream>
#include <fstream>

// quick work around to the issue with the current working directory being the binary directory and not the source directory
static std::string REL_PATH_TO_FILES_FOLDER = "..\\..\\assembler_tests\\files\\";

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
	log(TEST, std::stringstream() << "Running Assembler Tests");

	// fileTests();
	// directoryTests();
	preprocessorTests();





	return 0;
}


/**
 * Test the PreprocessorV3 class
 */
void preprocessorTests() {
	log(TEST, std::stringstream() << "Running Preprocessor Tests");

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
	log(TEST, std::stringstream() << "Running Directory Tests");

	// test creating a file and its attributes
	Directory* directory = new Directory(REL_PATH_TO_FILES_FOLDER);
	log(TEST, std::stringstream() << "Directory Name: " << directory->getDirectoryName());
	log(TEST, std::stringstream() << "Directory Path: " << directory->getDirectoryPath());
	log(TEST, std::stringstream() << "Directory Size: " << directory->getDirectorySize());

	// test getting files
	std::vector<File*> files = directory->getSubfiles();
	for (int i = 0; i < files.size(); i++) {
		log(TEST, std::stringstream() << "File " << i << ": " << files[i]->getFileName());
	}

	// test getting directories
	std::vector<Directory*> directories = directory->getSubdirectories();
	for (int i = 0; i < directories.size(); i++) {
		log(TEST, std::stringstream() << "Directory " << i << ": " << directories[i]->getDirectoryName());
	}
}


/**
 * Test the File class implementation
 */
void fileTests() {
	// test creating a file and its attributes
	File* file = new File(REL_PATH_TO_FILES_FOLDER + "empty.txt");
	log(TEST, std::stringstream() << "File Name: " << file->getFileName());
	log(TEST, std::stringstream() << "File Extension: " << file->getExtension());
	log(TEST, std::stringstream() << "File Path: " << file->getFilePath());
	log(TEST, std::stringstream() << "File Size: " << file->getFileSize());

	fileReaderAll();
	// fileReaderByte();
	// fileReaderBytes();
}

/**
 * Test reading all bytes from file
 */
void fileReaderAll() {
	log(TEST, std::stringstream() << "Running File:READ_ALL Tests");
	// clear write to file
	clearFile(REL_PATH_TO_FILES_FOLDER + "writeToFile.txt");

	// read from test file
	FileReader reader = FileReader(new File(REL_PATH_TO_FILES_FOLDER + "readFromFile.txt"));
	std::string fileContents = reader.readAll();
	reader.close();
	log(TEST, std::stringstream() << "File Contents: \n" << fileContents);

	// write to test file
	FileWriter writer = FileWriter(new File(REL_PATH_TO_FILES_FOLDER + "writeToFile.txt"));
	writer.writeString(fileContents);
	writer.close();
}

/**
 * Test reading individual bytes at a time
 */
void fileReaderByte() {
	log(TEST, std::stringstream() << "Running File:READ_BYTE Tests");
	// clear write to file
	clearFile(REL_PATH_TO_FILES_FOLDER + "writeToFile.txt");

	// read byte one by one from test file
	FileReader reader = FileReader(new File(REL_PATH_TO_FILES_FOLDER + "readFromFile.txt"));
	std::string fileContents;
	while (reader.hasNextByte()) {
		fileContents += reader.readByte();
	}
	reader.close();
	log(TEST, std::stringstream() << "File Contents: \n" << fileContents);

	// write bye one by one to test file
	FileWriter writer = FileWriter(new File(REL_PATH_TO_FILES_FOLDER + "writeToFile.txt"));
	for (int i = 0; i < fileContents.length(); i++) {
		writer.writeByte(fileContents[i]);
	}
	writer.close();
}

/**
 * Test reading chunks of bytes at a time
 */
void fileReaderBytes() {
	log(TEST, std::stringstream() << "Running File:READ_BYTES Tests");
	// clear write to file
	clearFile(REL_PATH_TO_FILES_FOLDER + "writeToFile.txt");

	// read byte one by one from test file
	File* file = new File(REL_PATH_TO_FILES_FOLDER + "readFromFile.txt");
	FileReader reader = FileReader(file);
	std::string fileContents;

	// first reach in chunks of 8 bytes
	int sizeLeft = file->getFileSize();
	while (sizeLeft >= 8) {
		fileContents += reader.readBytes(8);
	}

	// read the remaining bytes in the file
	while (reader.hasNextByte()) {
		fileContents += reader.readByte();
	}
	reader.close();
	log(TEST, std::stringstream() << "File Contents: \n" << fileContents);

	// write bye one by one to test file
	FileWriter writer = FileWriter(new File(REL_PATH_TO_FILES_FOLDER + "writeToFile.txt"));
	for (int i = 0; i < fileContents.length(); i++) {
		writer.writeByte(fileContents[i]);
	}
	writer.close();
}