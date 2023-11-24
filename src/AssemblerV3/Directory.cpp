#include "Directory.h"
#include "Logger.h"

#include <filesystem>

/**
 * Constructs a directory with the given path
 */
Directory::Directory(std::string dirPath, bool createDirectoryIfNotPresent) {
	this->dirPath = dirPath;

	if (!isValidDirectoryPath(dirPath)) {
		log(ERROR, std::stringstream() << "Directory::Directory() - Invalid directory path: " << dirPath);
	}

	if (createDirectoryIfNotPresent && !exists()) {
		create();
	}
}

/**
 * Destructs a directory
 */
Directory::~Directory() {

}

/**
 * Returns the name of the directory
 * 
 * @return the name of the directory
 */
std::string Directory::getDirectoryName() {
	return dirPath.substr(dirPath.find_last_of(SEPARATOR) + 1);
}

/**
 * Returns the size of the directory, including all subdirectories and their contents
 * 
 * @return the size of the directory
 */
int Directory::getDirectorySize() {
	int size = 0;

	for (const auto & entry : std::filesystem::directory_iterator(dirPath)) {
		if (entry.is_directory()) {
			Directory* subdir = new Directory(entry.path().string());
			size += subdir->getDirectorySize();
		} else if (entry.is_regular_file()) {
			size += std::filesystem::file_size(entry.path());
		}
	}

	return size;
}


/**
 * Returns the path of the directory
 * 
 * @return the path of the directory
 */
std::string Directory::getDirectoryPath() {
	return dirPath;
}

/**
 * Returns the subfiles of the directory, not including any subdirectories or its contents
 * 
 * @return the subfiles of the directory
 */
std::vector<File*> Directory::getSubfiles() {
	std::vector<File*> subfiles;

	// get the contents of this directory
	for (const auto & entry : std::filesystem::directory_iterator(dirPath)) {
		if (!entry.is_regular_file()) {
			continue;
		}

		subfiles.push_back(new File(entry.path().string()));
	}

	return subfiles;
}

/**
 * Returns the subdirectories of the directory, not including any subfiles
 * 
 * @return the subdirectories of the directory
 */
std::vector<Directory*> Directory::getSubdirectories() {
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
Directory* Directory::getSubdirectory(const std::string subdirectoryPath) {
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
File* Directory::getSubfile(const std::string subfilePath) {
	File* subfile;

	return subfile;
}

/**
 * Returns whether or not the subdirectory exists
 * 
 * @param subdirectoryPath the path of the subdirectory relative to the current directory
 * 
 * @return whether or not the subdirectory exists
 */
bool Directory::subdirectoryExists(const std::string subdirectoryPath) {
	return std::filesystem::exists(dirPath + SEPARATOR + subdirectoryPath);
}

/**
 * Returns whether or not the subfile exists
 * 
 * @param subfilePath the path of the subfile relative to the current directory
 * 
 * @return whether or not the subfile exists
 */
bool Directory::subfileExists(const std::string subfilePath) {
	return std::filesystem::exists(dirPath + SEPARATOR + subfilePath);
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