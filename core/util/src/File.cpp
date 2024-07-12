#include <util/File.h>
#include <util/Logger.h>

#include <fstream>

/**
 * Constructs a file object with the given file name and directory.
 *
 * @param fileName the name of the file
 * @param fileDirectory the directory of the file
 */
File::File(const std::string fileName, const std::string fileExtension, const std::string fileDirectory = "", bool createFileIfNotPresent)
		: m_fileName(fileName), m_fileExtension(fileExtension) {
	if (fileDirectory.empty()) {
		m_fileDirectory = std::filesystem::current_path().string();
	} else {
		m_fileDirectory = fileDirectory;
	}

	if (!isValidFileName(fileName)) {
		lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "File::File() - Invalid file name: " << fileName);
	} else if (!isValidFileExtension(fileExtension)) {
		lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "File::File() - Invalid file extension: " << fileExtension);
	} else if (!isValidFileDirectory(fileDirectory)) {
		lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "File::File() - Invalid file directory: " << fileDirectory);
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
		lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "File::File() - File path does not contain an extension: " << filePath);
	}

	std::string fileNameAndExtension = filePath.substr(filePath.find_last_of(SEPARATOR) + 1);
	m_fileName = fileNameAndExtension.substr(0, fileNameAndExtension.find_last_of("."));
	m_fileExtension = fileNameAndExtension.substr(fileNameAndExtension.find_last_of(".") + 1);
	m_fileDirectory = filePath.substr(0, filePath.find_last_of(SEPARATOR));

	if (!isValidFileName(m_fileName)) {
		lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "File::File() - Invalid file name: " << m_fileName);
	} else if (!isValidFileExtension(m_fileExtension)) {
		lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "File::File() - Invalid file extension: " << m_fileExtension);
	} else if (!isValidFileDirectory(m_fileDirectory)) {
		lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "File::File() - Invalid file directory: " << m_fileDirectory);
	}

	if (createFileIfNotPresent && !exists()) {
		create();
	}
}

File::File() : m_fileName(""), m_fileDirectory(""), m_fileExtension("") { }

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
std::string File::getFileName() const {
	return m_fileName;
}

/**
 * Returns the extension of the file
 *
 * @return the extension of the file
 */
std::string File::getExtension() const {
	return m_fileExtension;
}

/**
 * Returns the path of the file
 *
 * @return the path of the file
 */
std::string File::getFilePath() const {
	return m_fileDirectory + SEPARATOR + m_fileName + "." + m_fileExtension;
}

/**
 * Returns the directory of the file
 *
 * @return the directory of the file
 */
std::string File::getFileDirectory() const {
	return m_fileDirectory;
}

/**
 * Gets the size of the file in bytes
 *
 * @return the size of the file in bytes
 */
int File::getFileSize() const {
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
bool File::exists() const {
	return std::filesystem::exists(this->getFilePath());
}

/**
 * Creates the file
 */
void File::create() const {
	std::ofstream file(this->getFilePath());
	file.close();
}



/**
 * Constructs a file writer object with the given file
 *
 * @param file the file to write to
 */
FileWriter::FileWriter(const File& file) : m_file(file) {
	m_fileStream = new std::ofstream(file.getFilePath(), std::ifstream::out);
	m_closed = false;

	if (!m_fileStream->good()) {
		lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "FileWriter::FileWriter() - Failed to open file: " << file.getFilePath());
	}
}

FileWriter::FileWriter(const File& file, std::_Ios_Openmode flags) : m_file(file) {
	m_fileStream = new std::ofstream(file.getFilePath(), flags);
	m_closed = false;

	if (!m_fileStream->good()) {
		lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "FileWriter::FileWriter() - Failed to open file: " << file.getFilePath());
	}
}

/**
 * Destructs a file writer object
 */
FileWriter::~FileWriter() {
	this->close();
}

FileWriter& FileWriter::operator<<(std::string str) {
	this->write(str);
	return *this;
}

FileWriter& FileWriter::operator<<(char byte) {
	this->write(byte);
	return *this;
}

FileWriter& FileWriter::operator<<(const char* str) {
	this->write(str);
	return *this;
}

/**
 * Writes a string to the file
 *
 * @param text the string to write
 */
void FileWriter::write(const std::string text) {
	if (m_closed) {
		exit(EXIT_FAILURE);
	}

	(*m_fileStream) << text;
    for (int i = 0; i < text.size(); i++) {
        m_bytes_written.push_back(text[i]);
    }
}


ByteWriter::ByteWriter(FileWriter& filewriter) : m_filewriter(filewriter) { }

/**
 * @brief 					Writes sequence of bytes in little endian order
 *
 * @param 					data: contains bytes to write and length
 * @return 					reference to byte writer
 */
ByteWriter& ByteWriter::operator<<(Data data) {
	for (int i = 0; i < data.num_bytes; i++) {
		m_filewriter.write(data.value & 0xFF);
		data.value >>= 8;
	}
	return (*this);
}

