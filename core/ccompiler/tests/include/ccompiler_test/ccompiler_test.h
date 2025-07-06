#pragma once
#ifndef CCOMPILER_TEST_H
#define CCOMPILER_TEST_H

#include <gtest/gtest.h>

extern "C" {
    #include "ccompiler/ccompiler.h"
    #include "ccompiler/codegen.h"
    #include "ccompiler/lexer.h"
    #include "ccompiler/massert.h"
    #include "ccompiler/parser.h"
    #include "ccompiler/stringbuffer.h"
}

#endif /* CCOMPILER_TEST_H */