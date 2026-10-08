#pragma once

#include "assembler/instruction_table.h"
#include "assembler/object_file.h"
#include "assembler/options.h"
#include "assembler/tokenizer.h"
#include "emulator32bit/emulator32bit_util.h"
#include "util/file.h"
#include <emulator32bit/emulator32bit.h>

#include <string>
#include <unordered_map>

/// @brief              Assembler to convert a basm assembly file into an object file.
///                     Specific to an assembly file, cannot reassemble this or another file.
///
/// @todo               TODO: Add assembly documentation
class Assembler
{
  public:
    /// @brief          Initializes the assembler and creates the output object file.
    /// @param processed_file   Input file to process. Post preprocessor.
    /// @param output_path      Output file path to write the object file to. If left empty, creates
    ///                         the object file as the same path as the input file with the .bo
    ///                         extension.
    explicit Assembler(const File processed_file, const std::string &output_path = "");

    /// @brief          Same, but takes the tokens straight from the preprocessor instead of
    ///                 lexing the .bi file again. Errors then point at the original source.
    /// @param processed_file   The .bi file the preprocessor wrote. Only used for its name.
    /// @param source           Result of Preprocessor::take_result().
    /// @param output_path      See above.
    Assembler(const File processed_file, basm::PreprocessedSource source,
              const std::string &output_path = "");

    /// @brief          Assembles the input assembly into an object file.
    void assemble();

    /// @brief          Get the output object file.
    /// @return         The file.
    File get_output_file() const;

    /// @brief          The object file that was assembled, the same that was written. Only valid
    ///                 once assemble() is done.
    const ObjectFile &object() const;

    /// @brief          Whether a warning ends the assembling like an error does (-W error).
    ///                 Off by default.
    void set_warnings_as_errors(bool enabled);

  private:
    bool m_warnings_as_errors = false;

    /// @brief Input .bi file that will be assembled.
    const File m_in_file;

    /// @brief Output .bo object file.
    File m_out_obj_file;

    /// @brief Whether assemble() ran already (it does its work once).
    bool m_assembled = false;

    /// @brief Owns the text that the tokens point into.
    std::shared_ptr<basm::SourceManager> m_sources;

    /// @brief Tokens of the input file.
    std::vector<basm::Token> m_tokens;

    /// @brief Sets up the output file and state. Common part of the constructors.
    void init(const File &processed_file, const std::string &output_path);

    /// @brief Read position in the tokens.
    basm::TokenCursor m_cursor;

    /// @brief Produced object file.
    ObjectFile m_obj;

    /// @brief Which section is currently assembling.
    enum class Section
    {
        /// @brief      No section declared.
        NONE,

        /// @brief      In the DATA section.
        DATA,

        /// @brief      In the BSS section.
        BSS,

        /// @brief      In the TEXT section.
        TEXT
    } m_cur_section = Section::NONE;

    /// @brief Index into the section table of the current section.
    U32 m_cur_section_index = U32(-1);

    /// @brief First token of the statement being assembled, where errors about the statement as
    ///        a whole (as opposed to one of its tokens) are reported.
    const basm::Token *m_statement = nullptr;

    /// @brief Total number of declared scopes. Monotically increasing.
    U32 m_total_scopes = 0;

    /// @brief Nested scope id.
    std::vector<U32> m_scopes;

    /// @brief The .scope of each of the scopes that are open, to say which one is not closed.
    std::vector<const basm::Token *> m_scope_sites;

    /// @brief How deep the expression being parsed is nested (parentheses, unary operators).
    int m_expression_depth = 0;

    /// @brief Whether .stop ended the assembling, the rest of the source (like the end of a scope)
    ///        was not looked at.
    bool m_stopped = false;

    /// @brief Logs the error at the token (file, line, column, source line) and terminates.
    [[noreturn]] void fail(const basm::Token &at, const std::string &message);

    /// @brief Logs a warning at the token (file, line, column, source line) and keeps assembling.
    void warn(const basm::Token &at, const std::string &message);

