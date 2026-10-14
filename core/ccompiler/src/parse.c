#include "ccompiler/parse.h"

#include "ccompiler/util.h"

#include <string.h>

typedef struct
{
    const char *text;
    NodeKind kind;
} BinaryOperator;

// The binary operators, a table for each precedence level, the loosest first. Each table ends
// with an entry without text.
static const BinaryOperator logor_ops[] = {{"||", ND_LOGOR}, {NULL, ND_NUM}};
static const BinaryOperator logand_ops[] = {{"&&", ND_LOGAND}, {NULL, ND_NUM}};
static const BinaryOperator bitor_ops[] = {{"|", ND_BITOR}, {NULL, ND_NUM}};
static const BinaryOperator bitxor_ops[] = {{"^", ND_BITXOR}, {NULL, ND_NUM}};
static const BinaryOperator bitand_ops[] = {{"&", ND_BITAND}, {NULL, ND_NUM}};
static const BinaryOperator equality_ops[] = {{"==", ND_EQ}, {"!=", ND_NE}, {NULL, ND_NUM}};
static const BinaryOperator relational_ops[] = {
    {"<", ND_LT}, {"<=", ND_LE}, {">", ND_GT}, {">=", ND_GE}, {NULL, ND_NUM}};
static const BinaryOperator shift_ops[] = {{"<<", ND_SHL}, {">>", ND_SHR}, {NULL, ND_NUM}};
static const BinaryOperator additive_ops[] = {{"+", ND_ADD}, {"-", ND_SUB}, {NULL, ND_NUM}};
static const BinaryOperator multiplicative_ops[] = {
    {"*", ND_MUL}, {"/", ND_DIV}, {"%", ND_MOD}, {NULL, ND_NUM}};

static const BinaryOperator *const binary_levels[] = {
    logor_ops,    logand_ops,     bitor_ops, bitxor_ops,   bitand_ops,
    equality_ops, relational_ops, shift_ops, additive_ops, multiplicative_ops,
};

enum
{
    NUM_BINARY_LEVELS = sizeof(binary_levels) / sizeof(binary_levels[0]),
};

/// The return type of the function that is being parsed, for its `return` statements.
static Type *current_return_type;

static Node *new_node(NodeKind kind, const Token *tok)
{
    Node *node = arena_alloc(sizeof(Node));
    node->kind = kind;
    node->tok = tok;
    return node;
}

static Node *new_num(int value, const Token *tok)
{
    Node *node = new_node(ND_NUM, tok);
    node->value = value;
    node->ty = ty_int;
    return node;
}

static Node *new_unary(NodeKind kind, Node *operand, const Token *tok)
{
    Node *node = new_node(kind, tok);
    node->lhs = operand;
    node->ty = ty_int;
    return node;
}

static Node *new_binary(NodeKind kind, Node *lhs, Node *rhs, const Token *tok)
{
    Node *node = new_node(kind, tok);
    node->lhs = lhs;
    node->rhs = rhs;
    node->ty = ty_int;
    return node;
}

/// Skips a token that has to be there.
static Token *skip(Token *tok, const char *text)
{
    if (!token_is(tok, text))
    {
        error_tok(tok, "expected '%s'", text);
    }
    return tok->next;
}

static Node *expr(Token **rest, Token *tok);

/// primary = "(" expr ")" | num
static Node *primary(Token **rest, Token *tok)
{
    if (token_is(tok, "("))
    {
        Node *node = expr(&tok, tok->next);
        *rest = skip(tok, ")");
        return node;
    }
    if (tok->kind == TK_NUM)
    {
        Node *node = new_num(tok->value, tok);
        *rest = tok->next;
        return node;
    }
    error_tok(tok, "expected an expression");
    return NULL;
}

/// unary = ("+" | "-" | "~" | "!") unary | primary
static Node *unary(Token **rest, Token *tok)
{
    if (token_is(tok, "+"))
    {
        return unary(rest, tok->next);
    }
    if (token_is(tok, "-"))
    {
        return new_unary(ND_NEG, unary(rest, tok->next), tok);
    }
    if (token_is(tok, "~"))
    {
        return new_unary(ND_BITNOT, unary(rest, tok->next), tok);
    }
    if (token_is(tok, "!"))
    {
        return new_unary(ND_NOT, unary(rest, tok->next), tok);
    }
    return primary(rest, tok);
}

