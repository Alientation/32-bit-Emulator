#include "Directory.h"

#include <filesystem>

/**
 * Constructs a directory with the given path
 */
Directory::Directory(std::string dirPath) {
	this->dirPath = dirPath;
}

/**
 * Destructs a directory
 */
Directory::~Directory() {

}

/**
 * Returns the path of the directory
 * 
 * @return the path of the directory
 */
std::string Directory::path() {
	return dirPath;
}

/**
 * Returns the subfiles of the directory, not including any subdirectories or its contents
 * 
 * @return the subfiles of the directory
 */
std::vector<File*> Directory::subfiles() {
	std::vector<File*> subfiles;

	// get the contents of this directory
	for (const auto & entry : std::filesystem::directory_iterator(dirPath)) {
		if (!entry.is_regular_file()) {
			continue;
		}


		std::string path = entry.path().string();
		std::string fileName = path.substr(path.find_last_of(SEPARATOR) + 1);
		std::string fileExtension = fileName.substr(fileName.find_last_of(".") + 1);

		subfiles.push_back(new File(fileName, fileExtension, dirPath));
	}

	return subfiles;
}

/**
 * Returns the subdirectories of the directory, not including any subfiles
 * 
 * @return the subdirectories of the directory
 */
std::vector<Directory*> Directory::subdirectories() {
	std::vector<Directory*> subdirs;

	// get the contents of the directory
	for (const auto & entry : std::filesystem::directory_iterator(dirPath)) {
		if (!entry.is_directory()) {
			continue;
		}

		std::string path = entry.path().string();
		subdirs.push_back(new Directory(path));
	}

	return subdirs;
}

/**
 * Returns the subdirectory of a path relative to the current directory
 * 
 * @param subdirectoryPath the path of the subdirectory relative to the current directory
 * 
 * @return the subdirectory of a path relative path
 */
Directory* Directory::subdirectory(const std::string subdirectoryPath) {
	Directory* subdir;

	return subdir;
}

/**
 * Returns the subfile of a path relative to the current directory
 * 
 * @param subfilePath the path of the subfile relative to the current directory
 * 
 * @return the subfile of a path relative path
 */
File* Directory::subfile(const std::string subfilePath) {
	File* subfile;

	return subfile;
}

/**
 * Returns whether or not the directory exists
 * 
 * @return whether or not the directory exists
 */
bool Directory::exists() {
	return std::filesystem::exists(dirPath);
}

/**
 * Creates the directory
 */
void Directory::create() {
	std::filesystem::create_directories(dirPath);
}