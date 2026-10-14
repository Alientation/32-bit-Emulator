#include "ccompiler/type.h"

static Type void_type = {.kind = TY_VOID, .size = 1, .align = 1};
static Type int_type = {.kind = TY_INT, .size = 4, .align = 4};

Type *ty_void = &void_type;
Type *ty_int = &int_type;
