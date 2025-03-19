#include "ccompiler/ccompiler.h"

#include "ccompiler/lexer.h"
#include "ccompiler/parser.h"
#include "ccompiler/codegen.h"

char* ccompile (const char *filepath)
{
    struct LexerData lexer = lexer_init ();
    lex (filepath, &lexer);
    lexer_print (&lexer);

    return "";
}