#include "ccompiler/ccompiler.h"

#include "ccompiler/lexer.h"
#include "ccompiler/parser.h"
#include "ccompiler/codegen.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void ccompile (const char *filepath)
{
    int filepath_len = strlen (filepath);
    int last_dot = filepath_len - 1;
    while (last_dot >= 0 && filepath[last_dot] != '.')
    {
        last_dot--;
    }

    if (last_dot <= 0 || strcmp (filepath + last_dot + 1, "c") != 0)
    {
        fprintf (stderr, "ERROR: invalid filepath %s, expected *.c", filepath);
        exit (EXIT_FAILURE);
    }
    printf ("Compiling \'%s\'.\n\n", filepath);

    struct LexerData lexer;
    lexer_init (&lexer);
    lex (filepath, &lexer);
    lexer_print (&lexer);
    printf ("\n");

    struct ParserData parser;
    parser_init (&parser);
    parse (&lexer, &parser);
    parser_print (&parser);
    printf ("\n");

    char *output_filepath = calloc (last_dot + 1 + sizeof ("basm"), sizeof (char));
    if (!output_filepath)
    {
        fprintf (stderr, "ERROR: failed to allocate memory\n");
        exit (EXIT_FAILURE);
    }

    strncpy (output_filepath, filepath, last_dot);
    strcpy (output_filepath + last_dot, ".basm");

    printf ("Generating assembly code in \'%s\'.\n\n", output_filepath);

    codegen (&parser, output_filepath);
    free (output_filepath);
}