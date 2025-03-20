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

    struct LexerData lexer;
    int exitcode = lexer_init (&lexer);
    if (exitcode)
    {
        lexer_fail:
        lexer_free (&lexer);
        return exitcode;
    }
    exitcode = lex (filepath, &lexer);
    if (exitcode) goto lexer_fail;
    lexer_print (&lexer);
    printf ("\n");

    struct ParserData parser;
    exitcode = parser_init (&parser);
    if (exitcode)
    {
        parser_fail:
        parser_free (&parser);
        goto lexer_fail;
    }

    exitcode = parse (&lexer, &parser);
    if (exitcode)
    {
        goto parser_fail;
    }
    parser_print (&parser);
    printf ("\n");


    char *output_filepath = calloc (last_dot + 1 + sizeof ("basm"), sizeof (char));
    if (!output_filepath)
    {
        perror ("Memory allocation failure");
        exitcode = 1;
        goto parser_fail;
    }

    strncpy (output_filepath, filepath, last_dot);
    strcpy (output_filepath + last_dot, ".basm");

    printf ("Generating assembly code in \'%s\'.\n\n", output_filepath);

    exitcode = codegen (&parser, output_filepath);
    free (output_filepath);
    output_filepath = NULL;

    if (exitcode) goto parser_fail;
    return 0;
}