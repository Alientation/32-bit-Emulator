#include "ccompiler/ccompiler.h"

#include "ccompiler/lexer.h"
#include "ccompiler/parser.h"
#include "ccompiler/codegen.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void ccompile (const char *filepath)
{
    size_t filepath_len = strlen (filepath);
    size_t last_dot = filepath_len - 1;
    while (last_dot + 1 > 0 && filepath[last_dot] != '.')
    {
        last_dot--;
    }

    if (last_dot <= 0 || strcmp (filepath + last_dot + 1, "c") != 0)
    {
        fprintf (stderr, "ERROR: invalid filepath %s, expected *.c\n", filepath);
        exit (EXIT_FAILURE);
    }
    printf ("Compiling \'%s\'.\n\n", filepath);

    lexer_data_t lexer = LEXER_INIT;
    if (!lex_file (filepath, &lexer))
    {
        lexer_free (&lexer);
        fprintf (stderr, "ERROR: failed to lex file %s\n", filepath);
        exit (EXIT_FAILURE);
    }
    lexer_print (&lexer);
    printf ("\n");

    printf("JUST TESTING LEXER\n");
    exit(EXIT_SUCCESS);

    parser_data_t parser;
    parser_init (&parser);
    parse (&lexer, &parser);

    if (parser.had_error)
    {
        parser_free (&parser);
        lexer_free (&lexer);
        fprintf (stderr, "ERROR: %s\n", parser.err_msg_buffer.buf);
        exit (EXIT_FAILURE);
    }

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
    parser_free (&parser);
    lexer_free (&lexer);
}