#ifndef CCOMPILER_PARSE_H
#define CCOMPILER_PARSE_H

#include "ccompiler/lex.h"
#include "ccompiler/type.h"

typedef enum
{
    /// An integer constant, `value`.
    ND_NUM,
    /// Unary operators, the operand is `lhs`.
    ND_NEG,
    ND_BITNOT,
    ND_NOT,
    /// Binary operators, the operands are `lhs` and `rhs`.
    ND_ADD,
    ND_SUB,
    ND_MUL,
    ND_DIV,
    ND_MOD,
    ND_SHL,
    ND_SHR,
    ND_LT,
    ND_LE,
    ND_GT,
    ND_GE,
    ND_EQ,
    ND_NE,
    ND_BITAND,
    ND_BITXOR,
    ND_BITOR,
    ND_LOGAND,
    ND_LOGOR,
    /// `return lhs;`, or `return;` when `lhs` is null.
    ND_RETURN,
    /// `{ ... }`: the statements are the list that starts at `body`.
    ND_BLOCK,
} NodeKind;

typedef struct Node Node;

struct Node
{
    NodeKind kind;
    /// The next statement of the block this node is in.
    Node *next;
    Node *lhs;
    Node *rhs;
    Node *body;
    int value;
    /// The type of an expression, null for a statement.
    Type *ty;
    /// The token the node starts at, for error messages.
    const Token *tok;
};

typedef struct Function Function;

struct Function
{
    /// The next function of the file.
    Function *next;
    const char *name;
    /// The token of the name, for error messages.
    const Token *tok;
    Type *return_type;
    /// An `ND_BLOCK`.
    Node *body;
};

/// Parses a file: a list of function definitions that return `int` or `void` and have no
/// parameters, with `return` statements and blocks.
///
/// @param[in] tok The first token of the file, see tokenize().
/// @return The functions in the order of the file. Exits with an error at the first thing that is
/// not valid.
Function *parse(Token *tok);

#endif
