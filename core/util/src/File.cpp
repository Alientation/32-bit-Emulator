#include <util/file.h>
#include <util/loggerv2.h>

#include <fstream>

std::string trim_dir_path(const std::string& str)
{
	std::vector<std::string> segments;
	size_t i = 0;
	while(i < str.size()) {
		size_t end = str.find("\\", i);
		if (end == std::string::npos) {
			end = str.size();
		}
		size_t other_separator_end = str.find("/", i);
		if (other_separator_end == std::string::npos) {
			other_separator_end = str.size();
		}
		end = end < other_separator_end ? end : other_separator_end;

		segments.push_back(str.substr(i, end - i));
		i = end+1;

		if (segments.back() == ".") {
			segments.pop_back();
		} else if (segments.back() == "..") {
			segments.pop_back();
			if (segments.size() > 0) {
				segments.pop_back();
			}
		}
	}

	std::string res;
	for (size_t i = 0; i < segments.size(); i++) {
		res += segments[i];
		if (i + 1 < segments.size()) {
			res += File::SEPARATOR;
		}
	}
	return res;
}

/**
 * Constructs a file object with the given file name and directory.
 *
 * @param name the name of the file
 * @param dir the directory of the file
 */
File::File(const std::string& name, const std::string& extension,
		   const std::string& dir, bool create_if_not_present) :
	m_name(name),
	m_extension(extension)
{
	if (dir.empty()) {
		m_dir = std::filesystem::current_path().string();
	} else {
		m_dir = trim_dir_path(dir);
	}

	if (!valid_name(name)) {
		ERROR_SS(std::stringstream() << "File::File() - Invalid file name: " << name);
	} else if (!valid_extension(extension)) {
		ERROR_SS(std::stringstream() << "File::File() - Invalid file extension: " << extension);
	} else if (!valid_dir(dir)) {
		ERROR_SS(std::stringstream() << "File::File() - Invalid file directory: " << dir);
	}

	if (create_if_not_present && !exists()) {
		create();
	}
}

/**
 * Constructs a file object with the given file path.
 *
 * @param path the path of the file
 */
File::File(const std::string& path, bool create_if_not_present)
{
	std::size_t extension_separator_index = path.find_last_of(".");
	if (extension_separator_index == std::string::npos) {
		ERROR_SS(std::stringstream() << "File::File() - File path does not contain an extension: " << path);
	}

	bool has_dir = path.find_last_of(SEPARATOR) == std::string::npos;
	std::string name_and_extension = has_dir ? path : path.substr(path.find_last_of(SEPARATOR) + 1);
	m_name = name_and_extension.substr(0, name_and_extension.find_last_of("."));
	m_extension = name_and_extension.substr(name_and_extension.find_last_of(".") + 1);
	m_dir = has_dir ? "" : trim_dir_path(path.substr(0, path.find_last_of(SEPARATOR)));

	if (!valid_name(m_name)) {
		ERROR_SS(std::stringstream() << "File::File() - Invalid file name: " << m_name);
	} else if (!valid_extension(m_extension)) {
		ERROR_SS(std::stringstream() << "File::File() - Invalid file extension: " << m_extension);
	} else if (!valid_dir(m_dir)) {
		ERROR_SS(std::stringstream() << "File::File() - Invalid file directory: " << m_dir);
	}

	if (create_if_not_present && !exists()) {
		create();
	}
}

File::File() :
	m_name(""),
	m_extension(""),
	m_dir("")
{

}

/**
 * Returns the name of the file
 *
 * @return the name of the file
 */
std::string File::get_name() const
{
	return m_name;
}

/**
 * Returns the extension of the file
 *
 * @return the extension of the file
 */
std::string File::get_extension() const
{
	return m_extension;
}

/**
 * Returns the path of the file
 *
 * @return the path of the file
 */
std::string File::get_path() const
{
	if (m_dir.size() == 0) {
		return m_name + "." + m_extension;
	}
	return m_dir + SEPARATOR + m_name + "." + m_extension;
}

/**
 * Returns the directory of the file
 *
 * @return the directory of the file
 */
std::string File::get_dir() const
{
	return m_dir;
}

