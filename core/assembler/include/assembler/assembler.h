#pragma once

#include "assembler/instruction_table.h"
#include "assembler/object_file.h"
#include "assembler/options.h"
#include "assembler/tokenizer.h"
#include "emulator32bit/emulator32bit.h"
#include "emulator32bit/emulator32bit_util.h"
#include "util/file.h"

#include <string>
#include <unordered_map>

/// Assembler to convert a basm assembly file into an object file.
/// Specific to an assembly file, cannot reassemble this or another file. The syntax it reads is in
/// docs/basm-syntax.md.
class Assembler
{
  public:
    /// Initializes the assembler: creates the output object file and lexes the input file (a
    /// lexical error is fatal).
    ///
    /// @param[in] processed_file Input file to process. Post preprocessor.
    /// @param[in] output_path Output file path to write the object file to. If left empty, the
    ///     object file has the path of the input file with the .bo extension.
    explicit Assembler(const File &processed_file, const std::string &output_path = "");

    /// Same, but takes the tokens straight from the preprocessor instead of
    /// lexing the .bi file again. Errors then point at the original source.
    ///
    /// @param[in] processed_file The .bi file the preprocessor wrote. Only used for its name.
    /// @param[in] source Result of Preprocessor::take_result().
    /// @param[in] output_path Output file path to write the object file to. If left empty, the
    ///     object file has the path of the input file with the .bo extension.
    Assembler(const File &processed_file, basm::PreprocessedSource source,
              const std::string &output_path = "");

    /// Assembles the input assembly into an object file and writes it. Does nothing if it was done
    /// already. A syntax error is fatal.
    void assemble();

    /// @return The object file that assemble () writes.
    File get_output_file() const;

    /// @return The object file that was assembled, the same that was written. Only valid once
    ///     assemble () is done.
    const ObjectFile &object() const;

    /// Chooses whether a warning ends the assembling like an error does (-W error). Off by
    /// default.
    ///
    /// @param[in] enabled Whether a warning is an error.
    void set_warnings_as_errors(bool enabled);

  private:
    bool m_warnings_as_errors = false;

    /// Input .bi file that will be assembled.
    const File m_in_file;

    /// Output .bo object file.
    File m_out_obj_file;

    /// Whether assemble() ran already (it does its work once).
    bool m_assembled = false;

    /// Owns the text that the tokens point into.
    std::shared_ptr<basm::SourceManager> m_sources;

    /// Tokens of the input file.
    std::vector<basm::Token> m_tokens;

    /// Sets up the output file and state. Common part of the constructors.
    ///
    /// @param[in] processed_file The input file.
    /// @param[in] output_path The path of the object file, empty for the default.
    void init(const File &processed_file, const std::string &output_path);

    /// Read position in the tokens.
    basm::TokenCursor m_cursor;

    /// Produced object file.
    ObjectFile m_obj;

    /// Which section is currently assembling.
    enum class Section
    {
        /// No section declared.
        NONE,

        /// In the DATA section.
        DATA,

        /// In the BSS section.
        BSS,

        /// In the TEXT section.
        TEXT,

        /// In the RODATA section.
        RODATA,

        /// In the INIT_ARRAY section.
        INIT_ARRAY,

        /// In FINI_ARRAY section.
        FINI_ARRAY,

        /// In a section of the program's own (`.section "name"`).
        USER,

        /// In a section of the program's own that is zero filled (`nobits`).
        USER_BSS
    } m_cur_section = Section::NONE;

    /// @return The section being assembled if it is one of .data, .rodata, .init_array,
    /// .fini_array, null for .text, .bss, a user section and outside of a section.
    const ObjectFile::ByteSection *current_byte_section() const;

    /// @return The user section being assembled, null if it is another one.
    ObjectFile::UserSection *current_user_section();

    /// The size of the current section if it is .bss or a user section that is `nobits`,
    /// which is what .org, .advance and .align move. Only valid in those sections.
    ///
    /// @return The size, which the caller changes.
    word &zero_size();

