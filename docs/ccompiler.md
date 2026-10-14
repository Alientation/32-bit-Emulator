# C compiler

`bcc` compiles C to basm. It is written in C and is meant to compile itself and run on the emulator. Everything is in `core/ccompiler`. This describes what is there and the decisions the rest is built on; what is missing is listed under [Plan](#plan).

## What works

The lexer and the preprocessor are complete: every token, constant and string literal of C17 with the C23 parts of [Language](#language), and every directive of C17 ([Lexer and preprocessor](#lexer-and-preprocessor)).

The parser and the code generator are a first slice, end to end: a file of functions that return `int` or `void` and take no parameters, whose bodies are blocks and `return` statements, with `int` constants (character constants among them) and all the arithmetic, bitwise, shift, comparison and logical operators on `int`. Nothing else is accepted yet (no variables, calls, `if`, other types or strings). A constant of another type (`12u`, `2147483648`, `1.5`) is an error until the parser knows its type.

```
build/release/ccompiler/bcc -o hello.basm hello.c            # without -o the assembly goes to stdout
build/release/ccompiler/bcc -E -I include -D DEBUG hello.c   # only the preprocessed text
build/release/assembler/basm -o hello core/ccompiler/lib/crt0.basm hello.basm -outdir .
build/release/emulator32bit/emu32 -e hello.bexe              # the result of main is in x0
```

`crt0.basm` is the startup code: it sets `sp`, clears `fp` and `lr`, calls `_main` and halts. There are no arguments and no exit call yet.

## Pipeline

| Stage | Files | Does |
|-------|-------|------|
| driver | `src/main.c` | `bcc [-E] [-o out] [-I dir] [-D name[=value]] [-U name] in.c`: reads the file, runs the stages, writes the assembly (with `-E` the preprocessed text) |
| lexer | `lex.h`, `src/lex.c` | file to preprocessing tokens (`tokenize`); after the preprocessor, those to the parser's tokens (`convert_pp_tokens`): keywords, constants with their type and value, string literals as bytes |
| preprocessor | `preprocess.h`, `src/preprocess.c` | the directives, macro expansion and included files (`preprocess`); `print_tokens` for `-E` |
| types | `type.h`, `src/type.c` | `ty_void`, the integer and floating types; compared by pointer |
| parser | `parse.h`, `src/parse.c` | recursive descent to a typed AST (`Node`, `Function`); the binary operators are a table of precedence levels |
| code generator | `codegen.h`, `src/codegen.c` | AST to basm text |
| utilities | `util.h`, `src/util.c` | the arena (`arena_alloc`, nothing is freed), `error`, `verror_at`, `vwarn_at`, `Int64` (64 bit arithmetic on two words), `HashMap` |

`main` runs `tokenize`, `preprocess`, `convert_pp_tokens`, `parse` and `codegen`. Between the last two it rejects every constant that is not an `int`, since the parser reads the value of any `TK_NUM` as one; that check goes when the parser has the other types.

## Lexer and preprocessor

**Phases of translation.** `new_source_file` does phases 1 and 2: `\r\n` and `\r` are `\n`, a UTF-8 byte order mark is dropped, and a backslash at the end of a line joins it to the next, with the newline put back after the joined line so that the lines after it keep their numbers (an error on a joined line is reported at its first line). `tokenize` makes preprocessing tokens (phase 3): a comment is a space, and each token records whether it starts a line (`at_bol`) and has space before it (`has_space`), which is what the directives, `#` and `-E` need. The keywords stay names until `convert_pp_tokens` (phase 7), so `#if`, `#define bool _Bool` and `defined(int)` work. A number is read as a pp-number (`1.2.3e+4_x` is one) and checked only when it is converted, and a character that starts no token (`@`, a quote without its end) is a `TK_PP_OTHER`: both are errors only if they are still there after preprocessing, so `#if 0` can hold `don't`.

**Constants.**

- An integer constant is decimal, `0x`, `0b` or octal, with `'` between digits and the suffixes `u`, `l` and `ll` in either order. Its type is the first of the list of C17 6.4.4.1 that holds the value, with the sizes of [abi.md](abi.md#data-types) (`long` is 32 bit): `2147483648` is a `long long` and `0x80000000` an `unsigned int`. The value has up to 64 bits, in `value` and `value_high`.
- A floating constant (decimal or hexadecimal, `f` and `l`) gets its type and keeps its spelling: turning it into a value needs floating point in the compiler, which is step 5 of the [plan](#plan).
- A plain character constant is an `int`, the value of the `char` (which is signed: `'\377'` is -1). Two to four characters are a multi-character constant with GCC's value (the first byte highest) and a warning. With `u`, `U` or `L` it is one code unit of `char16_t` (`unsigned short`), `char32_t` (`unsigned int`) or `wchar_t` (`int`).
- A string literal is its bytes in the target's layout, little endian elements and the 0 element at the end. `\u` and `\U` are UTF-8 in a plain or `u8` string, UTF-16 in a `u` and UTF-32 in a `U` or `L` one, which also decode the UTF-8 of the source. String literals next to each other are one, with the prefix that one of them has; two different prefixes are an error.
- A digraph (`<:`, `%:%:`, ...) keeps its spelling, for `#` and `-E`, and `token_is` compares what it stands for.

**Macros.** Expansion is Prosser's algorithm, as in chibicc: each token has a hide set of the macros it came out of, which may not expand on it again; for a function-like macro the hide set is the intersection of those of the name and the `)`. The arguments are expanded before they are put in, except the operands of `#` and `##`. An empty operand of `##` is a placemarker token while the replacement is made. A token that the preprocessor makes (pasted, stringized, `__LINE__`) is lexed from a text of its own, so that every token points into the text of a file. Besides C17 there are C23's `__VA_OPT__` and the variable arguments of a call that can be left out. A macro that is defined again differently is an error, the same definition again is not.

**Directives.** All of C17 and C23's `#elifdef`, `#elifndef` and `#warning`.

- `#include "file"` searches the directory of the file with the `#include`, then the `-I` directories in their order; `#include <file>` only the `-I` directories (there are no headers of the compiler yet). An included file is at most 200 deep, and a conditional has to end in the file it starts in.
- `#pragma once` (by the name the file was opened with) and `_Pragma("once")` do what GCC does; other pragmas are ignored.
- `#if` computes in 64 bits, as C17 6.10.1 has it (`intmax_t` is `long long`), with `Int64` since the compiler's source has no 64 bit type. A name that is left after expansion is 0, `true` is 1, `defined` and `__has_include` are replaced before the expansion. Division by 0 and a shift count outside 0 to 63 are errors where they are evaluated (not in `0 && 1 / 0`); a signed overflow wraps.
- An error stops the compiler; `#warning`, tokens after a directive that takes none, a macro name without white space after it and a multi-character constant are warnings.

**Predefined macros.** `__STDC__` (1), `__STDC_VERSION__` (`201710L`), `__STDC_HOSTED__` (1), `__STDC_UTF_16__`, `__STDC_UTF_32__`, `__STDC_NO_ATOMICS__`, `__STDC_NO_COMPLEX__`, `__STDC_NO_THREADS__`, `__STDC_NO_VLA__`, `__bcc__`, `__ILP32__` and `_ILP32`, the sizes (`__SIZEOF_INT__` and the others, `__CHAR_BIT__`), the types of the standard headers (`__SIZE_TYPE__` is `unsigned int`, `__WCHAR_TYPE__` `int`, ...), their limits (`__INT_MAX__`, ...) and the byte order (`__BYTE_ORDER__` is `__ORDER_LITTLE_ENDIAN__`); the list is `predefined_macros` in `preprocess.c`. `__GNUC__` is not defined, since the GNU extensions are not there. `__FILE__`, `__LINE__` (of the macro call a token came out of) and `__COUNTER__` are worked out where they are used. `__DATE__` is `"Jan  1 1970"` and `__TIME__` `"00:00:00"`, which the standard allows when there is no date: the output depends on nothing but the input (the [bootstrap](#self-hosting) compares two outputs byte for byte), and the libc needs no clock.

**Not there:** `#include_next`, `#embed` and `__has_embed`, `__has_c_attribute`, GCC's `, ## __VA_ARGS__` and named variadic parameters (`args...`), and universal character names in names (a name may have UTF-8 characters). `-E` writes no line markers; it starts a line where a token started one, and puts a space between two tokens that would be read as other tokens without it.

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

`ident` is a `TK_IDENT` and `num` a `TK_NUM` of type `int` (the driver rejects the others). The operators of `op(n)` are the rows of `binary_levels` in `parse.c`, the loosest first:

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
| Take from C23 | `bool`, `true`, `false`, `nullptr`, `static_assert`, `typeof`, `0b` constants, `'` digit separators, `{}` as an initializer, unnamed parameters, `[[attributes]]` (parsed and ignored), `#elifdef`, `#warning`, `__VA_OPT__`, `__has_include`; no K&R definitions, and `f()` means `f(void)` |
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
- The library it needs is small and is written in C and basm in the [plan](#plan). The calls of the compiler today (`nm -u` of the debug objects): `fopen`, `fread`, `fwrite`, `fputc`, `fflush`, `ferror`, `fclose`, `fprintf`, `vfprintf` (`%s` and `%d`), `stdout`, `stderr`, `calloc`, `exit`, `memcpy`, `memcmp`, `strchr`, `strcmp`, `strncmp`, `strlen`, `strstr`, `assert`, and `va_list`. `fprintf` has to return the number of characters it wrote: `error_at` uses it to put the caret under the place.

## Code generation

- Every expression is evaluated into `x0`. A binary operation evaluates the right operand, pushes it (`str x0, [sp, -4]!`), evaluates the left one, and pops the right one into `x1`. It is slow and simple; the registers `x9`–`x15` and `x19`–`x27` are for a later register allocator.
- Every function has the frame record of [abi.md](abi.md#frame): the caller's `fp` at `[fp]`, the return address at `[fp, 4]`. A `return` branches to the epilogue; reaching the end of a function returns 0, which is what `main` needs.
- **Names:** a C function `name` is the global label `_name`, because basm does not allow a label that is a keyword (`add`, `b`, `sp`, `eq`, see [basm-syntax.md](basm-syntax.md#lexical-rules)) and no keyword starts with `_`. The compiler's own labels are `L<number>`. The same prefix goes for data when there is any. `_start` is `crt0`'s.
- Comparison results are `cset`, a division is `sdiv`, a remainder `sdiv`/`mul`/`sub`, `>>` is `asr` (signed `int`), a constant is `ldr x0, =value`.

## Tests

`core/integration_tests/ccompiler_integration/ccompiler_integration_test.cpp` runs the whole chain (`bcc`, `basm`, `emu32`) on C text in the test, and checks the value `main` returned and the error messages. A new feature is a case there, with the expected value worked out by hand and checked with the host compiler (the `int` expressions of the slice mean the same on the host). Label: `ccompiler_integration`.

The lexer and the preprocessor also have unit tests, `core/ccompiler/tests/lex_test.cpp` and `preprocess_test.cpp`, which call the library from C++ (the headers in `extern "C"`) and check the errors as death tests, since an error exits. The tokens of a preprocessed text are compared as `print_tokens` writes them; an expected expansion is one of the examples of the C standard or what the host's `cpp -P` makes of the same text. Label: `ccompiler`, with the subset check.

## Plan

Each step ends with the full `./build.sh` passing and the docs up to date (ISA and ABI decisions go in [abi.md](abi.md)).

0. **Done:** the harness and the slice above, and the lexer and the preprocessor.
1. Core C: local variables, assignment and the compound forms, `if`/`while`/`for`, calls following [abi.md](abi.md#calling-convention), pointers and arrays, globals, string literals, `sizeof`, `char` and `short`.
2. Aggregates: structs and unions (passed by pointer, returned through a hidden pointer), typedef, enum, initializers, function pointers, `switch`, variadic functions.
3. The freestanding headers (the preprocessor is done), `crt0` with `argc`/`argv` and `exit`, and the small libc above. This needs **semihosting calls for files** on the emulator (`exit`, `open`, `read`, `write`, `close`, the arguments of the program; `swi 1` has only debugging calls now), kept behind one `syscall.c` of the libc so that the operating system replaces just that file, and more RAM than the 32 pages of the default for `emu32`.
4. The bootstrap of the previous section.
5. `long long`, `float`, `double` (a value is one or two words; the type system and the code generator treat it so from the start), bit-fields, and `libbasmrt` (`__divdi3` and the others in [abi.md](abi.md#runtime-library)), compiled by the compiler itself.
6. Register allocation, constant folding, the remaining C23 parts.

The model for the structure is chibicc (Rui Ueyama), a self-hosting C11 compiler with a commit history that reads as a tutorial.
