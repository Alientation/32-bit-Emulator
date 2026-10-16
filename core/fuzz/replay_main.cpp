// Runs the inputs of a fuzz target without libFuzzer, so that the saved inputs (corpus/<target>)
// are a test of every build, with any compiler. Each argument is a file or a directory of files.

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

/// Feeds one file to the target.
static void run_file(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    const std::vector<char> bytes((std::istreambuf_iterator<char>(in)),
                                  std::istreambuf_iterator<char>());
    LLVMFuzzerTestOneInput(reinterpret_cast<const uint8_t *>(bytes.data()), bytes.size());
}

int main(int argc, char **argv)
{
    size_t count = 0;
    for (int i = 1; i < argc; i++)
    {
        const std::filesystem::path arg(argv[i]);
        std::vector<std::filesystem::path> files;
        if (std::filesystem::is_directory(arg))
        {
            for (const std::filesystem::directory_entry &entry :
                 std::filesystem::directory_iterator(arg))
            {
                files.push_back(entry.path());
            }
        }
        else
        {
            files.push_back(arg);
        }

        for (const std::filesystem::path &file : files)
        {
            // Printed first, so that the input of a crash is the last line.
            std::cout << file.string() << std::endl;
            run_file(file);
            count++;
        }
    }
    std::cout << count << " inputs ran\n";
    return count == 0 ? 1 : 0;
}