    /// @return Whether the current section holds bytes, as opposed to code or a size: .data,
    /// .rodata, .init_array, .fini_array and any user section.
    bool in_byte_section() const;

    /// The bytes of that section.
    ///
    /// @return The bytes, which the caller appends to.
    std::vector<byte> &section_bytes();

    /// The relocations of that section.
    ///
    /// @return The relocations, which the caller appends to.
    std::vector<ObjectFile::RelocationEntry> &section_relocations();

    /// Makes a byte section the current one, as .data and .rodata do.
    ///
    /// @param[in] section Which section it is.
    /// @param[in] name The name of the section in the section table.
    void enter_byte_section(Section section, const char *name);

    /// Fails unless `name`, the name of a section in `.section`, is one the program can
    /// give to a section of its own.
    ///
    /// @param[in] at The token of the name, for the error.
    /// @param[in] name The name.
    void check_section_name(const basm::Token &at, const std::string &name);

    /// @return Whether instructions can be assembled in the current section: .text, or a user
    /// section that is executable.
    bool in_code_section() const;

    /// Appends an instruction to the current code section.
    ///
    /// @param[in] instruction The encoded instruction.
    void emit_instruction(word instruction);

    /// Emits `mov to, from`.
    ///
    /// @param[in] to The destination register.
    /// @param[in] from The source register.
    void emit_move(byte to, byte from);

    /// @return Where the next instruction goes, as an offset in the current code section.
    word code_offset();

    /// The relocations of the current code section.
    ///
    /// @return The relocations, which the caller appends to.
    std::vector<ObjectFile::RelocationEntry> &code_relocations();

    /// Index into the section table of the current section.
    U32 m_cur_section_index = U32(-1);

    /// First token of the statement being assembled, where errors about the statement as
    /// a whole (as opposed to one of its tokens) are reported.
    const basm::Token *m_statement = nullptr;

    /// Total number of declared scopes. Monotonically increasing.
    U32 m_total_scopes = 0;

    /// Nested scope id.
    std::vector<U32> m_scopes;

    /// The .scope of each of the scopes that are open, to say which one is not closed.
    std::vector<const basm::Token *> m_scope_sites;

    /// The sections that .pushsection left, to go back to with .popsection, and the
    /// .pushsection of each to say which one is not closed.
    struct SavedSection
    {
        Section section;
        U32 index;
        const basm::Token *site;
    };

    std::vector<SavedSection> m_saved_sections;

    /// How deep the expression being parsed is nested (parentheses, unary operators).
    size_t m_expression_depth = 0;

    /// Whether .stop ended the assembling, so the rest of the source (like the end of a scope)
    /// was not looked at.
    bool m_stopped = false;

    /// Logs the error at the token (file, line, column, source line) and terminates.
    ///
    /// @param[in] at The token the error is about.
    /// @param[in] message What is wrong.
    [[noreturn]] void fail(const basm::Token &at, const std::string &message);

    /// Logs a warning at the token (file, line, column, source line) and keeps assembling.
    ///
    /// @param[in] at The token the warning is about.
    /// @param[in] message What is doubtful.
    void warn(const basm::Token &at, const std::string &message);

    /// Consumes the next token if it has the type, otherwise fails with `message`.
    ///
    /// @param[in] type The type the token has to have.
    /// @param[in] message What to say if it does not.
    /// @return The consumed token.
    const basm::Token &expect(basm::TokenType type, const std::string &message);

    /// Fails with `message` at the next token unless `condition` holds.
    ///
    /// @param[in] condition What has to hold.
    /// @param[in] message What to say if it does not.
    void check(bool condition, const std::string &message);

    /// A statement is followed by the end of its line.
    void expect_end_of_statement();

