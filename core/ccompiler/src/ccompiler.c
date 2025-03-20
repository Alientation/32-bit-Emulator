#include "ccompiler/ccompiler.h"

#include "ccompiler/lexer.h"
#include "ccompiler/parser.h"
#include "ccompiler/codegen.h"

char* ccompile (const char *filepath)
{
    struct LexerData lexer = lexer_init ();
    lex (filepath, &lexer);
    lexer_print (&lexer);

    struct ParserData parser = parser_init ();
    parse (&lexer, &parser);
    parser_print (&parser);
    return "";
}