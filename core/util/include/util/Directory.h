#include <string>
#include <vector>
#include "File.h"

#ifndef DIRECTORY_H
#define DIRECTORY_H


class Directory;

class Directory {
	public:
		static bool valid_path(const std::string& path) {
			return path.find_first_of("*?\"<>|") == std::string::npos;
		}

		Directory(const std::string& path, bool create_if_not_present = false);
		~Directory();

		std::string get_name();
		std::string get_path();
		int get_size();
		std::vector<File*> get_subfiles();
		std::vector<Directory*> get_subdirs();
		Directory* get_subdir(const std::string& subdir_path);
		File* get_subfile(const std::string& subfile_path);

		bool subdir_exists(const std::string& subdir_path);
		bool subfile_exists(const std::string& subfile_path);

		bool exists();
		void create();

	private:
		std::string m_dir_path;

};

#endif