    /// Evaluates an expression with the operators of C and its precedence:
    /// `( )`, unary `- + ~ !`, `* / %`, `+ -`, `<< >>`, `< <= > >=`, `== !=`, `&`, `^`,
    /// `|`, `&&`, `||` (lowest). Operands are number and character literals, constants
    /// (`.equ`) and labels. The arithmetic is 64 bit and wraps; `/ % >>` and the
    /// comparisons treat the values as signed, the comparisons and `! && ||` give 1 or 0.
    /// Fails if there is no operand, on division by zero and on a shift by a negative amount
    /// or by 64 or more. A constant has to be defined before the expression. The address of
    /// a label is only known when linking, so the only things that can be done with one are
    /// the difference of two labels of the same section that are defined (`end - start`),
    /// a constant, and adding a number to it (`label + 4`), which is not a number but a
    /// relocation with an addend (see parse_symbol_operand). Returns the bits of the signed
    /// result (so `0 - 1` is all ones). Warns if the value is outside of [min, max], as an
    /// unsigned number.
    ///
    /// @param[in] min The lowest value that does not get a warning.
    /// @param[in] max The highest value that does not get a warning.
    /// @return The bits of the value.
    dword parse_expression(dword min = 0, dword max = -1);

    /// Same as parse_expression() with the result as a signed number. For the operands
    /// that can be negative (an offset). Does not check a range.
    ///
    /// @return The value.
    sdword parse_signed_expression();

    /// @return Whether the next token can start an expression.
    bool at_expression();

    /// A value while an expression is evaluated: a number, or a symbol plus a number (the
    /// symbol's offset in its section, if it is defined yet, plus what was added to it).
    /// A symbol with a number added can be subtracted from another symbol of the same
    /// section, which makes a number, or be the target of a relocation, whose addend is
    /// `value - base`.
    struct ExprValue
    {
        sdword value = 0;

        /// The symbol this value is based on, null for a number.
        const basm::Token *label = nullptr;

        /// Section of that symbol, U32(-1) if it is not defined (yet).
        U32 section = U32(-1);

        /// Offset of the symbol in its section, 0 if it is not defined.
        sdword base = 0;
    };

    /// Parses the operand `symbol`, `symbol + number` or `symbol - number` of a relocation
    /// (`adrp`, `:lo12:`, `.word`, a branch) and fails with `expected` if the expression
    /// is not based on a symbol.
    ///
    /// @param[in] expected What to say if there is no symbol.
    /// @return The symbol and the number added to it.
    ExprValue parse_symbol_operand(const std::string &expected);

    /// Adds the relocation of `target`, a symbol with an addend, to `relocations`. It is at
    /// `offset` in the section of the relocations.
    ///
    /// @param[in,out] relocations The relocations of the section the relocation is for.
    /// @param[in] offset Where the relocation applies, in that section.
    /// @param[in] type The kind of relocation.
    /// @param[in] target The symbol and the addend.
    void add_relocation(std::vector<ObjectFile::RelocationEntry> &relocations, word offset,
                        ObjectFile::RelocationEntry::Type type, const ExprValue &target);

    /// Precedence climbing: parses the part of an expression made of operators of at least
    /// `min_precedence`. All the binary operators associate to the left.
    ///
    /// @param[in] min_precedence The lowest precedence of an operator that this call may consume.
    /// @return The value of that part of the expression.
    ExprValue parse_binary_expression(S32 min_precedence);

    /// operand := number | char | constant | label
    /// | '(' expression ')' | ('-' | '+' | '~' | '!') operand
    ///
    /// @return The value of the operand.
    ExprValue parse_unary_expression();

    /// Looks `symbol` up the way a label is: the scopes that are open from the innermost out,
    /// then the file. A constant is a number. A label that is defined is its offset in its
    /// section. Anything else is a symbol that is not defined yet (it can be later in the file,
    /// or in another file), which has no value but can be the target of a relocation.
    ///
    /// @param[in] symbol The identifier to look up.
    /// @return The value of the constant or label that `symbol` names here.
    ExprValue lookup_symbol(const basm::Token &symbol);

    /// Fails unless the value is a number and not the offset of a label.
    ///
    /// @param[in] value The value to check.
    void require_number(const ExprValue &value);

    /// Name of a constant or label in the scope that is open, the same as its symbol.
    ///
    /// @param[in] name The name as written.
    /// @return The name with the scope.
    std::string scoped_name(const std::string &name) const;

