#pragma once
#ifndef STATIC_LIBRARY_H
#define STATIC_LIBRARY_H

#include "assembler/object_file.h"
#include "util/file.h"

#include <vector>

void WriteStaticLibrary(std::vector<File>& objs, File out);
void ReadStaticLibrary(std::vector<ObjectFile>& objs, File in);

#endif /* STATIC_LIBRARY_H */