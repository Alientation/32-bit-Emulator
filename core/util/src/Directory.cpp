#include "util/directory.h"
#include "util/file.h"
#include "util/logger.h"

#include <filesystem>

/**
 * Constructs a directory with the given path
 */
Directory::Directory(const std::string& path, bool create_if_not_present)
{
	this->m_dir_path = trim_dir_path(path);

	if (!valid_path(path)) {
		ERROR_SS(std::stringstream() << "Directory::Directory() - Invalid directory path: " << path);
	}

	if (create_if_not_present && !exists()) {
		create();
	}
}

/**
 * Returns the name of the directory
 *
 * @return the name of the directory
 */
std::string Directory::get_name()
{
	return m_dir_path.substr(m_dir_path.find_last_of(File::SEPARATOR) + 1);
}

/**
 * Returns the size of the directory, including all subdirectories and their contents
 *
 * @return the size of the directory
 */
int Directory::get_size()
{
	int size = 0;

	for (const auto & entry : std::filesystem::directory_iterator(m_dir_path)) {
		if (entry.is_directory()) {
			Directory* subdir = new Directory(entry.path().string());
			size += subdir->get_size();
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
std::string Directory::get_path()
{
	return m_dir_path;
}

/**
 * Returns the subfiles of the directory, not including any subdirectories or its contents
 *
 * @return the subfiles of the directory
 */
std::vector<File> Directory::get_subfiles()
{
	std::vector<File> subfiles;

	// get the contents of this directory
	for (const auto & entry : std::filesystem::directory_iterator(m_dir_path)) {
		if (!entry.is_regular_file()) {
			continue;
		}

		subfiles.push_back(File(entry.path().string()));
	}

	return subfiles;
}

/**
 * @brief 					Returns all files located underneath this directory
 *
 * @return 					List of subfiles
 */
std::vector<File> Directory::get_all_subfiles()
{
	std::vector<File> all_subfiles;
	for (const auto & entry : std::filesystem::recursive_directory_iterator(m_dir_path)) {
		if (entry.is_regular_file()) {
			all_subfiles.push_back(File(entry.path().string()));
		}
	}
	return all_subfiles;
}


/**
 * Returns the subdirectories of the directory, not including any subfiles
 *
 * @return the subdirectories of the directory
 */
std::vector<Directory> Directory::get_subdirs()
{
	std::vector<Directory> subdirs;

	// get the contents of the directory
	for (const auto & entry : std::filesystem::directory_iterator(m_dir_path)) {
		if (!entry.is_directory()) {
			continue;
		}

		std::string path = entry.path().string();
		subdirs.push_back(Directory(path));
	}

	return subdirs;
}

/**
 * Returns the subdirectory of a path relative to the current directory
 *
 * @param subdir_path the path of the subdirectory relative to the current directory
 *
 * @return the subdirectory of a path relative path
 */
Directory Directory::get_subdir(const std::string& subdir_path)
{
	return Directory(m_dir_path + File::SEPARATOR + subdir_path);
}

/**
 * Returns the subfile of a path relative to the current directory
 *
 * @param subfile_path the path of the subfile relative to the current directory
 *
 * @return the subfile of a path relative path
 */
File Directory::get_subfile(const std::string& subfile_path)
{
	return File(m_dir_path + File::SEPARATOR + subfile_path);
}

/**
 * Returns whether or not the subdirectory exists
 *
 * @param subdir_path the path of the subdirectory relative to the current directory
 *
 * @return whether or not the subdirectory exists
 */
bool Directory::subdir_exists(const std::string& subdir_path)
{
	return std::filesystem::exists(m_dir_path + File::SEPARATOR + subdir_path);
}

/**
 * Returns whether or not the subfile exists
 *
 * @param subfile_path the path of the subfile relative to the current directory
 *
 * @return whether or not the subfile exists
 */
bool Directory::subfile_exists(const std::string& subfile_path)
{
	return std::filesystem::exists(m_dir_path + File::SEPARATOR + subfile_path);
}

/**
 * Returns whether or not the directory exists
 *
 * @return whether or not the directory exists
 */
bool Directory::exists()
{
	return std::filesystem::exists(m_dir_path);
}

/**
 * Creates the directory
 */
void Directory::create()
{
	std::filesystem::create_directories(m_dir_path);
}