    using Constants = std::unordered_map<std::string, sdword>;

    /// The values of `.equ`, by name (with the scope for those defined in one).
    Constants m_constants;

    /// Comma separated expressions.
    ///
    /// @return The values.
    std::vector<dword> parse_arguments();

    /// Shared by .byte, .dbyte, .word, ... Appends the arguments to .data, `n_bytes` each.
    ///
    /// @param[in] directive The name of the directive, for the errors.
    /// @param[in] n_bytes The size of each value.
    void define_data(const char *directive, U8 n_bytes);

    /// Shared by .float and .double. Appends the arguments, floating point numbers, to the
    /// current section.
    ///
    /// @param[in] directive The name of the directive, for the errors.
    /// @param[in] dbl Whether the numbers are doubles rather than floats.
    void define_float(const char *directive, bool dbl);

    /// Appends one value of `n_bytes` to the current section, little endian: a number that
    /// fits, or (for 4 bytes) the address of a symbol, which becomes a relocation.
    /// `first` is the token the value starts at, for errors.
    ///
    /// @param[in] directive The name of the directive, for the errors.
    /// @param[in] n_bytes The size of the value.
    /// @param[in] value The value.
    /// @param[in] first The first token of the value.
    void append_value(const char *directive, U8 n_bytes, const ExprValue &value,
                      const basm::Token &first);

    /// Parses the name of a system register (`vbar`, `elr`, ...).
    ///
    /// @return The number of the register.
    byte parse_sysreg();

    /// Parses a register: x0-x29, sp or xzr.
    ///
    /// @return The number of the register.
    byte parse_register();

    /// Parses a shift: `lsl`, `lsr`, `asr` or `ror` and the amount.
    ///
    /// @param[out] shift Receives the kind of shift.
    /// @param[out] shift_amt Receives the amount.
    void parse_shift(ShiftType &shift, S32 &shift_amt);

    /// Operations of the form `op xd, xn, <xm[, shift] | imm14 | :lo12:symbol>`.
    ///
    /// @param[in] opcode The opcode of the operation.
    /// @param[in] implicit_dest The destination is not written, it is xzr (cmp, cmn, tst, teq).
    /// @return The encoded instruction.
    word parse_format_o(byte opcode, bool implicit_dest = false);

    /// The shifts: `op xd, xn, <xm | imm5>`.
    ///
    /// @param[in] type The shift.
    /// @return The encoded instruction.
    word parse_format_o1(ShiftType type);

    /// The long multiplies: `op xlo, xhi, xn, xm`.
    ///
    /// @param[in] is_signed Whether the multiply is signed.
    /// @return The encoded instruction.
    word parse_format_o2(bool is_signed);

    /// The moves: `op xd, <xm | imm19>`.
    ///
    /// @param[in] opcode The opcode of the move.
    /// @return The encoded instruction.
    word parse_format_o3(byte opcode);

    /// The loads and stores: `op xt, [xn{, offset}]`, with the addressing modes of docs/isa.md.
    ///
    /// @param[in] opcode The opcode of the access.
    /// @param[in] unaligned Whether this is the unaligned form (`ldur`, `ldurh`, `stur`, `sturh`),
    ///        which has a plain offset only.
    /// @return The encoded instruction.
    word parse_format_m(byte opcode, bool unaligned = false);

    /// Warns when the writeback of a pre or post indexed access is useless or easy to misread
    /// (the base register is xzr, or is the register that is loaded or stored, or the offset is 0).
    ///
    /// @param[in] mode The addressing mode as it is written.
    /// @param[in] reg_t The register that is loaded or stored.
    /// @param[in] reg_a The base register.
    /// @param[in] is_load Whether the access is a load.
    /// @param[in] zero_offset Whether the offset is known to be 0.
    void warn_writeback(encoding::AddrType mode, byte reg_t, byte reg_a, bool is_load,
                        bool zero_offset);

