#include <string>
#include <cstdint>

#ifndef FILE_H
#define FILE_H

class File;
class FileWriter;
class FileReader;

static const std::string SEPARATOR = "\\";

class File {
	public:
		File(std::string fileName, std::string fileDirectory);
		File(std::string filePath);
		~File();
		std::string getFileName();
		std::string getExtension();
		std::string getFilePath();
		uintmax_t getFileSize();
	private:
		void createFileIfNotExist();
		std::string fileName;
		std::string fileDirectory;
		uintmax_t fileSize;
};

class FileWriter {
	public:
		FileWriter(File* file);
		~FileWriter();
		void writeString(std::string text);
		void writeByte(char byte);
		void writeBytes(char* bytes);
		void close();
	private:
		File* file;
		std::ofstream* fileStream;
		bool closed;
};

class FileReader {
	public:
		FileReader(File* file);
		~FileReader();
		std::string readAll();
		char readByte();
		char* readBytes(unsigned int numBytes);
		bool hasNextByte();
		void close();
	private:
		File* file;
		std::ifstream* fileStream;
		bool closed;
};


#endif