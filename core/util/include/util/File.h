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

std::string trim_dir_path(const std::string& str);

// TODO probably best to add some form of thread safe locking to the file operations
// todo, automatically convert separators
class File {
	public:
		inline static const std::string SEPARATOR = std::string(1, std::filesystem::path::preferred_separator);

		static bool valid_name(const std::string& name) {
			return name.find_first_of("\\/:*?\"<>|") == std::string::npos && name.size() > 0;
		}

		static bool valid_extension(const std::string& extension) {
			return extension.find_first_of("\\/:*?\"<>|") == std::string::npos && extension.size() > 0;
		}

		static bool valid_dir(const std::string& dir) {
			return dir.find_first_of("*?\"<>|") == std::string::npos;
		}

		static bool valid_path(const std::string& path) {
			return path.find_first_of("*?\"<>|") == std::string::npos;
		}

		File(const std::string& name, const std::string& extension, const std::string& dir = "", bool create_if_not_present = false);
		File(const std::string& path, bool create_if_not_present = false);
		File();

		std::string get_name() const;
		std::string get_extension() const;
		std::string get_path() const;
		std::string get_dir() const;
		int get_size() const;
		bool exists() const;
		void create() const;
	private:
		std::string m_name;
		std::string m_extension;
		std::string m_dir;
};

class FileWriter {
	public:
		FileWriter(const File& file);
		FileWriter(const File& file, std::_Ios_Openmode flags);
		~FileWriter();
		FileWriter& operator<<(std::string);
		FileWriter& operator<<(char byte);
		FileWriter& operator<<(const char* bytes);

		void write(std::string text);
		void write(char byte);
		void write(const char* bytes);
        char last_byte_written();
        char* last_bytes_written(unsigned int numBytes);
		void flush();
		void close();
	private:
		File m_file;
        std::vector<char> m_bytes_written;
		std::ofstream* m_file_stream;
		bool m_closed;
};

class ByteWriter {
	public:
		ByteWriter(FileWriter& filewriter);
		struct Data {
			unsigned long long value;
			int num_bytes;
			Data(unsigned long long value, int num_bytes);
			Data(unsigned long long value, int num_bytes, bool little_endian);
		};

		ByteWriter& operator<<(Data data);
	private:
		FileWriter& m_filewriter;
};

class FileReader {
	public:
		FileReader(const File& file);
		FileReader(const File& file, std::_Ios_Openmode flags);
		~FileReader();
		std::string read_all();
		char read_byte();
		char peek_byte();
		char* read_bytes(unsigned int num_bytes);
		char* read_token(char token_delimiter);
		bool has_next_byte();
		void close();
	private:
		File m_file;
		std::ifstream* m_file_stream;
		bool m_closed;
};

class ByteReader {
	public:
		ByteReader(std::vector<unsigned char> &bytes) : m_bytes(bytes) {};
		struct Data {
			unsigned long long val = 0;
			int num_bytes = 0;
			bool little_endian = true;
			Data(int num_bytes);
			Data(int num_bytes, bool little_endian);
		};

		ByteReader& operator>>(Data &data);
		unsigned char read_byte(bool little_endian = true);
		unsigned short read_hword(bool little_endian = true);
		unsigned int read_word(bool little_endian = true);
		unsigned long long read_dword(bool little_endian = true);
		void skip_bytes(int num_bytes);
	private:
		std::vector<unsigned char>& m_bytes;
		int m_cur_byte = 0;
};


#endif