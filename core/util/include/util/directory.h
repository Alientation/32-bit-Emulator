#pragma once

#include "util/file.h"

#include <string>
#include <vector>

/// A directory on the file system, addressed by its path.
class Directory
{
  public:
    /// @param path the path to check
    /// @return whether the path has none of the characters that a path cannot contain
    static bool valid_path(const std::string &path)
    {
        return path.find_first_of("*?\"<>|") == std::string::npos;
    }

    /// Constructs a directory with the given path.
    ///
    /// @param path the path of the directory
    /// @param create_if_not_present whether to create the directory when it does not exist
    Directory(const std::string &path, bool create_if_not_present = false);

    /// @return the name of the directory
    std::string get_name() const;

    /// @return size in bytes of the directory, including all subdirectories and their contents
    int get_size() const;

    /// @return relative path to the directory
    std::string get_path() const;

    /// @return absolute path to the directory
    std::string get_abs_path() const;

    /// @return subfiles of the directory, not including any subdirectories or its contents
    std::vector<File> get_subfiles();

    /// @return all files located underneath this directory
    std::vector<File> get_all_subfiles();

    /// @return subdirectories of the directory, not including any subfiles
    std::vector<Directory> get_subdirs();

    /// Returns the subdirectory of a path relative to the current directory
    ///
    /// @param subdir_path the path of the subdirectory relative to the current directory
    ///
    /// @return the subdirectory of a path relative path
    Directory get_subdir(const std::string &subdir_path);

    /// Returns the subfile of a path relative to the current directory
    ///
    /// @param subfile_path the path of the subfile relative to the current directory
    ///
    /// @return the subfile of a path relative path
    File get_subfile(const std::string &subfile_path);

    /// Returns whether or not the subdirectory exists
    ///
    /// @param subdir_path the path of the subdirectory relative to the current directory
    ///
    /// @return whether or not the subdirectory exists
    bool subdir_exists(const std::string &subdir_path);

    /// Returns whether or not the subfile exists
    ///
    /// @param subfile_path the path of the subfile relative to the current directory
    ///
    /// @return whether or not the subfile exists
    bool subfile_exists(const std::string &subfile_path);

    /// @return whether or not the directory exists
    bool exists() const;

    /// Creates the directory
    void create();

  private:
    std::string m_dir_path;
};