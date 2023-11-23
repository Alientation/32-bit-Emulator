#include "File.h"

#include "Logger.h"
#include <fstream>
#include <filesystem>

/**
 * Constructs a file object with the given file name and directory.
 * 
 * @param fileName the name of the file
 * @param fileDirectory the directory of the file
 */
File::File(const std::string fileName, const std::string fileExtension, const std::string fileDirectory = "") {
	this->fileName = fileName;
	this->fileExtension = fileExtension;

	if (fileDirectory.empty()) {
		this->fileDirectory = std::filesystem::current_path().string();
	} else {
		this->fileDirectory = fileDirectory;
	}

	if (!isValidFileName(fileName)) {
		log(ERROR, std::stringstream() << "File::File() - Invalid file name: " << fileName);
	} else if (!isValidFileExtension(fileExtension)) {
		log(ERROR, std::stringstream() << "File::File() - Invalid file extension: " << fileExtension);
	} else if (!isValidFileDirectory(fileDirectory)) {
		log(ERROR, std::stringstream() << "File::File() - Invalid file directory: " << fileDirectory);
	}

	createFileIfNotExist();
}

/**
 * Constructs a file object with the given file path.
 * 
 * @param filePath the path of the file
 */
File::File(const std::string filePath) {
	std::size_t extensionSeparatorIndex = filePath.find_last_of(".");
	if (extensionSeparatorIndex == std::string::npos) {
		log(ERROR, std::stringstream() << "File::File() - File path does not contain an extension: " << filePath);
	}

	std::string fileNameAndExtension = filePath.substr(filePath.find_last_of(SEPARATOR) + 1);
	this->fileName = fileNameAndExtension.substr(0, fileNameAndExtension.find_last_of("."));
	this->fileExtension = fileNameAndExtension.substr(fileNameAndExtension.find_last_of(".") + 1);
	this->fileDirectory = filePath.substr(0, filePath.find_last_of(SEPARATOR));

	if (!isValidFileName(fileName)) {
		log(ERROR, std::stringstream() << "File::File() - Invalid file name: " << fileName);
	} else if (!isValidFileExtension(fileExtension)) {
		log(ERROR, std::stringstream() << "File::File() - Invalid file extension: " << fileExtension);
	} else if (!isValidFileDirectory(fileDirectory)) {
		log(ERROR, std::stringstream() << "File::File() - Invalid file directory: " << fileDirectory);
	}

	createFileIfNotExist();
}

/**
 * Creates the file if it does not exist
 */
void File::createFileIfNotExist() {
	// check if file exists, if not then create the file
	std::ifstream file(this->getFilePath());
	if (!file.good()) {
		std::ofstream file(this->getFilePath());
		file.close();
	}
}

/**
 * Destructs a file object
 */
File::~File() {
	
}

/**
 * Returns the name of the file
 * 
 * @return the name of the file
 */
std::string File::getFileName() {
	return this->fileName;
}

/**
 * Returns the extension of the file
 * 
 * @return the extension of the file
 */
std::string File::getExtension() {
	return this->fileExtension;
}

/**
 * Returns the path of the file
 * 
 * @return the path of the file
 */
std::string File::getFilePath() {
	return this->fileDirectory + SEPARATOR + this->fileName + "." + this->fileExtension;
}

/**
 * Returns the directory of the file
 * 
 * @return the directory of the file
 */
std::string File::getFileDirectory() {
	return this->fileDirectory;
}

/**
 * Gets the size of the file in bytes
 * 
 * @return the size of the file in bytes
 */
int File::getFileSize() {
	int fileSize = 0;

	std::ifstream fileStream = std::ifstream(this->getFilePath(), std::ifstream::in);
	while (fileStream.peek() != EOF) {
		fileStream.get();
		fileSize++;
	}

	return fileSize;
}




/**
 * Constructs a file writer object with the given file
 * 
 * @param file the file to write to
 */
FileWriter::FileWriter(File* file) {
	this->file = file;
	this->fileStream = new std::ofstream(file->getFilePath(), std::ifstream::out);
	this->closed = false;

	if (!this->fileStream->good()) {
		log(ERROR, std::stringstream() << "FileWriter::FileWriter() - Failed to open file: " << file->getFilePath());
	}
}

/**
 * Destructs a file writer object
 */
FileWriter::~FileWriter() {
	this->close();
}

/**
 * Writes a string to the file
 * 
 * @param text the string to write
 */
void FileWriter::writeString(const std::string text) {
	(*this->fileStream) << text;
}

/**
 * Writes a byte to the file
 * 
 * @param byte the byte to write
 */
void FileWriter::writeByte(const char byte) {
	(*this->fileStream) << byte;
}

/**
 * Writes a byte array to the file
 * 
 * @param bytes the byte array to write
 */
void FileWriter::writeBytes(char* bytes) {
	(*this->fileStream) << bytes;
}

/**
 * Closes the file writer
 */
void FileWriter::close() {
	if (!this->closed) {
		this->fileStream->close();
		this->closed = true;
	}
}




/**
 * Constructs a file reader object with the given file
 * 
 * @param file the file to read from
 */
FileReader::FileReader(File* file) {
	this->file = file;
	this->fileStream = new std::ifstream(file->getFilePath(), std::ifstream::in);
	this->closed = false;

	if (!this->fileStream->good()) {
		log(ERROR, std::stringstream() << "FileReader::FileReader() - Failed to open file: " << file->getFilePath());
	}
}

/**
 * Destructs a file reader object
 */
FileReader::~FileReader() {
	this->close();
}

/**
 * Reads the entire file and returns it as a string
 * 
 * @return the entire file as a string
 */
std::string FileReader::readAll() {
	std::string fileContents;
	while (fileStream->good()) {
		fileContents += fileStream->get();
	}
	close();
	return fileContents;
}

/**
 * Reads a byte from the file
 * 
 * @return the byte read from the file
 */
char FileReader::readByte() {
	return this->fileStream->get();;
}

/**
 * Reads a number of bytes from the file
 * 
 * @param numBytes the number of bytes to read
 * @return the bytes read from the file
 */
char* FileReader::readBytes(const unsigned int numBytes) {
	char* bytes = new char[numBytes];
	this->fileStream->read(bytes, numBytes);

	if (fileStream->fail()) {
		log(ERROR, std::stringstream() << "FileReader::readBytes() - Failed to read " << 
				numBytes << " bytes from file: " << this->file->getFilePath());
	}

	return bytes;
}

/**
 * Returns true if there is another byte to read
 * 
 * @return true if there is another byte to read
 */
bool FileReader::hasNextByte() {
	return this->fileStream->peek() != EOF;
}

/**
 * Closes the file reader
 */
void FileReader::close() {
	if (!this->closed) {
		this->fileStream->close();
		this->closed = true;
	}
}