/**
 * Gets the size of the file in bytes
 *
 * @return the size of the file in bytes
 */
int File::get_size() const
{
	int fileSize = 0;

	std::ifstream file_stream = std::ifstream(this->get_path(), std::ifstream::in);
	while (file_stream.peek() != EOF) {
		file_stream.get();
		fileSize++;
	}

	return fileSize;
}

/**
 * Returns true if the file exists
 *
 * @return true if the file exists
 */
bool File::exists() const
{
	return std::filesystem::exists(this->get_path());
}

/**
 * Creates the file
 */
void File::create() const
{
	std::filesystem::path fs_path(get_path());

    // Create all necessary directories
    std::filesystem::create_directories(fs_path.parent_path());

	std::ofstream file(get_path());
	file.close();
}



/**
 * Constructs a file writer object with the given file
 *
 * @param file the file to write to
 */
FileWriter::FileWriter(const File& file) :
	m_file(file)
{
	m_file_stream = new std::ofstream(file.get_path(), std::ifstream::out);
	m_closed = false;

	if (!m_file_stream->good()) {
		ERROR_SS(std::stringstream() << "FileWriter::FileWriter() - Failed to open file: " << file.get_path());
	}
}

FileWriter::FileWriter(const File& file, std::_Ios_Openmode flags) :
	m_file(file)
{
	m_file_stream = new std::ofstream(file.get_path(), flags);
	m_closed = false;

	if (!m_file_stream->good()) {
		ERROR_SS(std::stringstream() << "FileWriter::FileWriter() - Failed to open file: " << file.get_path());
	}
}

/**
 * Destructs a file writer object
 */
FileWriter::~FileWriter()
{
	this->close();
}

FileWriter& FileWriter::operator<<(std::string str)
{
	this->write(str);
	return *this;
}

FileWriter& FileWriter::operator<<(char byte)
{
	this->write(byte);
	return *this;
}

FileWriter& FileWriter::operator<<(const char* str)
{
	this->write(str);
	return *this;
}

/**
 * Writes a string to the file
 *
 * @param text the string to write
 */
void FileWriter::write(const std::string text)
{
	if (m_closed) {
		exit(EXIT_FAILURE);
	}

	(*m_file_stream) << text;
    for (size_t i = 0; i < text.size(); i++) {
        m_bytes_written.push_back(text[i]);
    }
}

ByteWriter::Data::Data(unsigned long long value, int num_bytes) : value(value), num_bytes(num_bytes)
{

}

ByteWriter::Data::Data(unsigned long long value, int num_bytes, bool little_endian)
{
	if (little_endian) {
		this->value = value;
	} else {
		for (int i = 0; i < num_bytes; i++) {
			this->value <<= 8;
			this->value += value & 0xFF;
			value >>= 8;
		}
	}
	this->num_bytes = num_bytes;
}

ByteWriter::ByteWriter(FileWriter& filewriter) :
	m_filewriter(filewriter)
{

}

/**
 * @brief 					Writes sequence of bytes in little endian order
 *
 * @param 					data: contains bytes to write and length
 * @return 					reference to byte writer
 */
