#pragma once
#ifndef STATICLIBRARY_H
#define STATICLIBRARY_H
#include "assembler/ObjectFile.h"
#include "util/File.h"

#include <vector>

void WriteStaticLibrary(std::vector<File>& objs, File out);
void ReadStaticLibrary(std::vector<ObjectFile>& objs, File in);

#endif /* STATICLIBRARY_H */