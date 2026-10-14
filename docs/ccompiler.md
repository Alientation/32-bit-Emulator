# C compiler

`bcc` compiles C to basm. It is written in C and is meant to compile itself and run on the emulator. Everything is in `core/ccompiler`. This describes what is there and the decisions the rest is built on; what is missing is listed under [Plan](#plan).

## What works

A first slice, end to end: a file of functions that return `int` or `void` and take no parameters, whose bodies are blocks and `return` statements, with integer constants and all the arithmetic, bitwise, shift, comparison and logical operators on `int`. Nothing else is accepted yet (no variables, calls, `if`, other types, strings or preprocessor); the lexer already knows every C17 keyword and punctuator except the digraphs (`<:`, `%:`, ...).

```
build/release/ccompiler/bcc -o hello.basm hello.c            # without -o the assembly goes to stdout
build/release/assembler/basm -o hello core/ccompiler/lib/crt0.basm hello.basm -outdir .
build/release/emulator32bit/emu32 -e hello.bexe              # the result of main is in x0
```

`crt0.basm` is the startup code: it sets `sp`, clears `fp` and `lr`, calls `_main` and halts. There are no arguments and no exit call yet.

## Pipeline

| Stage | Files | Does |
|-------|-------|------|
| driver | `src/main.c` | `bcc [-o out.basm] in.c`: reads the file, runs the stages, writes the assembly |
| lexer | `lex.h`, `src/lex.c` | file to tokens: integer constants (`0x`, `0b`, octal, `'` separators), identifiers, keywords, punctuators, comments |
| types | `type.h`, `src/type.c` | `ty_void`, `ty_int`; compared by pointer |
| parser | `parse.h`, `src/parse.c` | recursive descent to a typed AST (`Node`, `Function`); the binary operators are a table of precedence levels |
| code generator | `codegen.h`, `src/codegen.c` | AST to basm text |
| utilities | `util.h`, `src/util.c` | the arena (`arena_alloc`, nothing is freed), `error`, `error_at` |

The preprocessor will go between the lexer and the parser (`Token *preprocess (Token *)`); the tokens will need a flag for the start of a line and for white space before them.

## Grammar

The parser is recursive descent with **one token of lookahead and no backtracking**: each function looks at the current token and decides, and builds the AST as it goes (there is no separate parse tree). This is the grammar of what `src/parse.c` accepts today; the `///` comment above each parse function repeats its rule. Notation: `"x"` is a token, `a | b` a choice, `a?` optional, `a*` zero or more, `( )` grouping.

```
translation-unit = function*
function         = ("int" | "void") ident "(" "void"? ")" "{" stmt* "}"
stmt             = "return" expr? ";"
                 | "{" stmt* "}"
                 | ";"
expr             = binary(0)
binary(n)        = binary(n+1) (op(n) binary(n+1))*        left associative; binary(10) = unary
unary            = ("+" | "-" | "~" | "!") unary | primary
primary          = "(" expr ")" | num
```

`ident` is a `TK_IDENT` and `num` a `TK_NUM` (a constant that fits an `int`). The operators of `op(n)` are the rows of `binary_levels` in `parse.c`, the loosest first:

| n | Operators | Node kinds |
|---|-----------|------------|
| 0 | `\|\|` | `ND_LOGOR` |
| 1 | `&&` | `ND_LOGAND` |
| 2 | `\|` | `ND_BITOR` |
| 3 | `^` | `ND_BITXOR` |
| 4 | `&` | `ND_BITAND` |
| 5 | `==` `!=` | `ND_EQ` `ND_NE` |
| 6 | `<` `<=` `>` `>=` | `ND_LT` `ND_LE` `ND_GT` `ND_GE` |
| 7 | `<<` `>>` | `ND_SHL` `ND_SHR` |
| 8 | `+` `-` | `ND_ADD` `ND_SUB` |
| 9 | `*` `/` `%` | `ND_MUL` `ND_DIV` `ND_MOD` |

What the grammar does not say, the parser checks as it parses: `return expr;` is an error in a `void` function and `return;` in an `int` one, and a function name may be defined only once.

A new construct is added to this section in the same change as the parser. A new binary operator is a row of `binary_levels` and of the table above; assignment, `?:` and the comma operator are not binary levels and get functions of their own, looser than level 0.

**Where one token of lookahead stops being enough.** C is not LL(1), but each of these is still decided without backtracking:

| Construct | Needs | Planned solution |
|-----------|-------|------------------|
| Declaration or expression statement: `T * x;` | whether `T` is a typedef name | a scope table of names, asked at the start of a statement (`is_typename`, as in chibicc) |
| Cast or parenthesized expression: `(T) x`, `(x)` | the token after `(`, and the same table | the same question after `(` |
| A label, `name:` | the token after the identifier | `tok->next`; the tokens are a linked list, so this costs nothing |
| A function or a variable: `int f(void) {`, `int f;` | the declarator first | parse the declarator, then look for `{` |
| Compound literal or parenthesized type in `sizeof` | the token after `(` | the typename question again |

So the parser needs a table of the typedef names (and variables, which shadow them) that are visible, with scopes, before typedef and casts exist: that is a step of the [plan](#plan) (core C), and the lexer stays unaware of it.

## Language

The target is C17, plus the cheap parts of C23. The compiler is checked against this list, and the lexer's keyword table is the C17 list plus the C23 words that are taken.

| | Features |
|-|----------|
| Take from C23 | `bool`, `true`, `false`, `nullptr`, `static_assert`, `typeof`, `0b` constants, `'` digit separators, `{}` as an initializer, unnamed parameters, `[[attributes]]` (parsed and ignored), `#elifdef`, `#warning`; no K&R definitions, and `f()` means `f(void)` |
| Take, in stages | structs, unions, enums, typedef, function pointers, `switch`, `goto`, designated initializers, compound literals, flexible array members, variadic functions, bit-fields, `long long`, `float`, `double`, `_Generic` |
| Skip | VLAs, `_Complex`, `_Atomic` and threads, `_BitInt`, `constexpr`, decimal floating point, `#embed`, trigraphs. `long double` is a `double` |

## Self-hosting

"Self-hosting" is this, with the assembler and linker staying on the host (`basm` is C++ and does not run on the emulator):

1. The host compiler builds `bcc` (stage 1).
2. Stage 1 compiles the source of `bcc` to basm, host `basm` links it with `crt0` and a libc (stage 2, a `.bexe`).
3. `emu32` runs stage 2, which compiles the source of `bcc` again to basm (stage 3).
4. The basm of steps 2 and 3 has to be **byte for byte the same**.

For that the compiler is written in exactly the subset it compiles, in the order the [plan](#plan) adds it:

- Pure C. No VLAs (`-Werror=vla`), no GNU extensions, no `_Noreturn` or attributes, and no `long`, `float` or `double` (`tools/check_selfhost_subset.sh`, a ctest). `int`, `char`, `size_t` and `bool` are enough: the compiler does arithmetic on 32 bit values only, and the target `long` is 32 bit anyway (ILP32, [abi.md](abi.md#data-types)).
- No C++ code and no `util` library, so the `AEMU_FATAL` rules of the C++ code do not apply: an error prints a message and calls `exit (1)` ([style.md](style.md#c-the-compiler)).
- The library it needs is small and is written in C and basm in the [plan](#plan). The calls of the compiler today (`nm -u` of the debug objects): `fopen`, `fread`, `fwrite`, `fputc`, `fflush`, `ferror`, `fclose`, `fprintf`, `vfprintf` (`%s` and `%d`), `stdout`, `stderr`, `calloc`, `exit`, `memcpy`, `memcmp`, `strcmp`, `strncmp`, `strlen`, `strstr`, `assert`, and `va_list`. `fprintf` has to return the number of characters it wrote: `error_at` uses it to put the caret under the place.

## Code generation

- Every expression is evaluated into `x0`. A binary operation evaluates the right operand, pushes it (`str x0, [sp, -4]!`), evaluates the left one, and pops the right one into `x1`. It is slow and simple; the registers `x9`–`x15` and `x19`–`x27` are for a later register allocator.
- Every function has the frame record of [abi.md](abi.md#frame): the caller's `fp` at `[fp]`, the return address at `[fp, 4]`. A `return` branches to the epilogue; reaching the end of a function returns 0, which is what `main` needs.
- **Names:** a C function `name` is the global label `_name`, because basm does not allow a label that is a keyword (`add`, `b`, `sp`, `eq`, see [basm-syntax.md](basm-syntax.md#lexical-rules)) and no keyword starts with `_`. The compiler's own labels are `L<number>`. The same prefix goes for data when there is any. `_start` is `crt0`'s.
- Comparison results are `cset`, a division is `sdiv`, a remainder `sdiv`/`mul`/`sub`, `>>` is `asr` (signed `int`), a constant is `ldr x0, =value`.

## Tests

`core/integration_tests/ccompiler_integration/ccompiler_integration_test.cpp` runs the whole chain (`bcc`, `basm`, `emu32`) on C text in the test, and checks the value `main` returned and the error messages. A new feature is a case there, with the expected value worked out by hand and checked with the host compiler (the `int` expressions of the slice mean the same on the host). Label: `ccompiler_integration`; `ccompiler` is the subset check.

## Plan

Each step ends with the full `./build.sh` passing and the docs up to date (ISA and ABI decisions go in [abi.md](abi.md)).

0. **Done:** the harness and the slice above.
1. Core C: local variables, assignment and the compound forms, `if`/`while`/`for`, calls following [abi.md](abi.md#calling-convention), pointers and arrays, globals, string literals, `sizeof`, `char` and `short`.
2. Aggregates: structs and unions (passed by pointer, returned through a hidden pointer), typedef, enum, initializers, function pointers, `switch`, variadic functions.
3. The preprocessor (`#include`, `#define` with arguments, conditionals), the freestanding headers, `crt0` with `argc`/`argv` and `exit`, and the small libc above. This needs **semihosting calls for files** on the emulator (`exit`, `open`, `read`, `write`, `close`, the arguments of the program; `swi 1` has only debugging calls now), kept behind one `syscall.c` of the libc so that the operating system replaces just that file, and more RAM than the 32 pages of the default for `emu32`.
4. The bootstrap of the previous section.
5. `long long`, `float`, `double` (a value is one or two words; the type system and the code generator treat it so from the start), bit-fields, and `libbasmrt` (`__divdi3` and the others in [abi.md](abi.md#runtime-library)), compiled by the compiler itself.
6. Register allocation, constant folding, the remaining C23 parts.

The model for the structure is chibicc (Rui Ueyama), a self-hosting C11 compiler with a commit history that reads as a tutorial.