    /// @brief Consumes the next token if it has the type, otherwise fails with `message`.
    const basm::Token &expect(basm::TokenType type, const std::string &message);

    /// @brief Fails with `message` at the next token unless `condition` holds.
    void check(bool condition, const std::string &message);

    /// @brief A statement is followed by the end of its line.
    void expect_end_of_statement();

    /// @brief Evaluates an expression with the operators of C and its precedence:
    ///        `( )`, unary `- + ~ !`, `* / %`, `+ -`, `<< >>`, `< <= > >=`, `== !=`, `&`, `^`,
    ///        `|`, `&&`, `||` (lowest). Operands are number and character literals, constants
    ///        (`.equ`) and labels. The arithmetic is 64 bit and wraps; `/ % >>` and the
    ///        comparisons treat the values as signed, the comparisons and `! && ||` give 1 or 0.
    ///        Fails if there is no operand, on division by zero and on a shift by a negative amount
    ///        or by 64 or more. A constant has to be defined before the expression. The address of
    ///        a label is only known when linking, so the only things that can be done with one are
    ///        the difference of two labels of the same section that are defined (`end - start`),
    ///        a constant, and adding a number to it (`label + 4`), which is not a number but a
    ///        relocation with an addend (see parse_symbol_operand). Returns the bits of the signed
    ///        result (so `0 - 1` is all ones). Warns if the value is outside of [min, max], as an
    ///        unsigned number.
    dword parse_expression(dword min = 0, dword max = -1);

    /// @brief Same as parse_expression() with the result as a signed number. For the operands
    ///        that can be negative (an offset). Does not check a range.
    sdword parse_signed_expression();

    /// @brief Whether the next token can start an expression.
    bool at_expression();

    /// @brief A value while an expression is evaluated: a number, or a symbol plus a number (the
    ///        symbol's offset in its section, if it is defined yet, plus what was added to it).
    ///        A symbol with a number added can be subtracted from another symbol of the same
    ///        section, which makes a number, or be the target of a relocation, whose addend is
    ///        `value - base`.
    struct ExprValue
    {
        sdword value = 0;

        /// @brief The symbol this value is based on, null for a number.
        const basm::Token *label = nullptr;

        /// @brief Section of that symbol, U32(-1) if it is not defined (yet).
        U32 section = U32(-1);

        /// @brief Offset of the symbol in its section, 0 if it is not defined.
        sdword base = 0;
    };

    /// @brief Parses the operand `symbol`, `symbol + number` or `symbol - number` of a relocation
    ///        (`adrp`, `:lo12:`, `.word`, a branch) and fails with `expected` if the expression
    ///        is not based on a symbol.
    ExprValue parse_symbol_operand(const std::string &expected);

    /// @brief Adds the relocation of `target`, a symbol with an addend, to `relocations`. It is at
    ///        `offset` in the section of the relocations.
    void add_relocation(std::vector<ObjectFile::RelocationEntry> &relocations, word offset,
                        ObjectFile::RelocationEntry::Type type, const ExprValue &target);

    /// @brief Precedence climbing: the part of an expression made of operators of at least
    ///        `min_precedence`.
    ExprValue parse_binary_expression(int min_precedence);
    ExprValue parse_unary_expression();

    /// @brief Value of the constant or label that `symbol` names here (the innermost scope that has
    ///        it), or fails if there is none yet.
    ExprValue lookup_symbol(const basm::Token &symbol);

    /// @brief Fails unless the value is a number and not the offset of a label.
    void require_number(const ExprValue &value);

    /// @brief Name of a constant or label in the scope that is open, the same as its symbol.
    std::string scoped_name(const std::string &name) const;

    /// @brief The values of `.equ`, by name (with the scope for those defined in one).
    std::unordered_map<std::string, sdword> m_constants;

    /// @brief Comma separated expressions.
    std::vector<dword> parse_arguments();

    /// @brief Shared by .byte, .dbyte, .word, ... Appends the arguments to .data, `n_bytes` each.
    void define_data(const char *directive, U8 n_bytes);

    byte parse_sysreg();

    byte parse_register();

    void parse_shift(ShiftType &shift, int &shift_amt);

