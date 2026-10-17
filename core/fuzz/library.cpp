// Fuzz target of the static library reader and of the choice of its members: the input is the .bo
// of a program and a .ba (two bytes, little endian, with the size of the .bo, then the .bo, then
// the .ba), which are read, the members that the program needs chosen, linked, loaded and run for
// a few thousand instructions, as `basm -l` does. What it may do with a file that is not valid is
// end in a FatalError; a crash, a sanitizer report, a failed assertion, any other exception or a
// hang is a bug.
//
// The files are in a directory of the process (fuzz-<pid> in the temporary directory). The seeds
// are made by fuzz/seeds/library_seeds.sh.

#include "assembler/linker.h"
#include "assembler/load_executable.h"
#include "assembler/object_file.h"
#include "assembler/static_library.h"
#include "util/logger.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <ostream>
#include <string>
#include <unistd.h>
#include <vector>

namespace
{

/// Enough to run the start of a program, little enough for many runs a second.
constexpr U64 kInstructions = 2000;

/// The bytes that hold the size of the object file.
constexpr size_t kSizeBytes = 2;

/// @return A directory of this process, made now.
std::filesystem::path make_work_dir()
{
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / ("aemu-library-fuzz-" + std::to_string(getpid()));
    std::filesystem::create_directories(dir);
    return dir;
}

const std::filesystem::path &work_dir()
{
    static const std::filesystem::path dir = make_work_dir();
    return dir;
}

std::vector<byte> read_bytes(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    return std::vector<byte>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    aemu::log::set_level(aemu::log::Level::Off);
    aemu::log::set_fatal_action(aemu::log::FatalAction::Throw);
    if (size < kSizeBytes) return 0;

    const size_t object_size = size_t(data[0]) | (size_t(data[1]) << 8);
    const uint8_t *const object = data + kSizeBytes;
    const size_t rest = size - kSizeBytes;
    const size_t object_end = object_size < rest ? object_size : rest;

    const std::filesystem::path library = work_dir() / "in.ba";
    {
        std::ofstream out(library, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char *>(object + object_end),
                  std::streamsize(rest - object_end));
    }

    const std::filesystem::path executable = work_dir() / "out.bexe";
    Emulator32bit emu(std::make_unique<RAM>(16, 0), std::make_unique<ROM>(16, 16),
                      std::make_unique<MockDisk>());
    try
    {
        std::vector<ObjectFile> objects(1);
        objects[0].read_object_file(std::vector<byte>(object, object + object_end));
        const std::vector<LibraryMember> members = read_static_library(File(library.string()));
        Linker(select_library_members(objects, members), File(executable.string())).link();
        emu.mmu->begin_process();
        LoadExecutable(emu, read_bytes(executable)).load();
    }
    catch (const aemu::log::FatalError &)
    {
        return 0;
    }

    std::ostream discard(nullptr);
    emu.set_output(discard);
    emu.set_error_output(discard);
    emu.system_bus->console.set_output(nullptr);
    emu.run(kInstructions);
    return 0;
}
