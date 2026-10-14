#include "ccompiler/codegen.h"
#include "ccompiler/lex.h"
#include "ccompiler/parse.h"
#include "ccompiler/util.h"

#include <stdio.h>
#include <string.h>

static void usage(void)
{
    fprintf(stderr, "usage: bcc [-o output.basm] input.c\n");
}

int main(int argc, char **argv)
{
    const char *input = NULL;
    const char *output = NULL;
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
        {
            output = argv[++i];
        }
        else if (argv[i][0] != '-' && !input)
        {
            input = argv[i];
        }
        else
        {
            usage();
            return 1;
        }
    }
    if (!input)
    {
        usage();
        return 1;
    }

    SourceFile *file = read_source_file(input);
    Token *tok = tokenize(file);
    Function *prog = parse(tok);

    FILE *out = output ? fopen(output, "w") : stdout;
    if (!out)
    {
        error("cannot open '%s' for writing", output);
    }
    codegen(prog, out);
    if (fflush(out) != 0 || ferror(out) || (out != stdout && fclose(out) != 0))
    {
        error("cannot write the output");
    }
    return 0;
}
