// Fuzz target of the loader: the input is a .bexe, which is read, loaded and run for a few
// thousand instructions. An input that is not a valid executable has to end in a FatalError; a
// crash, a sanitizer report, a failed assertion, any other exception or a hang is a bug.

#include "assembler/load_executable.h"
#include "util/logger.h"

#include <cstdint>

namespace
{

/// Enough for a program to reach its loops and a few exceptions, little enough for many runs a
/// second.
constexpr U64 kInstructions = 10000;

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    aemu::log::set_level(aemu::log::Level::Off);
    aemu::log::set_fatal_action(aemu::log::FatalAction::Throw);

    // The machine of the tests (assembler_test.h): 16 pages of RAM, then 16 of ROM.
    Emulator32bit emu(std::make_unique<RAM>(16, 0), std::make_unique<ROM>(16, 16),
                      std::make_unique<MockDisk>());
    emu.mmu->begin_process();
    try
    {
        LoadExecutable(emu, std::vector<byte>(data, data + size)).load();
    }
    catch (const aemu::log::FatalError &)
    {
        return 0;
    }
    emu.run(kInstructions);
    return 0;
}
