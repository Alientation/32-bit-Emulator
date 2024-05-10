#include "util/File.h"
#include "util/Logger.h"

#include <fstream>
#include <filesystem>

/**
 * Constructs a file object with the given file name and directory.
 * 
 * @param fileName the name of the file
 * @param fileDirectory the directory of the file
 */
File::File(const std::string fileName, const std::string fileExtension, const std::string fileDirectory = "", bool createFileIfNotPresent) {
	this->fileName = fileName;
	this->fileExtension = fileExtension;

	if (fileDirectory.empty()) {
		this->fileDirectory = std::filesystem::current_path().string();
	} else {
		this->fileDirectory = fileDirectory;
	}

	if (!isValidFileName(fileName)) {
		log(LogType::ERROR, std::stringstream() << "File::File() - Invalid file name: " << fileName);
	} else if (!isValidFileExtension(fileExtension)) {
		log(LogType::ERROR, std::stringstream() << "File::File() - Invalid file extension: " << fileExtension);
	} else if (!isValidFileDirectory(fileDirectory)) {
		log(LogType::ERROR, std::stringstream() << "File::File() - Invalid file directory: " << fileDirectory);
	}

	if (createFileIfNotPresent && !exists()) {
		create();
	}
}

/**
 * Constructs a file object with the given file path.
 * 
 * @param filePath the path of the file
 */
File::File(const std::string filePath, bool createFileIfNotPresent) {
	std::size_t extensionSeparatorIndex = filePath.find_last_of(".");
	if (extensionSeparatorIndex == std::string::npos) {
		log(LogType::ERROR, std::stringstream() << "File::File() - File path does not contain an extension: " << filePath);
	}

	std::string fileNameAndExtension = filePath.substr(filePath.find_last_of(SEPARATOR) + 1);
	this->fileName = fileNameAndExtension.substr(0, fileNameAndExtension.find_last_of("."));
	this->fileExtension = fileNameAndExtension.substr(fileNameAndExtension.find_last_of(".") + 1);
	this->fileDirectory = filePath.substr(0, filePath.find_last_of(SEPARATOR));

	if (!isValidFileName(fileName)) {
		log(LogType::ERROR, std::stringstream() << "File::File() - Invalid file name: " << fileName);
	} else if (!isValidFileExtension(fileExtension)) {
		log(LogType::ERROR, std::stringstream() << "File::File() - Invalid file extension: " << fileExtension);
	} else if (!isValidFileDirectory(fileDirectory)) {
		log(LogType::ERROR, std::stringstream() << "File::File() - Invalid file directory: " << fileDirectory);
	}

	if (createFileIfNotPresent && !exists()) {
		create();
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
 * Returns true if the file exists
 * 
 * @return true if the file exists
 */
bool File::exists() {
	return std::filesystem::exists(this->getFilePath());
}

/**
 * Creates the file
 */
void File::create() {
	std::ofstream file(this->getFilePath());
	file.close();
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
		log(LogType::ERROR, std::stringstream() << "FileWriter::FileWriter() - Failed to open file: " << file->getFilePath());
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

    for (int i = 0; i < text.size(); i++) {
        this->bytes_written.push_back(text[i]);
    }
}

/**
 * Writes a byte to the file
 * 
 * @param byte the byte to write
 */
void FileWriter::writeByte(const char byte) {
	(*this->fileStream) << byte;

    this->bytes_written.push_back(byte);
}

/**
 * Writes a byte array to the file
 * 
 * @param bytes the byte array to write
 */
void FileWriter::writeBytes(char* bytes) {
	(*this->fileStream) << bytes;

    int size = sizeof(bytes);
    for (int i = 0; i < size; i++) {
        this->bytes_written.push_back(bytes[i]);
    }
}


char FileWriter::lastByteWritten() {
    if (this->bytes_written.size() > 0) {
        return this->bytes_written.back();
    }
    return '\0';
}

char* FileWriter::lastBytesWritten(unsigned int numBytes) {
    char* bytes = new char[numBytes];

    for (int i = std::max(0ULL, numBytes - bytes_written.size()); i < numBytes; i++) {
        bytes[i] = this->bytes_written[this->bytes_written.size() - numBytes + i];
    }

    return bytes;
}


/**
 * Closes the file writer
 */
void FileWriter::close() {
	if (!this->closed) {	
		this->closed = true;
		delete fileStream;
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
		log(LogType::ERROR, std::stringstream() << "FileReader::FileReader() - Failed to open file: " << file->getFilePath());
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
	while (fileStream->peek() != EOF) {
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
 * Returns the next byte to be read from the file without advancing the file pointer
 * 
 * @return the next byte to be read from the file
 */
char FileReader::peekByte() {
	return this->fileStream->peek();
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
		log(LogType::ERROR, std::stringstream() << "FileReader::readBytes() - Failed to read " << 
				numBytes << " bytes from file: " << this->file->getFilePath());
	}

	return bytes;
}

/**
 * Reads all bytes from the file till the next token delimiter is encountered
 * 
 * @param delimiter the delimiter to stop reading at
 * 
 * @return the bytes read from the file
 */
char* FileReader::readToken(const char tokenDelimiter) { // TODO: make this take in a regex separator
	std::string token = "";
	while (this->fileStream->peek() != tokenDelimiter && this->fileStream->peek() != EOF) {
		token += this->fileStream->get();
	}

	return (char*)token.c_str();
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
		delete fileStream;
		this->closed = true;
	}
}
