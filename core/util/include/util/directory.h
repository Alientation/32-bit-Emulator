#pragma once

#include "util/file.h"

#include <string>
#include <vector>

/// A directory on the file system, addressed by its path.
class Directory
{
  public:
    /// @param[in] path The path to check.
    /// @return Whether the path has none of the characters that a path cannot contain.
    static bool valid_path(const std::string &path)
    {
        return path.find_first_of("*?\"<>|") == std::string::npos;
    }

    /// Constructs a directory with the given path. An invalid path is a fatal error.
    ///
    /// @param[in] path The path of the directory.
    /// @param[in] create_if_not_present Whether to create the directory when it does not exist.
    Directory(const std::string &path, bool create_if_not_present = false);

    /// @return The name of the directory: the last segment of its path.
    std::string get_name() const;

    /// @return The path of the directory as it was given, normalized by trim_dir_path ().
    std::string get_path() const;

    /// @return The regular files in this directory and in the directories below it. Fails if the
    ///     directory does not exist.
    std::vector<File> get_all_subfiles();

    /// @param[in] subfile_path The path of a file or directory, relative to this directory.
    /// @return Whether it exists.
    bool subfile_exists(const std::string &subfile_path);

    /// @return Whether the directory exists.
    bool exists() const;

    /// Creates the directory, and the directories above it that do not exist.
    void create();

  private:
    std::string m_dir_path;
};