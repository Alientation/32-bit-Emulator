#pragma once
#ifndef FILE_H
#define FILE_H

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

class File;
class FileWriter;
class FileReader;

// TODO probably best to add some form of thread safe locking to the file operations
class File {
	public:
		inline static const std::string SEPARATOR = std::string(1, std::filesystem::path::preferred_separator);

		static bool isValidFileName(const std::string fileName) {
			return fileName.find_first_of("\\/:*?\"<>|") == std::string::npos && fileName.size() > 0;
		}

		static bool isValidFileExtension(const std::string fileExtension) {
			return fileExtension.find_first_of("\\/:*?\"<>|") == std::string::npos && fileExtension.size() > 0;
		}

		static bool isValidFileDirectory(const std::string directory) {
			return directory.find_first_of("*?\"<>|") == std::string::npos;
		}

		static bool isValidFilePath(const std::string filepath) {
			return filepath.find_first_of("*?\"<>|") == std::string::npos;
		}

		File(std::string fileName, std::string fileExtension, std::string fileDirectory, bool createFileIfNotPresent = false);
		File(std::string filePath, bool createFileIfNotPresent = false);
		~File();

		std::string getFileName();
		std::string getExtension();
		std::string getFilePath();
		std::string getFileDirectory();
		int getFileSize();
		bool exists();
		void create();
	private:
		std::string fileName;
		std::string fileExtension;
		std::string fileDirectory;
};

class ByteWriter {
	public:
		ByteWriter(FileWriter *filewriter);
		struct Data {
			unsigned long long value;
			int num_bytes;
			Data(unsigned long long value, int num_bytes) : value(value), num_bytes(num_bytes) {}
			Data(unsigned long long value, int num_bytes, bool little_endian) {
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
		};

		ByteWriter& operator<<(Data data);
	private:
		FileWriter *filewriter;
};

class FileWriter {
	public:
		FileWriter(File* file);
		FileWriter(File* file, std::_Ios_Openmode flags);
		~FileWriter();
		FileWriter& operator<<(std::string);
		FileWriter& operator<<(char byte);
		FileWriter& operator<<(const char* bytes);

		void write(std::string text);
		void write(char byte);
		void write(const char* bytes);
        char lastByteWritten();
        char* lastBytesWritten(unsigned int numBytes);
		void flush();
		void close();
	private:
		File* file;
        std::vector<char> bytes_written;
		std::ofstream* fileStream;
		bool closed;
};

class ByteReader {
	public:
		ByteReader(std::vector<unsigned char> &bytes) : bytes(bytes) {};
		struct Data {
			unsigned long long val = 0;
			int num_bytes = 0;
			bool little_endian = true;
			Data(int num_bytes) : num_bytes(num_bytes) {};
			Data(int num_bytes, bool little_endian) : num_bytes(num_bytes), little_endian(little_endian) {};
		};

		ByteReader& operator>>(Data &data);
		unsigned char read_byte(bool little_endian = true);
		unsigned short read_hword(bool little_endian = true);
		unsigned int read_word(bool little_endian = true);
		unsigned long long read_dword(bool little_endian = true);
		void skip_bytes(int num_bytes);
	private:
		std::vector<unsigned char> &bytes;
		int cur_byte = 0;
};

class FileReader {
	public:
		FileReader(File* file);
		FileReader(File* file, std::_Ios_Openmode flags);
		~FileReader();
		std::string readAll();
		char readByte();
		char peekByte();
		char* readBytes(unsigned int numBytes);
		char* readToken(char tokenDelimiter);
		bool hasNextByte();
		void close();
	private:
		File* file;
		std::ifstream* fileStream;
		bool closed;
};


#endif