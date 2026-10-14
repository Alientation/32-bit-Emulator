#ifndef CCOMPILER_TYPE_H
#define CCOMPILER_TYPE_H

#include <stdbool.h>

typedef enum
{
    TY_VOID,
    TY_CHAR,
    TY_SHORT,
    TY_INT,
    /// `long`, which is as wide as an `int` (docs/abi.md) but a type of its own.
    TY_LONG,
    TY_LLONG,
    TY_FLOAT,
    TY_DOUBLE,
    /// `long double`, which has the size and the representation of a `double`.
    TY_LDOUBLE,
} TypeKind;

typedef struct
{
    TypeKind kind;
    /// Size and alignment in bytes, as in docs/abi.md.
    int size;
    int align;
    /// Whether an integer type is unsigned. A plain `char` is signed.
    bool is_unsigned;
    /// The name as it is written in C, for messages.
    const char *name;
} Type;

/// The types that exist. They are compared by pointer. The lexer gives each constant one of them;
/// the parser takes only `int` so far.
extern Type *ty_void;
extern Type *ty_char;
extern Type *ty_uchar;
extern Type *ty_short;
extern Type *ty_ushort;
extern Type *ty_int;
extern Type *ty_uint;
extern Type *ty_long;
extern Type *ty_ulong;
extern Type *ty_llong;
extern Type *ty_ullong;
extern Type *ty_float;
extern Type *ty_double;
extern Type *ty_ldouble;

#endif
