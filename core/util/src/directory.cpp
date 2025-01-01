#include "util/directory.h"
#include "util/file.h"
#include "util/logger.h"

#include <filesystem>

Directory::Directory(const std::string& path, bool create_if_not_present)
{
	this->m_dir_path = trim_dir_path(path);

	if (!valid_path(path)) {
		ERROR("Directory::Directory() - Invalid directory path: %s", path.c_str());
	}

	if (create_if_not_present && !exists()) {
		create();
	}
}

std::string Directory::get_name() const
{
	return m_dir_path.substr(m_dir_path.find_last_of(File::SEPARATOR) + 1);
}

int Directory::get_size() const
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


std::string Directory::get_path() const
{
	return m_dir_path;
}

std::string Directory::get_abs_path() const
{
	return std::filesystem::absolute(get_path()).string();
}

std::vector<File> Directory::get_subfiles()
{
	std::vector<File> subfiles;

	// get the contents of this directory
	for (const auto & entry : std::filesystem::directory_iterator(m_dir_path)) {
		if (!entry.is_regular_file()) {
			continue;
		}

		subfiles.push_back(File(string_util::replace_all(entry.path().string(), "\\", File::SEPARATOR)));
	}

	return subfiles;
}

std::vector<File> Directory::get_all_subfiles()
{
	std::vector<File> all_subfiles;
	for (const auto & entry : std::filesystem::recursive_directory_iterator(m_dir_path)) {
		if (entry.is_regular_file()) {
			all_subfiles.push_back(File(string_util::replace_all(entry.path().string(), "\\", File::SEPARATOR)));
		}
	}
	return all_subfiles;
}

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

Directory Directory::get_subdir(const std::string& subdir_path)
{
	return Directory(m_dir_path + File::SEPARATOR + subdir_path);
}

File Directory::get_subfile(const std::string& subfile_path)
{
	return File(m_dir_path + File::SEPARATOR + subfile_path);
}

bool Directory::subdir_exists(const std::string& subdir_path)
{
	return std::filesystem::exists(m_dir_path + File::SEPARATOR + subdir_path);
}

bool Directory::subfile_exists(const std::string& subfile_path)
{
	return std::filesystem::exists(m_dir_path + File::SEPARATOR + subfile_path);
}

bool Directory::exists() const
{
	return std::filesystem::exists(m_dir_path);
}

void Directory::create()
{
	std::filesystem::create_directories(m_dir_path);
}