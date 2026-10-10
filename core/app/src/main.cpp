#include "assembler/assembler.h"
#include "assembler/build.h"
#include "assembler/linker.h"
#include "assembler/load_executable.h"
#include "assembler/object_file.h"
#include "assembler/preprocessor.h"
#include "emulator32bit/disk.h"
#include "emulator32bit/emulator32bit.h"
#include "util/file.h"
#include "util/logger.h"

#include <memory>
#include <string>
#include <vector>

// The sample that is built and run when there are no arguments.
static const std::string build_palindrome =
    "-I ./programs/include -o ./programs/build/palindrome ./programs/src/palindrome.basm -outdir "
    "./programs/build";

constexpr U64 kMaxCycles = 0x0;

static int run_app(int argc, char *argv[])
{
    // The arguments of the program, or a sample build if there are none.
    std::vector<std::string> args = Build::split_args(build_palindrome);
    if (argc > 1)
    {
        AEMU_INFO("Parsing command arguments");
        args.assign(argv + 1, argv + argc);
    }

    std::unique_ptr<Build> build;
    {
        AEMU_SCOPED_TIMER("Building program");
        build = std::make_unique<Build>(args);
        build->run();
    }

    if (build->has_work() && build->does_create_exe())
    {
        std::unique_ptr<Emulator32bit> emulator;
        long long pid;
        {
            AEMU_SCOPED_TIMER("Loading program into emulator");
            // The ROM and the disk are in memory, a sample does not need files of its own.
            emulator = std::make_unique<Emulator32bit>(std::make_unique<RAM>(16, 0),
                                                       std::make_unique<ROM>(16, 16),
                                                       std::make_unique<MockDisk>());
            pid = emulator->mmu->begin_process();
            LoadExecutable loader(*emulator, build->get_exe_file());
            loader.load();
        }

        {
            AEMU_INFO("Running emulator");
            AEMU_SCOPED_TIMER("Running emulator");
            emulator->run(kMaxCycles);
            emulator->print();
            emulator->mmu->end_process(pid);
        }
    }
    return EXIT_SUCCESS;
}

int main(int argc, char *argv[])
{
    // Errors in the toolchain and emulator libraries are thrown. They are already logged by the
    // time they get here.
    aemu::log::set_fatal_action(aemu::log::FatalAction::Throw);
    try
    {
        return run_app(argc, argv);
    }
    catch (const aemu::log::FatalError &)
    {
        return EXIT_FAILURE;
    }
    catch (const std::exception &error)
    {
        AEMU_ERROR("{}", error.what());
        return EXIT_FAILURE;
    }
}