ByteWriter& ByteWriter::operator<<(Data data)
{
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
void FileWriter::write(const char byte)
{
	if (m_closed) {
		exit(EXIT_FAILURE);
	}

	(*m_file_stream) << byte;

    m_bytes_written.push_back(byte);
}

/**
 * Writes a byte array to the file
 *
 * @param bytes the byte array to write
 */
void FileWriter::write(const char* bytes)
{
	if (m_closed) {
		exit(EXIT_FAILURE);
	}

	(*m_file_stream) << bytes;

    int size = sizeof(bytes);
    for (int i = 0; i < size; i++) {
        m_bytes_written.push_back(bytes[i]);
    }
}


char FileWriter::last_byte_written()
{
    if (m_bytes_written.size() > 0) {
        return m_bytes_written.back();
    }
    return '\0';
}

char* FileWriter::last_bytes_written(unsigned int num_bytes)
{
    char* bytes = new char[num_bytes];

    for (size_t i = std::max(0ULL, num_bytes - m_bytes_written.size()); i < num_bytes; i++) {
        bytes[i] = m_bytes_written[m_bytes_written.size() - num_bytes + i];
    }

    return bytes;
}

void FileWriter::flush()
{
	if (m_closed) {
		// error
		exit(EXIT_FAILURE);
	}

	m_file_stream->flush();
}


/**
 * Closes the file writer
 */
void FileWriter::close()
{
	if (!m_closed) {
		m_closed = true;
		delete m_file_stream;
	}
}

ByteReader::Data::Data(int num_bytes) : num_bytes(num_bytes) { };
ByteReader::Data::Data(int num_bytes, bool little_endian) : num_bytes(num_bytes), little_endian(little_endian) { };

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

bool ByteReader::has_next()
{
	return m_cur_byte < m_bytes.size();
}

unsigned char ByteReader::read_byte(bool little_endian)
{
	ByteReader::Data data(1, little_endian);
	(*this) >> data;
	return data.val;
}

unsigned short ByteReader::read_hword(bool little_endian)
{
	ByteReader::Data data(2, little_endian);
	(*this) >> data;
	return data.val;
}

unsigned int ByteReader::read_word(bool little_endian)
{
	ByteReader::Data data(4, little_endian);
	(*this) >> data;
	return data.val;
}

unsigned long long ByteReader::read_dword(bool little_endian)
{
	ByteReader::Data data(8, little_endian);
	(*this) >> data;
	return data.val;
}

void ByteReader::skip_bytes(int num_bytes)
{
	m_cur_byte += num_bytes;
}


/**
 * Constructs a file reader object with the given file
 *
 * @param file the file to read from
 */
FileReader::FileReader(const File& file) : m_file(file)
{
	m_file_stream = new std::ifstream(m_file.get_path(), std::ifstream::in);
	m_closed = false;

	if (!m_file_stream->good()) {
		ERROR_SS(std::stringstream() << "FileReader::FileReader() - Failed to open file: " << m_file.get_path());
	}
}


FileReader::FileReader(const File& file, std::_Ios_Openmode flags) : m_file(file)
{
	m_file_stream = new std::ifstream(m_file.get_path(), flags);
	m_closed = false;

	if (!m_file_stream->good()) {
		ERROR_SS(std::stringstream() << "FileReader::FileReader() - Failed to open file: " << m_file.get_path());
	}
}


/**
 * Destructs a file reader object
 */
FileReader::~FileReader()
{
	this->close();
}

/**
 * Reads the entire file and returns it as a string
 *
 * @return the entire file as a string
 */
std::string FileReader::read_all()
{
	std::string fileContents;
	while (m_file_stream->peek() != EOF) {
		fileContents += m_file_stream->get();
	}
	close();
	return fileContents;
}

/**
 * Reads a byte from the file
 *
 * @return the byte read from the file
 */
char FileReader::read_byte()
{
	return m_file_stream->get();;
}

/**
 * Returns the next byte to be read from the file without advancing the file pointer
 *
 * @return the next byte to be read from the file
 */
char FileReader::peek_byte()
{
	return m_file_stream->peek();
}

/**
 * Reads a number of bytes from the file
 *
 * @param num_bytes the number of bytes to read
 * @return the bytes read from the file
 */
char* FileReader::read_bytes(const unsigned int num_bytes)
{
	char* bytes = new char[num_bytes];
	m_file_stream->read(bytes, num_bytes);

	if (m_file_stream->fail()) {
		ERROR_SS(std::stringstream() << "FileReader::readBytes() - Failed to read " << num_bytes
				<< " bytes from file: " << m_file.get_path());
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
char* FileReader::read_token(const char token_delimiter) // TODO: make this take in a regex separator
{
	std::string token = "";
	while (m_file_stream->peek() != token_delimiter && m_file_stream->peek() != EOF) {
		token += m_file_stream->get();
	}

	return (char*)token.c_str();
}

/**
 * Returns true if there is another byte to read
 *
 * @return true if there is another byte to read
 */
bool FileReader::has_next_byte()
{
	return m_file_stream->peek() != EOF;
}

/**
 * Closes the file reader
 */
void FileReader::close()
{
	if (!m_closed) {
		delete m_file_stream;
		m_closed = true;
	}
}
