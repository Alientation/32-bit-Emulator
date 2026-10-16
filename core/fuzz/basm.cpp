// Fuzz target of the toolchain: the input is basm source, which is preprocessed, assembled, written
// as a .bo, read back and written again (the two files have to be the same), printed as a listing,
// linked, loaded and run for a few thousand instructions. What it may do with a source that is not
// valid is end in a FatalError; a crash, a sanitizer report, a failed assertion, any other
// exception, a hang or an object file that does not read back to itself is a bug.
//
// The files are in a directory of the process (fuzz-<pid> in the temporary directory), so the
// workers of a run do not share them. fuzz/basm.dict has the keywords of the language.

#include "assembler/assembler.h"
#include "assembler/linker.h"
#include "assembler/load_executable.h"
#include "assembler/object_file.h"
#include "assembler/preprocessor.h"
#include "util/logger.h"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <unistd.h>
#include <vector>

namespace
{

/// Enough to run the start of a program, little enough for many runs a second.
constexpr U64 kInstructions = 2000;

/// @return A directory of this process, made now.
std::filesystem::path make_work_dir()
{
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / ("aemu-basm-fuzz-" + std::to_string(getpid()));
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

/// The object file as the assembler wrote it reads back to one that writes the same bytes.
void require_round_trip(const std::filesystem::path &written, ObjectFile &read_back)
{
    const std::vector<byte> bytes = read_bytes(written);
    read_back.read_object_file(bytes);

    const std::filesystem::path again = work_dir() / "again.bo";
    read_back.write_object_file(File(again.string()));
    if (read_bytes(again) != bytes)
    {
        std::cerr << "basm: the object file that was read back writes other bytes\n";
        std::abort();
    }
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    aemu::log::set_level(aemu::log::Level::Off);
    aemu::log::set_fatal_action(aemu::log::FatalAction::Throw);

    const std::filesystem::path source = work_dir() / "in.basm";
    const std::filesystem::path processed = work_dir() / "in.bi";
    const std::filesystem::path object = work_dir() / "in.bo";
    const std::filesystem::path executable = work_dir() / "in.bexe";
    {
        std::ofstream out(source, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char *>(data), std::streamsize(size));
    }

    try
    {
        PreprocessorOptions options;
        options.write_output = false;
        Preprocessor preprocessor(File(source.string()), processed.string(), options);
        preprocessor.preprocess();
        Assembler assembler(File(processed.string()), preprocessor.take_result(), object.string());
        assembler.assemble();
    }
    catch (const aemu::log::FatalError &)
    {
        return 0;
    }

    ObjectFile obj;
    require_round_trip(object, obj);
    std::ostream discard(nullptr);
    obj.print(discard);

    Emulator32bit emu(std::make_unique<RAM>(16, 0), std::make_unique<ROM>(16, 16),
                      std::make_unique<MockDisk>());
    try
    {
        Linker({obj}, File(executable.string())).link();
        emu.mmu->begin_process();
        LoadExecutable(emu, read_bytes(executable)).load();
    }
    catch (const aemu::log::FatalError &)
    {
        return 0;
    }
    emu.set_output(discard);
    emu.set_error_output(discard);
    emu.system_bus->console.set_output(nullptr);
    emu.run(kInstructions);
    return 0;
}
