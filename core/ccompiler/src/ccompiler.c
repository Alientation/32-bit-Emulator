#include "ccompiler/ccompiler.h"

#include "ccompiler/lexer.h"
#include "ccompiler/parser.h"
#include "ccompiler/codegen.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int ccompile (const char *filepath)
{
    int filepath_len = strlen (filepath);
    int last_dot = filepath_len - 1;
    while (last_dot >= 0 && filepath[last_dot] != '.')
    {
        last_dot--;
    }

    if (last_dot <= 0 || strcmp (filepath + last_dot + 1, "c") != 0)
    {
        fprintf (stderr, "Invalid filepath %s, expected *.c", filepath);
        return CCOMPILER_FAILURE__BAD_SOURCE_FILE;
    }
    printf ("Compiling \'%s\'.\n\n", filepath);

    struct LexerData lexer = lexer_init ();
    int lex_exitcode = lex (filepath, &lexer);
    if (lex_exitcode)
    {
        return lex_exitcode;
    }
    lexer_print (&lexer);
    printf ("\n");

    struct ParserData parser = parser_init ();
    int parse_exitcode = parse (&lexer, &parser);
    if (parse_exitcode)
    {
        return parse_exitcode;
    }
    parser_print (&parser);
    printf ("\n");


    char *output_filepath = calloc (last_dot + 1 + sizeof ("basm"), sizeof (char));
    strncpy (output_filepath, filepath, last_dot);
    strcpy (output_filepath + last_dot, ".basm");

    printf ("Generating assembly code in \'%s\'.\n\n", output_filepath);

    // todo

    free (output_filepath);
    return 0;
}