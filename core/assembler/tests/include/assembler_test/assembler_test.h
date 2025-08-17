#pragma once

#include <assembler/assembler.h>
#include <assembler/build.h>
#include <assembler/linker.h>
#include <assembler/load_executable.h>
#include <assembler/object_file.h>
#include <assembler/preprocessor.h>
#include <assembler/static_library.h>
#include <assembler/tokenizer.h>
#include <gtest/gtest.h>
#include <util/logger.h>

static constexpr U64 MAX_INSTRUCTIONS = 10000;

class EmulatorFixture : public ::testing::Test
{
  protected:
    Emulator32bit *machine = nullptr;

    void SetUp () override
    {
        static ROM *rom =
            new ROM (File (AEMU_PROJECT_ROOT_DIR + "core/assembler/tests/rom.bin", true), 16, 16);
        static Disk *disk =
            new Disk (File (AEMU_PROJECT_ROOT_DIR + "core/assembler/tests/disk.bin"), 32, 32);

        machine = new Emulator32bit (new RAM (16, 0), new ROM (*rom), new Disk (*disk));
        long long pid = machine->system_bus->mmu->begin_process ();
    }

    void TearDown () override
    {
        delete machine;
    }
};