    /// @param[in] mode The addressing mode as it is written.
    /// @param[in] unaligned Whether the access is `ldur`, `ldurh`, `stur` or `sturh`.
    /// @return The mode that is encoded: those take a plain offset, which is encoded as the
    ///         unaligned mode. Fails if the mode is pre or post indexed.
    encoding::AddrType encoded_mode(encoding::AddrType mode, bool unaligned);

    /// `op xd, symbol`: the page address instructions (`adrp`, `adr`).
    ///
    /// @param[in] opcode The opcode.
    /// @return The encoded instruction.
    word parse_format_m1(byte opcode);

    /// A branch to a label or a byte distance: `op[.cond] target`.
    ///
    /// @param[in] opcode The opcode of the branch.
    /// @return The encoded instruction.
    word parse_format_b1(byte opcode);

    /// A branch to a register: `op[.cond] xd`.
    ///
    /// @param[in] link Whether it saves the return address (`blr`).
    /// @return The encoded instruction.
    word parse_format_b2(bool link);

    /// swi[.cond] [number]: the number is an immediate, not a branch offset.
    ///
    /// @param[in] opcode The opcode.
    /// @return The encoded instruction.
    word parse_format_swi(byte opcode);

    /// csel xd, xn, xm, cond   (and csinc, csinv, csneg)
    ///
    /// @param[in] opcode The opcode.
    /// @param[in] variant Which of csel, csinc, csinv and csneg.
    /// @return The encoded instruction.
    word parse_format_csel(byte opcode, byte variant);
    /// cset xd, cond   (and csetm): xd = cond ? 1 : 0, or all ones.
    ///
    /// @param[in] opcode The opcode.
    /// @param[in] variant Which of cset and csetm.
    /// @return The encoded instruction.
    word parse_format_cset(byte opcode, byte variant);
    /// cinc xd, xn, cond   (and cinv, cneg): xd = cond ? f(xn) : xn.
    ///
    /// @param[in] opcode The opcode.
    /// @param[in] variant Which of cinc, cinv and cneg.
    /// @return The encoded instruction.
    word parse_format_cinc(byte opcode, byte variant);
    /// sxtb xd, xn   (and the other unary operations)
    ///
    /// @param[in] operation Which unary operation.
    /// @return The encoded instruction.
    word parse_format_unary(byte operation);
    /// fadd.f32 xd, xn, xm   (and the other binary floating point instructions)
    ///
    /// @param[in] fn Which operation.
    /// @param[in] dbl Whether it is on doubles rather than floats.
    /// @return The encoded instruction.
    word parse_format_v2(byte fn, bool dbl);
    /// fmadd.f32 xd, xn, xm, xa   (and fmsub, fnmadd, fnmsub)
    ///
    /// @param[in] fn Which operation.
    /// @param[in] dbl Whether it is on doubles rather than floats.
    /// @return The encoded instruction.
    word parse_format_v4(byte fn, bool dbl);
    /// fsqrt.f32 xd, xn   (and the other unary instructions and the conversions)
    ///
    /// @param[in] fn Which operation.
    /// @param[in] dbl Whether it is on doubles rather than floats.
    /// @return The encoded instruction.
    word parse_format_v1(byte fn, bool dbl);
    /// fcmp.f32 xn, xm   (and fcmpe)
    ///
    /// @param[in] signaling Whether it is fcmpe, which signals on a NaN.
    /// @param[in] dbl Whether it is on doubles rather than floats.
    /// @return The encoded instruction.
    word parse_format_v3(bool signaling, bool dbl);
    /// fmov.f32 xd, xm  /  fmov.f32 xd, 1.5: moves a float or a double (a register pair), or
    /// loads a constant with the instructions of `ldr xd, =value`.
    ///
    /// @param[in] dbl Whether it moves a double rather than a float.
    void assemble_fmov(bool dbl);
    /// Reads a register, and for a double checks that it can start a pair.
    ///
    /// @param[in] pair Whether the register is the first of a pair (x0-x28).
    /// @return The number of the register.
    U8 parse_fp_register(bool pair);
    /// Parses a float literal, an integer literal or either with a sign: `1.5`, `-2`.
    ///
    /// @param[in] dbl Whether to make a double rather than a float.
    /// @return The bit pattern, the float in the low 32 bits.
    U64 parse_float_constant(bool dbl);
    /// Emits the instructions that put a 32 bit value in a register, see assemble_load_constant.
    ///
    /// @param[in] xd The register.
    /// @param[in] value The value.
    void emit_load_constant(byte xd, word value);
    /// Parses a condition (`eq`, `ne`, ...).
    ///
    /// @return The condition.
    ConditionCode parse_condition();

