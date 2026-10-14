#ifndef CCOMPILER_PREPROCESS_H
#define CCOMPILER_PREPROCESS_H

#include "ccompiler/lex.h"

#include <stdio.h>

typedef struct
{
    /// The directories `#include` searches, in this order: for `#include "file"` after the
    /// directory of the file with the `#include`, for `#include <file>` only these.
    const char *const *include_paths;
    size_t include_path_count;
    /// Directives that are processed before the file (for `-D` and `-U`), e.g.
    /// "#define DEBUG 1\n#undef NDEBUG\n". Null for none.
    const char *command_line;
} PreprocessOptions;

/// Runs the preprocessor (phase 4 of translation, C17 6.10) on the tokens of a file: the
/// directives are carried out and removed, the macros are expanded, the included files are put
/// in. The result is still preprocessing tokens, see convert_pp_tokens(). Each call starts
/// without macros except the predefined ones (docs/ccompiler.md).
///
/// @param[in] tok The tokens of the file, see tokenize(). They are reused.
/// @param[in] options The include paths and the command line macros.
/// @return The first token. Exits with an error at a directive that is not valid, an `#error`,
/// a file that cannot be included, a macro call that is not valid and an `#if` that is not
/// closed.
Token *preprocess(Token *tok, const PreprocessOptions *options);

/// Prints preprocessing tokens as text (`bcc -E`): a token that starts a line starts one, a
/// token with space before it gets one, and two tokens that would be one if they were written
/// together get one as well. Comments are gone and the lines are not where they were.
///
/// @param[in] tok The first token.
/// @param[out] out Where the text goes.
void print_tokens(const Token *tok, FILE *out);

#endif
