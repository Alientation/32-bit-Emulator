#pragma once

// Fixture for tests that run the preprocessor, assembler and linker in process on small sources.
// It does not need an emulator. Errors are checked with FatalAction::Throw, so a failing source
// throws aemu::log::FatalError instead of ending the test binary.

#include "assembler/assembler.h"
#include "assembler/build.h"
#include "assembler/linker.h"
#include "assembler/object_file.h"
#include "assembler/preprocessor.h"
#include "util/logger.h"

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

class ToolchainFixture : public ::testing::Test
{
  protected:
    /// Scratch directory of the test, removed if the test passes.
    fs::path m_dir;

    /// Options for the preprocessor: `<m_dir>/include` is the include directory.
    PreprocessorOptions m_options;

    aemu::log::ScopedLevel m_quiet{aemu::log::Level::Off};
    aemu::log::ScopedFatalAction m_throw{aemu::log::FatalAction::Throw};

    void SetUp() override
    {
        const ::testing::TestInfo *info = ::testing::UnitTest::GetInstance()->current_test_info();
        m_dir = fs::temp_directory_path() / "aemu_toolchain_unit"
                / (std::string(info->test_suite_name()) + "." + info->name());
        fs::remove_all(m_dir);
        fs::create_directories(m_dir / "include");
        fs::create_directories(m_dir / "out");

        m_options.system_dirs = {Directory((m_dir / "include").string())};
    }

    void TearDown() override
    {
        if (HasFailure())
        {
            std::cerr << "Test files kept in " << m_dir << "\n";
        }
        else
        {
            fs::remove_all(m_dir);
        }
    }

    /// Writes a file relative to the scratch directory and returns its path.
    std::string write(const std::string &relative, const std::string &text)
    {
        const fs::path path = m_dir / relative;
        fs::create_directories(path.parent_path());
        std::ofstream out(path, std::ios::binary);
        out << text;
        return path.string();
    }

    static std::string read(const std::string &path)
    {
        std::ifstream in(path, std::ios::binary);
        std::stringstream text;
        text << in.rdbuf();
        return text.str();
    }

    /// The message of the error that `action` ends with. Fails the test if there is none.
    static std::string error_of(const std::function<void()> &action)
    {
        try
        {
            action();
        }
        catch (const aemu::log::FatalError &error)
        {
            return error.what();
        }
        ADD_FAILURE() << "expected an error";
        return "";
    }

    static ::testing::AssertionResult contains(const std::string &text, const std::string &needle)
    {
        if (text.find(needle) != std::string::npos)
        {
            return ::testing::AssertionSuccess();
        }
        return ::testing::AssertionFailure() << "\"" << needle << "\" is not in:\n" << text;
    }
};
