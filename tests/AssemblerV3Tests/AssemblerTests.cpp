#include "Logger.h"
#include <../src/AssemblerV3/File.h>
#include <../src/AssemblerV3/Preprocess/PreprocessorV3.h>

#include <sstream>
#include <fstream>

void fileTests();
void fileReaderAll();
void fileReaderByte();
void fileReaderBytes();

void preprocessorTests();

void clearFile(std::string filePath) {
	std::ofstream file(filePath, std::ofstream::out | std::ofstream::trunc);
	file.close();
}


int main() {
	log(TEST, std::stringstream() << "Running Assembler Tests");

	// fileTests();
	preprocessorTests();





	return 0;
}


void preprocessorTests() {
	log(TEST, std::stringstream() << "Running Preprocessor Tests");

	// test creating a file and its attributes
	Process* process = new Process();
	File* file = new File("..\\tests\\AssemblerV3Tests\\Files\\preprocessorTest.basm");
	clearFile("..\\tests\\AssemblerV3Tests\\Files\\preprocessorTest.i");

	// test preprocessing
	Preprocessor* preprocessor = new Preprocessor(process, file);
	preprocessor->preprocess();
}


void fileTests() {
	// test creating a file and its attributes
	File* file = new File("..\\tests\\AssemblerV3Tests\\Files\\empty.txt");
	log(TEST, std::stringstream() << "File Name: " << file->getFileName());
	log(TEST, std::stringstream() << "File Extension: " << file->getExtension());
	log(TEST, std::stringstream() << "File Path: " << file->getFilePath());
	log(TEST, std::stringstream() << "File Size: " << file->getFileSize());

	// fileReaderAll();
	// fileReaderByte();
	// fileReaderBytes();
}

/**
 * Test reading all bytes from file
 */
void fileReaderAll() {
	log(TEST, std::stringstream() << "Running File:READ_ALL Tests");
	// clear write to file
	clearFile("..\\tests\\AssemblerV3Tests\\Files\\writeToFile.txt");

	// read from test file
	FileReader reader = FileReader(new File("..\\tests\\AssemblerV3Tests\\Files\\readFromFile.txt"));
	std::string fileContents = reader.readAll();
	reader.close();
	log(TEST, std::stringstream() << "File Contents: \n" << fileContents);

	// write to test file
	FileWriter writer = FileWriter(new File("..\\tests\\AssemblerV3Tests\\Files\\writeToFile.txt"));
	writer.writeString(fileContents);
	writer.close();
}

/**
 * Test reading individual bytes at a time
 */
void fileReaderByte() {
	log(TEST, std::stringstream() << "Running File:READ_BYTE Tests");
	// clear write to file
	clearFile("..\\tests\\AssemblerV3Tests\\Files\\writeToFile.txt");

	// read byte one by one from test file
	FileReader reader = FileReader(new File("..\\tests\\AssemblerV3Tests\\Files\\readFromFile.txt"));
	std::string fileContents;
	while (reader.hasNextByte()) {
		fileContents += reader.readByte();
	}
	reader.close();
	log(TEST, std::stringstream() << "File Contents: \n" << fileContents);

	// write bye one by one to test file
	FileWriter writer = FileWriter(new File("..\\tests\\AssemblerV3Tests\\Files\\writeToFile.txt"));
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
	clearFile("..\\tests\\AssemblerV3Tests\\Files\\writeToFile.txt");

	// read byte one by one from test file
	File* file = new File("..\\tests\\AssemblerV3Tests\\Files\\readFromFile.txt");
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
	FileWriter writer = FileWriter(new File("..\\tests\\AssemblerV3Tests\\Files\\writeToFile.txt"));
	for (int i = 0; i < fileContents.length(); i++) {
		writer.writeByte(fileContents[i]);
	}
	writer.close();
}