    /// @brief Operations of the form `op xd, xn, <xm[, shift] | imm14 | :lo12:symbol>`.
    /// @param implicit_dest The destination is not written, it is xzr (cmp, cmn, tst, teq).
    word parse_format_o(byte opcode, bool implicit_dest = false);
    word parse_format_o1(byte opcode);
    word parse_format_o2(byte opcode);
    word parse_format_o3(byte opcode);
    word parse_format_m(byte opcode);
    word parse_format_m1(byte opcode);
    word parse_format_b1(byte opcode);
    word parse_format_b2(byte opcode);
    word parse_format_swi(byte opcode);
    word parse_format_atomic(byte width, byte atopcode);
    void fill_local();
    void fill_local(std::vector<ObjectFile::RelocationEntry> &relocations, bool fill_branches);

    ///
    /// Assembler directives.
    ///
    void _global();
    void _extern();
    void _equ();
    void _org();
    void _scope();
    void _scend();
    void _advance();
    void _align();
    void _section();
    void _text();
    void _data();
    void _bss();
    void _stop();
    void _byte();
    void _dbyte();
    void _word();
    void _dword();
    void _sbyte();
    void _sdbyte();
    void _sword();
    void _sdword();
    void _char();
    void _ascii();
    void _asciz();

    ///
    /// Instructions.
    ///

    /// @brief Assembles the instruction at the cursor. How depends on its row in
    ///        instruction_list.h.
    void assemble_instruction(const basm::InstructionSpec &spec);

    // The instructions that have an operand syntax of their own.
    void _hlt();
    void _nop();
    void _eret();
    void _wfi();
    void _brk();
    void _msr();
    void _mrs();
    void _ret();

    using DirectiveFunction = void (Assembler::*)();
    /// @brief Function pointers to process an assembler directive.
    std::unordered_map<basm::TokenType, DirectiveFunction> m_directive_handlers = {
        {basm::TokenType::ASSEMBLER_GLOBAL, &Assembler::_global},
        {basm::TokenType::ASSEMBLER_EXTERN, &Assembler::_extern},
        {basm::TokenType::ASSEMBLER_EQU, &Assembler::_equ},
        {basm::TokenType::ASSEMBLER_ORG, &Assembler::_org},
        {basm::TokenType::ASSEMBLER_SCOPE, &Assembler::_scope},
        {basm::TokenType::ASSEMBLER_SCEND, &Assembler::_scend},
        {basm::TokenType::ASSEMBLER_ADVANCE, &Assembler::_advance},
        {basm::TokenType::ASSEMBLER_ALIGN, &Assembler::_align},
        {basm::TokenType::ASSEMBLER_SECTION, &Assembler::_section},
        {basm::TokenType::ASSEMBLER_TEXT, &Assembler::_text},
        {basm::TokenType::ASSEMBLER_DATA, &Assembler::_data},
        {basm::TokenType::ASSEMBLER_BSS, &Assembler::_bss},
        {basm::TokenType::ASSEMBLER_STOP, &Assembler::_stop},
        {basm::TokenType::ASSEMBLER_BYTE, &Assembler::_byte},
        {basm::TokenType::ASSEMBLER_DBYTE, &Assembler::_dbyte},
        {basm::TokenType::ASSEMBLER_WORD, &Assembler::_word},
        {basm::TokenType::ASSEMBLER_DWORD, &Assembler::_dword},
        {basm::TokenType::ASSEMBLER_SBYTE, &Assembler::_sbyte},
        {basm::TokenType::ASSEMBLER_SDBYTE, &Assembler::_sdbyte},
        {basm::TokenType::ASSEMBLER_SWORD, &Assembler::_sword},
        {basm::TokenType::ASSEMBLER_SDWORD, &Assembler::_sdword},
        {basm::TokenType::ASSEMBLER_CHAR, &Assembler::_char},
        {basm::TokenType::ASSEMBLER_ASCII, &Assembler::_ascii},
        {basm::TokenType::ASSEMBLER_ASCIZ, &Assembler::_asciz},
    };
};