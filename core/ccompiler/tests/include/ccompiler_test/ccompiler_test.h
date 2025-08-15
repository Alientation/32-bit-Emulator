#pragma once

#include <gtest/gtest.h>

extern "C" {
    #include "ccompiler/ccompiler.h"
    #include "ccompiler/codegen.h"
    #include "ccompiler/lexer.h"
    #include "ccompiler/massert.h"
    #include "ccompiler/parser.h"
    #include "ccompiler/stringbuffer.h"
}