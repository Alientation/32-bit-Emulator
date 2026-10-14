#include "ccompiler/type.h"

static Type void_type = {.kind = TY_VOID, .size = 1, .align = 1, .name = "void"};
static Type char_type = {.kind = TY_CHAR, .size = 1, .align = 1, .name = "char"};
static Type uchar_type = {
    .kind = TY_CHAR, .size = 1, .align = 1, .is_unsigned = true, .name = "unsigned char"};
static Type short_type = {.kind = TY_SHORT, .size = 2, .align = 2, .name = "short"};
static Type ushort_type = {
    .kind = TY_SHORT, .size = 2, .align = 2, .is_unsigned = true, .name = "unsigned short"};
static Type int_type = {.kind = TY_INT, .size = 4, .align = 4, .name = "int"};
static Type uint_type = {
    .kind = TY_INT, .size = 4, .align = 4, .is_unsigned = true, .name = "unsigned int"};
static Type long_type = {.kind = TY_LONG, .size = 4, .align = 4, .name = "long"};
static Type ulong_type = {
    .kind = TY_LONG, .size = 4, .align = 4, .is_unsigned = true, .name = "unsigned long"};
static Type llong_type = {.kind = TY_LLONG, .size = 8, .align = 4, .name = "long long"};
static Type ullong_type = {
    .kind = TY_LLONG, .size = 8, .align = 4, .is_unsigned = true, .name = "unsigned long long"};
static Type float_type = {.kind = TY_FLOAT, .size = 4, .align = 4, .name = "float"};
static Type double_type = {.kind = TY_DOUBLE, .size = 8, .align = 4, .name = "double"};
static Type ldouble_type = {.kind = TY_LDOUBLE, .size = 8, .align = 4, .name = "long double"};

Type *ty_void = &void_type;
Type *ty_char = &char_type;
Type *ty_uchar = &uchar_type;
Type *ty_short = &short_type;
Type *ty_ushort = &ushort_type;
Type *ty_int = &int_type;
Type *ty_uint = &uint_type;
Type *ty_long = &long_type;
Type *ty_ulong = &ulong_type;
Type *ty_llong = &llong_type;
Type *ty_ullong = &ullong_type;
Type *ty_float = &float_type;
Type *ty_double = &double_type;
Type *ty_ldouble = &ldouble_type;