/// A binary operator of the table, or null if the token is none of them.
static const BinaryOperator *find_operator(const BinaryOperator *ops, const Token *tok)
{
    for (; ops->text; ops++)
    {
        if (token_is(tok, ops->text))
        {
            return ops;
        }
    }
    return NULL;
}

/// The operators of one precedence level, which associate to the left, over the next tighter
/// level.
static Node *binary(Token **rest, Token *tok, size_t level)
{
    if (level == NUM_BINARY_LEVELS)
    {
        return unary(rest, tok);
    }
    Node *node = binary(&tok, tok, level + 1);
    const BinaryOperator *op;
    while ((op = find_operator(binary_levels[level], tok)))
    {
        const Token *start = tok;
        Node *rhs = binary(&tok, tok->next, level + 1);
        node = new_binary(op->kind, node, rhs, start);
    }
    *rest = tok;
    return node;
}

/// expr = binary(0), see docs/ccompiler.md#grammar
static Node *expr(Token **rest, Token *tok)
{
    return binary(rest, tok, 0);
}

/// stmt = "return" expr? ";" | "{" stmt* "}" | ";"
static Node *stmt(Token **rest, Token *tok)
{
    if (token_is(tok, "return"))
    {
        Node *node = new_node(ND_RETURN, tok);
        tok = tok->next;
        if (token_is(tok, ";"))
        {
            if (current_return_type != ty_void)
            {
                error_tok(tok, "a function that returns int has to return a value");
            }
        }
        else
        {
            if (current_return_type == ty_void)
            {
                error_tok(tok, "a function that returns void cannot return a value");
            }
            node->lhs = expr(&tok, tok);
        }
        *rest = skip(tok, ";");
        return node;
    }
    if (token_is(tok, "{"))
    {
        Node *node = new_node(ND_BLOCK, tok);
        Node head = {0};
        Node *cur = &head;
        tok = tok->next;
        while (!token_is(tok, "}"))
        {
            if (tok->kind == TK_EOF)
            {
                error_tok(tok, "expected '}'");
            }
            cur->next = stmt(&tok, tok);
            cur = cur->next;
        }
        node->body = head.next;
        *rest = tok->next;
        return node;
    }
    if (token_is(tok, ";"))
    {
        *rest = tok->next;
        return new_node(ND_BLOCK, tok);
    }
    error_tok(tok, "expected a statement");
    return NULL;
}

/// function = ("int" | "void") ident "(" "void"? ")" "{" stmt* "}"
static Function *function(Token **rest, Token *tok)
{
    Type *return_type;
    if (token_is(tok, "int"))
    {
        return_type = ty_int;
    }
    else if (token_is(tok, "void"))
    {
        return_type = ty_void;
    }
    else
    {
        error_tok(tok, "expected 'int' or 'void'");
        return NULL;
    }
    tok = tok->next;

    if (tok->kind != TK_IDENT)
    {
        error_tok(tok, "expected a function name");
    }
    Function *fn = arena_alloc(sizeof(Function));
    fn->return_type = return_type;
    char *name = arena_alloc(tok->len + 1);
    memcpy(name, tok->loc, tok->len);
    fn->name = name;
    fn->tok = tok;
    tok = tok->next;

    tok = skip(tok, "(");
    if (token_is(tok, "void"))
    {
        tok = tok->next;
    }
    tok = skip(tok, ")");

    if (!token_is(tok, "{"))
    {
        error_tok(tok, "expected '{'");
    }
    current_return_type = return_type;
    fn->body = stmt(rest, tok);
    current_return_type = NULL;
    return fn;
}

Function *parse(Token *tok)
{
    Function head = {0};
    Function *cur = &head;
    while (tok->kind != TK_EOF)
    {
        cur->next = function(&tok, tok);
        cur = cur->next;
        for (const Function *other = head.next; other != cur; other = other->next)
        {
            if (strcmp(other->name, cur->name) == 0)
            {
                error_tok(cur->tok, "'%s' is defined twice", cur->name);
            }
        }
    }
    return head.next;
}