/**
 * Writes a byte to the file
 *
 * @param byte the byte to write
 */
void FileWriter::write(const char byte) {
	if (m_closed) {
		exit(EXIT_FAILURE);
	}

	(*m_fileStream) << byte;

    m_bytes_written.push_back(byte);
}

/**
 * Writes a byte array to the file
 *
 * @param bytes the byte array to write
 */
void FileWriter::write(const char* bytes) {
	if (m_closed) {
		exit(EXIT_FAILURE);
	}

	(*m_fileStream) << bytes;

    int size = sizeof(bytes);
    for (int i = 0; i < size; i++) {
        m_bytes_written.push_back(bytes[i]);
    }
}


char FileWriter::lastByteWritten() {
    if (m_bytes_written.size() > 0) {
        return m_bytes_written.back();
    }
    return '\0';
}

char* FileWriter::lastBytesWritten(unsigned int numBytes) {
    char* bytes = new char[numBytes];

    for (int i = std::max(0ULL, numBytes - m_bytes_written.size()); i < numBytes; i++) {
        bytes[i] = m_bytes_written[m_bytes_written.size() - numBytes + i];
    }

    return bytes;
}

void FileWriter::flush() {
	if (m_closed) {
		// error
		exit(EXIT_FAILURE);
	}

	m_fileStream->flush();
}


/**
 * Closes the file writer
 */
void FileWriter::close() {
	if (!m_closed) {
		m_closed = true;
		delete m_fileStream;
	}
}



ByteReader& ByteReader::operator>>(ByteReader::Data &data) {
	if (data.little_endian) {
		for (int i = data.num_bytes-1; i >= 0; i--) {
			data.val <<= 8;
			data.val += m_bytes.at(m_cur_byte + i);
		}
		m_cur_byte += data.num_bytes;
	} else {
		for (int i = data.num_bytes-1; i >= 0; i--) {
			data.val += ((unsigned long long) m_bytes.at(m_cur_byte)) << (8 * i);
			m_cur_byte++;
		}
	}

	return (*this);
}

unsigned char ByteReader::read_byte(bool little_endian) {
	ByteReader::Data data(1, little_endian);
	(*this) >> data;
	return data.val;
}

unsigned short ByteReader::read_hword(bool little_endian) {
	ByteReader::Data data(2, little_endian);
	(*this) >> data;
	return data.val;
}

unsigned int ByteReader::read_word(bool little_endian) {
	ByteReader::Data data(4, little_endian);
	(*this) >> data;
	return data.val;
}

unsigned long long ByteReader::read_dword(bool little_endian) {
	ByteReader::Data data(8, little_endian);
	(*this) >> data;
	return data.val;
}

void ByteReader::skip_bytes(int num_bytes) {
	m_cur_byte += num_bytes;
}


/**
 * Constructs a file reader object with the given file
 *
 * @param file the file to read from
 */
FileReader::FileReader(const File& file) : m_file(file) {
	m_fileStream = new std::ifstream(m_file.getFilePath(), std::ifstream::in);
	m_closed = false;

	if (!m_fileStream->good()) {
		lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "FileReader::FileReader() - Failed to open file: " << m_file.getFilePath());
	}
}


FileReader::FileReader(const File& file, std::_Ios_Openmode flags) : m_file(file) {
	m_fileStream = new std::ifstream(m_file.getFilePath(), flags);
	m_closed = false;

	if (!m_fileStream->good()) {
		lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "FileReader::FileReader() - Failed to open file: " << m_file.getFilePath());
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
	while (m_fileStream->peek() != EOF) {
		fileContents += m_fileStream->get();
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
	return m_fileStream->get();;
}

/**
 * Returns the next byte to be read from the file without advancing the file pointer
 *
 * @return the next byte to be read from the file
 */
char FileReader::peekByte() {
	return m_fileStream->peek();
}

/**
 * Reads a number of bytes from the file
 *
 * @param numBytes the number of bytes to read
 * @return the bytes read from the file
 */
char* FileReader::readBytes(const unsigned int numBytes) {
	char* bytes = new char[numBytes];
	m_fileStream->read(bytes, numBytes);

	if (m_fileStream->fail()) {
		lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "FileReader::readBytes() - Failed to read " <<
				numBytes << " bytes from file: " << m_file.getFilePath());
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
	while (m_fileStream->peek() != tokenDelimiter && m_fileStream->peek() != EOF) {
		token += m_fileStream->get();
	}

	return (char*)token.c_str();
}

/**
 * Returns true if there is another byte to read
 *
 * @return true if there is another byte to read
 */
bool FileReader::hasNextByte() {
	return m_fileStream->peek() != EOF;
}

/**
 * Closes the file reader
 */
void FileReader::close() {
	if (!m_closed) {
		delete m_fileStream;
		m_closed = true;
	}
}