    /// The condition of the aliases is the one under which the *first* register is chosen, the
    /// instruction stores the opposite one. al and nv have no opposite.
    ///
    /// @return The opposite of the condition that is written.
    ConditionCode parse_inverted_condition();

    /// `ldr xd, =constant` or `ldr xd, =symbol`. A pseudo instruction, there is no literal pool: a
    /// load cannot reach one (no pc relative addressing), and any constant is built in at most 3
    /// instructions without touching memory. A constant is built with the shortest sequence:
    ///   0 to 0x7FFFF:        mov  xd, value
    ///   -0x80000 to -1:      mvn  xd, ~value
    ///   anything else:       mov  xd, value >> 14 / lsl xd, xd, 14 / orr xd, xd, value & 0x3FFF
    /// (the `orr` is left out when those bits are 0). A symbol is its address, adrp + add :lo12:.
    ///
    /// @return `false`, having consumed nothing, if the statement is not of that form.
    bool assemble_load_constant();
    /// The atomics: `op xt, xn, xm`.
    ///
    /// @param[in] width The width of the access (kAtomicWidth_*).
    /// @param[in] atopcode The operation (kAtomicId_*).
    /// @return The encoded instruction.
    word parse_format_atomic(byte width, byte atopcode);

    /// Fills in what is known without linking in all the relocations of the file, see below. The
    /// relocations that are filled in are removed.
    void fill_local();

    /// A relocation names the label of the innermost scope that has it, if there is one, and a
    /// branch to a label of this file does not depend on where the file ends up, so it is filled in
    /// now. Absolute addresses (e.g. adrp to a .text label) are only known after linking, so those
    /// are left for the linker.
    ///
    /// @param[in,out] relocations The relocations of one section.
    /// @param[in] fill_branches Whether to fill in the branches, which only the code section has.
    void fill_local(std::vector<ObjectFile::RelocationEntry> &relocations, bool fill_branches);

    // Assembler directives.

