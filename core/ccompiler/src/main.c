#include "ccompiler/codegen.h"
#include "ccompiler/lex.h"
#include "ccompiler/parse.h"
#include "ccompiler/preprocess.h"
#include "ccompiler/util.h"

#include <stdio.h>
#include <string.h>

static void usage(void)
{
    fprintf(stderr, "usage: bcc [-E] [-o output] [-I dir] [-D name[=value]] [-U name] input.c\n");
}

/// @return The value of an option that is either glued to it (`-Idir`) or the next argument
/// (`-I dir`), null if there is none.
static const char *option_value(int argc, char **argv, int *i)
{
    if (argv[*i][2])
    {
        return argv[*i] + 2;
    }
    if (*i + 1 < argc)
    {
        return argv[++*i];
    }
    return NULL;
}

/// Appends a directive for `-D name[=value]` (the value is 1 if there is none) or `-U name` to
/// the command line text.
static char *append_directive(char *text, bool define, const char *arg)
{
    const size_t old_len = text ? strlen(text) : 0;
    char *line = arena_alloc(old_len + strlen(arg) + 16);
    if (text)
    {
        memcpy(line, text, old_len);
    }
    char *p = line + old_len;
    const char *equals = strchr(arg, '=');
    if (!define)
    {
        memcpy(p, "#undef ", 7);
        memcpy(p + 7, arg, strlen(arg));
        p += 7 + strlen(arg);
    }
    else if (equals)
    {
        memcpy(p, "#define ", 8);
        memcpy(p + 8, arg, (size_t) (equals - arg));
        p += 8 + (equals - arg);
        *p++ = ' ';
        memcpy(p, equals + 1, strlen(equals + 1));
        p += strlen(equals + 1);
    }
    else
    {
        memcpy(p, "#define ", 8);
        memcpy(p + 8, arg, strlen(arg));
        p += 8 + strlen(arg);
        memcpy(p, " 1", 2);
        p += 2;
    }
    *p = '\n';
    return line;
}

/// The parser takes only `int` constants so far, and would read the value of an `unsigned`, a
/// `long long` or a floating constant as an `int`: those are an error until it knows them.
static void check_constants(const Token *tok)
{
    for (; tok->kind != TK_EOF; tok = tok->next)
    {
        if (tok->kind == TK_NUM && tok->ty != ty_int)
        {
            error_tok(tok, "constants of type '%s' are not supported yet", tok->ty->name);
        }
    }
}

int main(int argc, char **argv)
{
    const char *input = NULL;
    const char *output = NULL;
    bool preprocess_only = false;
    const char **include_paths = arena_alloc((size_t) argc * sizeof(char *));
    size_t include_path_count = 0;
    char *command_line = NULL;
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
        {
            output = argv[++i];
        }
        else if (strcmp(argv[i], "-E") == 0)
        {
            preprocess_only = true;
        }
        else if (argv[i][0] == '-' && (argv[i][1] == 'I' || argv[i][1] == 'D' || argv[i][1] == 'U'))
        {
            const char option = argv[i][1];
            const char *value = option_value(argc, argv, &i);
            if (!value)
            {
                usage();
                return 1;
            }
            if (option == 'I')
            {
                include_paths[include_path_count++] = value;
            }
            else
            {
                command_line = append_directive(command_line, option == 'D', value);
            }
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
    if (!file)
    {
        error("cannot open '%s'", input);
    }
    PreprocessOptions options = {include_paths, include_path_count, command_line};
    Token *tok = preprocess(tokenize(file), &options);

    Function *prog = NULL;
    if (!preprocess_only)
    {
        tok = convert_pp_tokens(tok);
        check_constants(tok);
        prog = parse(tok);
    }

    FILE *out = output ? fopen(output, "w") : stdout;
    if (!out)
    {
        error("cannot open '%s' for writing", output);
    }
    if (preprocess_only)
    {
        print_tokens(tok, out);
    }
    else
    {
        codegen(prog, out);
    }
    if (fflush(out) != 0 || ferror(out) || (out != stdout && fclose(out) != 0))
    {
        error("cannot write the output");
    }
    return 0;
}
