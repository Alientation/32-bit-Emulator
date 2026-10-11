#pragma once

#include "assembler/object_file.h"
#include "util/file.h"

#include <string>
#include <vector>

/// An object file in a static library (.ba), and the name that it has in it, which is the name of
/// the file it was made from.
struct LibraryMember
{
    std::string name;
    ObjectFile object;
};

/// Packs the object files into a static library, one member for each. The library is written
/// after all the object files were read, so it can be one of them.
///
/// @param[in] objects The object files (.bo). Each has to be valid.
/// @param[in] out The library to write (.ba).
void write_static_library(const std::vector<File> &objects, const File &out);

/// Reads the members of a static library. Fatal if the file is not a static library, or is cut
/// short.
///
/// @param[in] in The library (.ba).
/// @return The members, in the order they were written.
std::vector<LibraryMember> read_static_library(const File &in);

/// The object files to link when a program is made of `objects` and of the members of libraries.
/// These are `objects`, and then every member that defines a symbol that is used and not defined
/// so far, repeated until nothing is added, as members bring symbols of their own that they use.
/// A member that nothing needs is not linked, like the objects of a library of other toolchains.
///
/// @param[in] objects The object files of the program.
/// @param[in] members The members of the libraries that are linked.
/// @return The object files of the program and the members that are needed.
std::vector<ObjectFile> select_library_members(const std::vector<ObjectFile> &objects,
                                               const std::vector<LibraryMember> &members);