    /// Declares a symbol to be global outside this compilation unit. It does not depend on the
    /// section, so it can be anywhere (a macro can declare one).
    /// USAGE:                .global <symbol>
    void _global();
    /// Declares a symbol to exist in another compilation unit but not defined here.
    /// Symbol's binding info will be marked as weak. Can be anywhere, like
    /// .global.
    /// USAGE:                .extern <symbol>
    void _extern();
    /// Declares a symbol weak. If this file defines it, another file may define it
    /// too and that definition is used instead (a file that defines it without
    /// `.weak` wins, and of several weak ones the first linked does). If nothing
    /// defines it, its value is 0 and it is not an error. Can be anywhere, like
    /// .global.
    /// USAGE:                .weak <symbol>
    void _weak();
    /// Reserves `size` zeroed bytes in .bss for a global symbol that other files
    /// may reserve too. It is a weak definition in .bss, so a file that defines the
    /// symbol for real wins, and when several files only reserve it they share the
    /// space of the first one linked. Every file should give the same size, the
    /// linker does not know it. Does not change the current section.
    /// USAGE:                .comm <symbol>, <size>{, <alignment>}
    void _comm();
    /// Read only data. The program cannot write it.
    /// USAGE:                  .rodata
    void _rodata();
    /// Words with the addresses of functions to call before `main` (`.word f`).
    /// The linker joins them and defines `__init_array_start` and
    /// `__init_array_end` around them. Read only.
    /// USAGE:                  .init_array
    void _init_array();
    /// Same as .init_array, for the functions to call after `main`
    /// (`__fini_array_start`, `__fini_array_end`).
    /// USAGE:                  .fini_array
    void _fini_array();
    /// Gives a number a name, to be used in the expressions that follow. It is a
    /// constant of the file (of the scope, if there is one), not a symbol of the
    /// object file. Can be anywhere, also in a macro.
    /// USAGE:               .equ <name>, <expression>
    void _equ();
    /// Moves where the assembler is in a section. Can only move forward, not backward.
    /// USAGE:                .org <expression>
    void _org();
    /// Defines a local scope. Any symbol defined inside will be marked as
    /// local and will not be able to be marked as global. Symbols defined
    /// here are renamed <symbol>::SCOPE:<scope_id>.
    /// Local symbols defined at current scope level or above will have
    /// higher precedence over globally defined symbols.
    /// USAGE:                  .scope
    void _scope();
    /// Ends a local scope.
    /// USAGE:                  .scend
    void _scend();
    /// Moves where the assembler is in a section forward by a certain amount of bytes.
    /// USAGE:                  .advance <expression>
    void _advance();
    /// Pads the current section with zeros up to a multiple of the alignment. The section keeps
    /// the largest alignment given, which the linker keeps when it puts the sections together.
    /// USAGE:                  .align <expression>
    void _align();
    /// Makes a section with a name of the program's choosing the current one,
    /// and creates it the first time. The flags are a string of the letters
    /// r (read), w (write) and x (execute): "r" (read only), "rw" or "rx". A
    /// section is not writable and executable at once. A section made without
    /// flags is "rw". The linker joins the sections of the same name of all
    /// the files, and the linker script places them by name.
    /// Instructions can be assembled in an executable section, data
    /// directives in any of them (a user section holds bytes).
    /// A third operand, "nobits", makes the section zero filled like .bss (flags
    /// "rw"): the file has its size and no bytes, and only .advance, .align,
    /// .org and labels are legal in it.
    /// The names of the sections the assembler has (".text", ".data", ".bss",
    /// ".rodata", ".init_array", ".fini_array") are those sections, so
    /// `.section ".data"` is `.data`.
    /// USAGE:                  .section <string>[, <flags>[, "nobits"]]
    void _section();
    /// Remembers the current section and switches to another one like
    /// .section does (the same operands), so that .popsection can go back.
    /// Meant for macros, which put something in .rodata or a section of their
    /// own and carry on in the section they were used in. They nest.
    /// USAGE:                  .pushsection <string>[, <flags>[, "nobits"]]
    void _pushsection();
    /// Goes back to the section that the last .pushsection left.
    /// USAGE:                  .popsection
    void _popsection();
    /// Switches to the text section. Using it again continues the same section.
    ///     USAGE:                  .text
    void _text();
    /// Switches to the data section. Using it again continues the same section.
    ///     USAGE:                  .data
    void _data();
    /// Switches to the bss section. Using it again continues the same section.
    ///     USAGE:                  .bss
    void _bss();
    /// Stops assembling.
    /// USAGE:                  .stop
    void _stop();
    /// Data directives: append the comma separated values to the current section, little endian.
    /// USAGE:                .byte <expr>{, <expr>}   (also .dbyte, .word and .dword)
    void _byte();
    void _dbyte();
    void _word();
    void _dword();
    /// Floating point data: `.float 1.5, -2` (4 bytes each) and `.double 0.1` (8 bytes).
    void _float();
    void _double();
    /// Instructions given as their 32 bit words, in a code section: what the disassembler writes
    /// for a word that is not an instruction.
    /// USAGE:                .inst <expr>{, <expr>}
    void _inst();
    /// Repeats a value: `count` copies of a `size` byte value (little endian).
    /// The size is 1, 2, 4 or 8 and defaults to 1, the value defaults to 0 and
    /// is a number that fits (for a size of 4 it can also be the address of a
    /// symbol, like .word). For zeros, .advance does the same.
    /// USAGE:                  .fill <count>{, <size>{, <value>}}
    void _fill();
    /// Appends the bytes of a string, `.asciz` also appends a terminating 0.
    /// USAGE:                .ascii "<text>"   (also .asciz)
    void _ascii();
    void _asciz();

