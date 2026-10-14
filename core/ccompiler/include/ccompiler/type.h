#ifndef CCOMPILER_TYPE_H
#define CCOMPILER_TYPE_H

typedef enum
{
    TY_VOID,
    TY_INT,
} TypeKind;

typedef struct
{
    TypeKind kind;
    /// Size and alignment in bytes, as in docs/abi.md.
    int size;
    int align;
} Type;

/// The types that exist. They are compared by pointer.
extern Type *ty_void;
extern Type *ty_int;

#endif
