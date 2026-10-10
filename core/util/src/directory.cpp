#include "util/directory.h"

#include "util/file.h"
#include "util/logger.h"

#include <filesystem>

Directory::Directory(const std::string &path, bool create_if_not_present)
{
    this->m_dir_path = trim_dir_path(path);

    if (!valid_path(path))
    {
        AEMU_FATAL("Invalid directory path: {}", path);
    }

    if (create_if_not_present && !exists())
    {
        create();
    }
}

std::string Directory::get_name() const
{
    return m_dir_path.substr(m_dir_path.find_last_of(File::SEPARATOR) + 1);
}

std::string Directory::get_path() const
{
    return m_dir_path;
}

std::vector<File> Directory::get_all_subfiles()
{
    std::vector<File> all_subfiles;
    for (const auto &entry : std::filesystem::recursive_directory_iterator(m_dir_path))
    {
        if (entry.is_regular_file())
        {
            all_subfiles.push_back(
                File(entry.path().generic_string()));
        }
    }
    return all_subfiles;
}

bool Directory::subfile_exists(const std::string &subfile_path)
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