    // Instructions.

    /// Assembles the instruction at the cursor and appends it to .text. How depends on its row in
    /// instruction_list.h.
    ///
    /// add x1, x2, x3
    /// add x1, x2, 40
    /// add x1, x2, x3, lsl 4
    /// add x1, x2, :lo12:symbol
    /// add x1, x2, :lo12:symbol + 4
    ///
    /// cmp, cmn, tst and teq are the ALU operations without a destination, so they are written
    /// as `cmp xn, <operand>` and encoded with xzr as the destination. `ret` is `bx x29`, x29
    /// being the link register.
    ///
    /// @param[in] spec The row of the instruction in the instruction list.
    void assemble_instruction(const basm::InstructionSpec &spec);

    // The instructions that have an operand syntax of their own. Each one assembles its
    // instruction, see docs/isa.md.
    void _hlt();
    void _nop();
    void _tlbi();
    void _eret();
    void _wfi();
    void _brk();
    void _msr();
    void _mrs();
    /// `ret` is `bx x29`, x29 being the link register.
    void _ret();

    using DirectiveFunction = void (Assembler::*)();
    /// Function pointers to process an assembler directive.
    std::unordered_map<basm::TokenType, DirectiveFunction> m_directive_handlers = {
        {basm::TokenType::ASSEMBLER_GLOBAL, &Assembler::_global},
        {basm::TokenType::ASSEMBLER_EXTERN, &Assembler::_extern},
        {basm::TokenType::ASSEMBLER_WEAK, &Assembler::_weak},
        {basm::TokenType::ASSEMBLER_COMM, &Assembler::_comm},
        {basm::TokenType::ASSEMBLER_RODATA, &Assembler::_rodata},
        {basm::TokenType::ASSEMBLER_INIT_ARRAY, &Assembler::_init_array},
        {basm::TokenType::ASSEMBLER_FINI_ARRAY, &Assembler::_fini_array},
        {basm::TokenType::ASSEMBLER_EQU, &Assembler::_equ},
        {basm::TokenType::ASSEMBLER_ORG, &Assembler::_org},
        {basm::TokenType::ASSEMBLER_SCOPE, &Assembler::_scope},
        {basm::TokenType::ASSEMBLER_SCEND, &Assembler::_scend},
        {basm::TokenType::ASSEMBLER_ADVANCE, &Assembler::_advance},
        {basm::TokenType::ASSEMBLER_ALIGN, &Assembler::_align},
        {basm::TokenType::ASSEMBLER_SECTION, &Assembler::_section},
        {basm::TokenType::ASSEMBLER_PUSHSECTION, &Assembler::_pushsection},
        {basm::TokenType::ASSEMBLER_POPSECTION, &Assembler::_popsection},
        {basm::TokenType::ASSEMBLER_FILL, &Assembler::_fill},
        {basm::TokenType::ASSEMBLER_TEXT, &Assembler::_text},
        {basm::TokenType::ASSEMBLER_DATA, &Assembler::_data},
        {basm::TokenType::ASSEMBLER_BSS, &Assembler::_bss},
        {basm::TokenType::ASSEMBLER_STOP, &Assembler::_stop},
        {basm::TokenType::ASSEMBLER_BYTE, &Assembler::_byte},
        {basm::TokenType::ASSEMBLER_DBYTE, &Assembler::_dbyte},
        {basm::TokenType::ASSEMBLER_WORD, &Assembler::_word},
        {basm::TokenType::ASSEMBLER_DWORD, &Assembler::_dword},
        {basm::TokenType::ASSEMBLER_FLOAT, &Assembler::_float},
        {basm::TokenType::ASSEMBLER_DOUBLE, &Assembler::_double},
        {basm::TokenType::ASSEMBLER_ASCII, &Assembler::_ascii},
        {basm::TokenType::ASSEMBLER_ASCIZ, &Assembler::_asciz},
        {basm::TokenType::ASSEMBLER_INST, &Assembler::_inst},
    };
};