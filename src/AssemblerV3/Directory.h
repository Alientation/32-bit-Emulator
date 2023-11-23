#include <string>
#include <vector>
#include "File.h"

#ifndef DIRECTORY_H
#define DIRECTORY_H


class Directory;

class Directory {
	public:
		Directory(const std::string dirPath);
		~Directory();

		std::string path();
		std::vector<File*> subfiles();
		std::vector<Directory*> subdirectories();
		Directory* subdirectory(const std::string subdirectoryPath);
		File* subfile(const std::string subfilePath);

		bool exists();
		void create();

	private:
		std::string dirPath;

};

#endif