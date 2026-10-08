#pragma once

#include "util/directory.h"

#include <map>
#include <string>
#include <vector>

/// File extensions of the toolchain's file types (without the dot).
inline const std::string SOURCE_EXTENSION = "basm";
inline const std::string INCLUDE_EXTENSION = "binc";
inline const std::string PROCESSED_EXTENSION = "bi";
inline const std::string OBJECT_EXTENSION = "bo";
inline const std::string EXECUTABLE_EXTENSION = "bexe";
inline const std::string STATIC_LIBRARY_EXTENSION = "ba";

/// What the preprocessor needs to know about the build. The command line driver (Process) fills
/// it in, tests and other embedders can build one directly.
struct PreprocessorOptions
{
    /// Directories searched by `#include <"file">`.
    std::vector<Directory> system_dirs;

    /// Symbols that are defined before the file is read, as if it started with
    /// `#define name value`. These are the `-D name[=value]` flags.
    std::map<std::string, std::string> defines;

    /// Whether the preprocessed text is written to the .bi file. The assembler takes the tokens
    /// from the preprocessor itself, so the file is only there to look at (-kp).
    bool write_output = true;
};
