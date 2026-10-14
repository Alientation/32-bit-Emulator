#include "ccompiler/codegen.h"

#include "ccompiler/util.h"

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>

static FILE *output;

/// The number of the next label.
static int label_count;

/// The label a `return` of the function that is being generated jumps to.
static int return_label;

/// The number of values that are pushed on the stack and not popped yet, in words.
static int depth;

static void emit(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(output, fmt, ap);
    va_end(ap);
    fprintf(output, "\n");
}

static int new_label(void)
{
    return label_count++;
}

static void emit_label(int label)
{
    emit("L%d:", label);
}

static void push(void)
{
    emit("    str     x0, [sp, -4]!");
    depth++;
}

static void pop(const char *reg)
{
    emit("    ldr     %s, [sp], 4", reg);
    depth--;
}

static void gen_expr(const Node *node);

/// Evaluates `lhs && rhs` or `lhs || rhs` into x0 as 0 or 1. The right operand is skipped when
/// the left one decides the result.
static void gen_logical(const Node *node)
{
    const bool is_and = node->kind == ND_LOGAND;
    const int decided = new_label();
    const int end = new_label();

    gen_expr(node->lhs);
    emit("    cmp     x0, 0");
    emit("    b.%s    L%d", is_and ? "eq" : "ne", decided);
    gen_expr(node->rhs);
    emit("    cmp     x0, 0");
    emit("    b.%s    L%d", is_and ? "eq" : "ne", decided);
    emit("    mov     x0, %d", is_and ? 1 : 0);
    emit("    b       L%d", end);
    emit_label(decided);
    emit("    mov     x0, %d", is_and ? 0 : 1);
    emit_label(end);
}

/// Evaluates an expression into x0.
static void gen_expr(const Node *node)
{
    switch (node->kind)
    {
    case ND_NUM:
        emit("    ldr     x0, =%d", node->value);
        return;
    case ND_NEG:
        gen_expr(node->lhs);
        emit("    sub     x0, xzr, x0");
        return;
    case ND_BITNOT:
        gen_expr(node->lhs);
        emit("    mvn     x0, x0");
        return;
    case ND_NOT:
        gen_expr(node->lhs);
        emit("    cmp     x0, 0");
        emit("    cset    x0, eq");
        return;
    case ND_LOGAND:
    case ND_LOGOR:
        gen_logical(node);
        return;
    default:
        break;
    }

    // The right operand first, so that the left one ends up in x0 and the right one in x1.
    gen_expr(node->rhs);
    push();
    gen_expr(node->lhs);
    pop("x1");

    switch (node->kind)
    {
    case ND_ADD:
        emit("    add     x0, x0, x1");
        return;
    case ND_SUB:
        emit("    sub     x0, x0, x1");
        return;
    case ND_MUL:
        emit("    mul     x0, x0, x1");
        return;
    case ND_DIV:
        emit("    sdiv    x0, x0, x1");
        return;
    case ND_MOD:
        // There is no remainder instruction: n - (n / d) * d.
        emit("    sdiv    x2, x0, x1");
        emit("    mul     x2, x2, x1");
        emit("    sub     x0, x0, x2");
        return;
    case ND_SHL:
        emit("    lsl     x0, x0, x1");
        return;
    case ND_SHR:
        emit("    asr     x0, x0, x1");
        return;
    case ND_BITAND:
        emit("    and     x0, x0, x1");
        return;
    case ND_BITXOR:
        emit("    eor     x0, x0, x1");
        return;
    case ND_BITOR:
        emit("    orr     x0, x0, x1");
        return;
    case ND_EQ:
        emit("    cmp     x0, x1");
        emit("    cset    x0, eq");
        return;
    case ND_NE:
        emit("    cmp     x0, x1");
        emit("    cset    x0, ne");
        return;
    case ND_LT:
        emit("    cmp     x0, x1");
        emit("    cset    x0, lt");
        return;
    case ND_LE:
        emit("    cmp     x0, x1");
        emit("    cset    x0, le");
        return;
    case ND_GT:
        emit("    cmp     x0, x1");
        emit("    cset    x0, gt");
        return;
    case ND_GE:
        emit("    cmp     x0, x1");
        emit("    cset    x0, ge");
        return;
    default:
        break;
    }
    error_tok(node->tok, "this expression cannot be compiled yet");
}

static void gen_stmt(const Node *node)
{
    switch (node->kind)
    {
    case ND_RETURN:
        if (node->lhs)
        {
            gen_expr(node->lhs);
        }
        emit("    b       L%d", return_label);
        return;
    case ND_BLOCK:
        for (const Node *stmt = node->body; stmt; stmt = stmt->next)
        {
            gen_stmt(stmt);
        }
        return;
    default:
        break;
    }
    error_tok(node->tok, "this statement cannot be compiled yet");
}

void codegen(const Function *prog, FILE *out)
{
    output = out;
    emit(".text");
    for (const Function *fn = prog; fn; fn = fn->next)
    {
        emit("");
        emit(".global _%s", fn->name);
        emit("_%s:", fn->name);

        // The frame record of docs/abi.md: the caller's fp, then the return address.
        emit("    sub     sp, sp, 8");
        emit("    str     fp, [sp]");
        emit("    str     lr, [sp, 4]");
        emit("    mov     fp, sp");

        return_label = new_label();
        gen_stmt(fn->body);
        assert(depth == 0);

        // Reaching the end of a function is a return of 0, which is what main needs.
        emit("    mov     x0, 0");
        emit_label(return_label);
        emit("    mov     sp, fp");
        emit("    ldr     lr, [fp, 4]");
        emit("    ldr     fp, [fp]");
        emit("    add     sp, sp, 8");
        emit("    ret");
    }
}
