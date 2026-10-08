#pragma once

#include "assembler/build.h"
#include "assembler/object_file.h"
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
    /// @brief          State of the Assembler.
    enum State
    {
        /// @brief      Assembler has not started.
        NOT_ASSEMBLED,

        /// @brief      Assembler has begun, but has not finished.
        ASSEMBLING,

        /// @brief      Assembler has finished successfully and produced an object file.
        ASSEMBLED,

        /// @brief      Assembler encountered a warning but produced a valid object file.
        ASSEMBLER_WARNING,

        /// @brief      Assembler encountered an error and did not produce a valid object file.
        ASSEMBLER_ERROR,
    };

    /// @brief          Initializes the assembler and creates the output object file.
    /// @param process          Build process.
    /// @param processed_file   Input file to process. Post preprocessor.
    /// @param output_path      Output file path to write the object file to. If left empty, creates
    ///                         the object file as the same path as the input file with the .bo
    ///                         extension.
    Assembler(const Process *process, const File processed_file,
              const std::string &output_path = "");

    /// @brief          Same, but takes the tokens straight from the preprocessor instead of
    ///                 lexing the .bi file again. Errors then point at the original source.
    /// @param process          Build process.
    /// @param processed_file   The .bi file the preprocessor wrote. Only used for its name.
    /// @param source           Result of Preprocessor::take_result().
    /// @param output_path      See above.
    Assembler(const Process *process, const File processed_file, basm::PreprocessedSource source,
              const std::string &output_path = "");

    /// @brief          Assembles the input assembly into an object file.
    void assemble();

    /// @brief          Get the output object file.
    /// @return         The file.
    File get_output_file() const;

    /// @brief          Get assembler state.
    /// @return         Assembler state.
    State get_state() const;

  private:
    /// @brief Build process container.
    const Process *const m_process;

    /// @brief Input .bi file that will be assembled.
    const File m_in_file;

    /// @brief Output .bo object file.
    File m_out_obj_file;

    /// @brief State of the assembler.
    State m_state;

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

    /// @brief Evaluates `number (op number)*` left to right, without precedence.
    ///        Fails if there is no number. Warns if the value is outside of [min, max].
    dword parse_expression(dword min = 0, dword max = -1);

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
    word parse_format_atomic(byte width, byte atopcode);
    void fill_local();

    ///
    /// Assembler directives.
    ///
    void _global();
    void _extern();
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

    void _hlt();
    void _nop();
    void _add();
    void _sub();
    void _rsb();
    void _adc();
    void _sbc();
    void _rsc();
    void _mul();
    void _umull();
    void _smull();
    void _vabs();
    void _vneg();
    void _vsqrt();
    void _vadd();
    void _vsub();
    void _vdiv();
    void _vmul();
    void _vcmp();
    void _vsel();
    void _vcint();
    void _vcflo();
    void _vmov();
    void _and();
    void _orr();
    void _eor();
    void _bic();
    void _lsl();
    void _lsr();
    void _asr();
    void _ror();
    void _cmp();
    void _cmn();
    void _tst();
    void _teq();
    void _mov();
    void _mvn();
    void _ldr();
    void _str();
    void _ldrb();
    void _strb();
    void _ldrh();
    void _strh();
    void _msr();
    void _mrs();
    void _tlbi();
    void _swp();
    void _swpb();
    void _swph();
    void _ldadd();
    void _ldaddb();
    void _ldaddh();
    void _ldclr();
    void _ldclrb();
    void _ldclrh();
    void _ldset();
    void _ldsetb();
    void _ldseth();
    void _b();
    void _bl();
    void _bx();
    void _blx();
    void _swi();
    void _adrp();
    void _ret();

    using DirectiveFunction = void (Assembler::*)();
    /// @brief Function pointers to process an assembler directive.
    std::unordered_map<basm::TokenType, DirectiveFunction> m_directive_handlers = {
        {basm::TokenType::ASSEMBLER_GLOBAL, &Assembler::_global},
        {basm::TokenType::ASSEMBLER_EXTERN, &Assembler::_extern},
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

    using InstructionFunction = void (Assembler::*)();
    /// @brief Function pointers assemble an instruction.
    std::unordered_map<basm::TokenType, InstructionFunction> m_instruction_handlers = {
        {basm::TokenType::INSTRUCTION_HLT, &Assembler::_hlt},
        {basm::TokenType::INSTRUCTION_NOP, &Assembler::_nop},
        {basm::TokenType::INSTRUCTION_ADD, &Assembler::_add},
        {basm::TokenType::INSTRUCTION_SUB, &Assembler::_sub},
        {basm::TokenType::INSTRUCTION_RSB, &Assembler::_rsb},
        {basm::TokenType::INSTRUCTION_ADC, &Assembler::_adc},
        {basm::TokenType::INSTRUCTION_SBC, &Assembler::_sbc},
        {basm::TokenType::INSTRUCTION_RSC, &Assembler::_rsc},
        {basm::TokenType::INSTRUCTION_MUL, &Assembler::_mul},
        {basm::TokenType::INSTRUCTION_UMULL, &Assembler::_umull},
        {basm::TokenType::INSTRUCTION_SMULL, &Assembler::_smull},
        {basm::TokenType::INSTRUCTION_VABS, &Assembler::_vabs},
        {basm::TokenType::INSTRUCTION_VNEG, &Assembler::_vneg},
        {basm::TokenType::INSTRUCTION_VSQRT, &Assembler::_vsqrt},
        {basm::TokenType::INSTRUCTION_VADD, &Assembler::_vadd},
        {basm::TokenType::INSTRUCTION_VSUB, &Assembler::_vsub},
        {basm::TokenType::INSTRUCTION_VDIV, &Assembler::_vdiv},
        {basm::TokenType::INSTRUCTION_VMUL, &Assembler::_vmul},
        {basm::TokenType::INSTRUCTION_VCMP, &Assembler::_vcmp},
        {basm::TokenType::INSTRUCTION_VSEL, &Assembler::_vsel},
        {basm::TokenType::INSTRUCTION_VCINT, &Assembler::_vcint},
        {basm::TokenType::INSTRUCTION_VCFLO, &Assembler::_vcflo},
        {basm::TokenType::INSTRUCTION_VMOV, &Assembler::_vmov},
        {basm::TokenType::INSTRUCTION_AND, &Assembler::_and},
        {basm::TokenType::INSTRUCTION_ORR, &Assembler::_orr},
        {basm::TokenType::INSTRUCTION_EOR, &Assembler::_eor},
        {basm::TokenType::INSTRUCTION_BIC, &Assembler::_bic},
        {basm::TokenType::INSTRUCTION_LSL, &Assembler::_lsl},
        {basm::TokenType::INSTRUCTION_LSR, &Assembler::_lsr},
        {basm::TokenType::INSTRUCTION_ASR, &Assembler::_asr},
        {basm::TokenType::INSTRUCTION_ROR, &Assembler::_ror},
        {basm::TokenType::INSTRUCTION_CMP, &Assembler::_cmp},
        {basm::TokenType::INSTRUCTION_CMN, &Assembler::_cmn},
        {basm::TokenType::INSTRUCTION_TST, &Assembler::_tst},
        {basm::TokenType::INSTRUCTION_TEQ, &Assembler::_teq},
        {basm::TokenType::INSTRUCTION_MOV, &Assembler::_mov},
        {basm::TokenType::INSTRUCTION_MVN, &Assembler::_mvn},
        {basm::TokenType::INSTRUCTION_LDR, &Assembler::_ldr},
        {basm::TokenType::INSTRUCTION_STR, &Assembler::_str},
        {basm::TokenType::INSTRUCTION_LDRB, &Assembler::_ldrb},
        {basm::TokenType::INSTRUCTION_STRB, &Assembler::_strb},
        {basm::TokenType::INSTRUCTION_LDRH, &Assembler::_ldrh},
        {basm::TokenType::INSTRUCTION_STRH, &Assembler::_strh},

        {basm::TokenType::INSTRUCTION_MSR, &Assembler::_msr},
        {basm::TokenType::INSTRUCTION_MRS, &Assembler::_mrs},
        {basm::TokenType::INSTRUCTION_TLBI, &Assembler::_tlbi},

        {basm::TokenType::INSTRUCTION_SWP, &Assembler::_swp},
        {basm::TokenType::INSTRUCTION_SWPB, &Assembler::_swpb},
        {basm::TokenType::INSTRUCTION_SWPH, &Assembler::_swph},

        {basm::TokenType::INSTRUCTION_LDADD, &Assembler::_ldadd},
        {basm::TokenType::INSTRUCTION_LDADDB, &Assembler::_ldaddb},
        {basm::TokenType::INSTRUCTION_LDADDH, &Assembler::_ldaddh},

        {basm::TokenType::INSTRUCTION_LDCLR, &Assembler::_ldclr},
        {basm::TokenType::INSTRUCTION_LDCLRB, &Assembler::_ldclrb},
        {basm::TokenType::INSTRUCTION_LDCLRH, &Assembler::_ldclrh},

        {basm::TokenType::INSTRUCTION_LDSET, &Assembler::_ldset},
        {basm::TokenType::INSTRUCTION_LDSETB, &Assembler::_ldsetb},
        {basm::TokenType::INSTRUCTION_LDSETH, &Assembler::_ldseth},

        {basm::TokenType::INSTRUCTION_B, &Assembler::_b},
        {basm::TokenType::INSTRUCTION_BL, &Assembler::_bl},
        {basm::TokenType::INSTRUCTION_BX, &Assembler::_bx},
        {basm::TokenType::INSTRUCTION_BLX, &Assembler::_blx},
        {basm::TokenType::INSTRUCTION_SWI, &Assembler::_swi},
        {basm::TokenType::INSTRUCTION_ADRP, &Assembler::_adrp},
        {basm::TokenType::INSTRUCTION_RET, &Assembler::_ret},
    };
};