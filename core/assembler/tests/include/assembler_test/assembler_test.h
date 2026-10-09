#pragma once

#include "assembler/assembler.h"
#include "assembler/build.h"
#include "assembler/linker.h"
#include "assembler/load_executable.h"
#include "assembler/object_file.h"
#include "assembler/preprocessor.h"
#include "assembler/static_library.h"
#include "assembler/tokenizer.h"

#include <gtest/gtest.h>
#include <filesystem>
#include <string>

static constexpr U64 MAX_INSTRUCTIONS = 10000;

class EmulatorFixture : public ::testing::Test
{
  protected:
    Emulator32bit *machine = nullptr;

    /// The tests build without `-o`, which writes `a.bexe` into the working directory. Every test
    /// runs in a directory of its own so that tests running in parallel do not share that file.
    std::filesystem::path m_previous_dir;
    std::filesystem::path m_work_dir;

    void SetUp() override
    {
        const ::testing::TestInfo *info = ::testing::UnitTest::GetInstance()->current_test_info();
        m_previous_dir = std::filesystem::current_path();
        m_work_dir = std::filesystem::temp_directory_path() / "aemu_emulator_fixture"
                     / (std::string(info->test_suite_name()) + "." + info->name());
        std::filesystem::remove_all(m_work_dir);
        std::filesystem::create_directories(m_work_dir);
        std::filesystem::current_path(m_work_dir);

        // The ROM and the disk are in memory. A ROM or a disk backed by a file would be written
        // back to it, and in the source tree, by every test, all at once when they run in parallel.
        machine = new Emulator32bit(std::make_unique<RAM>(16, 0), std::make_unique<ROM>(16, 16),
                                    std::make_unique<MockDisk>());
        machine->mmu->begin_process();
    }

    void TearDown() override
    {
        delete machine;
        std::filesystem::current_path(m_previous_dir);
        std::filesystem::remove_all(m_work_dir);
    }
};
