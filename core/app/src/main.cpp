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

/*
    TODO:
    * CLEAN UP CODE
    *    - Standardized braces placement (always on new line)
    *    - Split long functions into smaller unit sized ones, especially if they can be reused elsewhere
    *    - Macro preprocessors should follow the standard ALL_CAPS_WITH_UNDERSCORE naming conventions
            (DEFINITIONS SHOULD COME WITH THE PROJECT NAME PREPENDED BEFORE)
    *    - Comment functions and complex logic
    *    - Make variables that have larger scopes have more meaningful names
    *    - FIX TODO (emulator32bit library)
    *        - Disk.h + Disk.cpp
    *        - FreeBlockList.h + FreeBlockList.cpp
    *        - VirtualMemory.h + VirtualMemory.cpp
            - Memory.h + Memory.cpp
            - Emulator32bit.h + Emulator32bit.cpp + Emulator32bitUtil.h
            - Instructions.cpp + SoftwareInterrupt.cpp

    * improve vscode extension for basm language like autocomplete, syntax highlighting, etc

    Support multi-core emulator. This would require sharing and synchronizing
    memory, disk, and etc

    Figure shared object files/dynamically linked libraries

    Enforce a zeroing of the first couple pages of memory to catch null pointers <- likely handled by the kernel
    Work out how the stack and heap will work (likely operates in virtual memory as opposed to physical memory)
    Work out how context switching will work (likely the only state that needs to be saved will be registers and process id)

    * Create tests with the assembler to run longer pieces of code

    Benchmark system for each component of the project (tokenizer, preprocessor, assembler, linker, emulator)
    Idea for optimization, add macros to specify whether we want a large amount of invalid state checking,
    idea is to have the ability to reduce branch mispredictions
    instead of passing exceptions into functions, have the class that contains the functions maintain its
    exception state.

    // Figure out IO, Disk, ports, etc
        - We will use memory mapped IO that can be accessed through syscalls to the kernel
        - Disk is implemented as a file stored on the host computer that represents disk memory
        - Ports will be memory mapped that will be accessed through syscalls to the kernel

    Change virtual memory to not automatically map more virtual memory pages to physical pages when they are accessed. This should instead through a segfault
    exception in the emulator.
    We instead will have to manually request the virtual memory to map a specfic set of virtual memory pages to a *type* of memory (by suppling the memory object?)
    or giving a range of physical page addresses. We also already have a way to map a specific physical page.
    Next, allow executable loader to write to a specific physical address so we can write the kernel/os to disk for the BIOS to load

    Add flag to set source dirs for build
    Add more relocation types, directives, preprocessors and build flags as needed
    Rework the build process to be more like the tokenizer
    Fix much of the hardcodedness of the assembler/object file/linker (the sections that are fixed fields of ObjectFile)
    Implement linker scripts that the linker will process to create the final BELF file
        - https://users.informatik.haw-hamburg.de/~krabat/FH-Labor/gnupro/5_GNUPro_Utilities/c_Using_LD/ldLinker_scripts.html
    Figure out executable linking/loading

    Rework how byte reader works, don't like how we have to manually extra all bytes from file and then
    pass it to the the byte reader. Maybe change that to ByteParser and add a ByteReader that uses it
    and the file reader to parse the bytes.

    Visualizer with raylib to visually show the state of the processor

    Bootloader that loads the kernel

    Add syscalls for virtual memory management, but for now, they will be controlled in c++ land
    Maybe have a toggle for virtual memory instead??, like using the mocks.
*/

const static std::string build_test =
    "-I ./tests/include -o ./tests/build/test ./tests/src/main.basm ./tests/src/other.basm -outdir "
    "./tests/build";
const static std::string build_fibonacci = "-I ./programs/include -o ./programs/build/fibonacci "
                                           "./programs/src/fibonacci.basm -outdir ./programs/build";
const static std::string build_preprocessor =
    "-kp -I ./programs/include -o ./programs/build/showcase_preprocessor "
    "./programs/src/showcase_preprocessor.basm -outdir ./programs/build";
const static std::string build_palindrome =
    "-I ./programs/include -o ./programs/build/palindrome ./programs/src/palindrome.basm -outdir "
    "./programs/build";
const static std::string build_library =
    "-o ./programs/build/libtest -ar ./programs/src/palindrome.basm -outdir ./programs/build";
const static std::string build_exe_from_library =
    "-l ./programs/build/libtest.ba -o ./programs/build/palindrome_list -outdir ./programs/build";
const static std::string build_exe_from_library_dir =
    "-libdir ./programs/build -o ./programs/build/palindrome_list -outdir ./programs/build";
const static std::string build_long_loop =
    "-o ./programs/build/long_loop ./programs/src/long_loop.basm -outdir ./programs/build";

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