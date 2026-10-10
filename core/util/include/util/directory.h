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

    /// @return relative path to the directory
    std::string get_path() const;

    /// @return all files located underneath this directory
    std::vector<File> get_all_subfiles();

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