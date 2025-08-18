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
    Assembler (const Process *process, const File processed_file,
               const std::string &output_path = "");

    /// @brief          Assembles the input assembly into an object file.
    void assemble ();

    /// @brief          Get the output object file.
    /// @return         The file.
    File get_output_file () const;

    /// @brief          Get assembler state.
    /// @return         Assembler state.
    State get_state () const;

  private:
    /// @brief Build process container.
    const Process *const m_process;

    /// @brief Input .bi file that will be assembled.
    const File m_in_file;

    /// @brief Output .bo object file.
    File m_out_obj_file;

    /// @brief State of the assembler.
    State m_state;

    /// @brief Tokenizer.
    Tokenizer m_tokenizer;

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
    U32 m_cur_section_index = U32 (-1);

    /// @brief Total number of declared scopes. Monotically increasing.
    U32 m_total_scopes = 0;

    /// @brief Nested scope id.
    std::vector<U32> m_scopes;

    /// @brief TODO:
    /// @param tok_i
    /// @param min
    /// @param max
    /// @return
    dword parse_expression (dword min = 0, dword max = -1);

    /// @brief TODO:
    /// @param tok_i
    /// @return
    std::vector<dword> parse_arguments ();

    /// @brief TODO:
    /// @param tok_i
    /// @return
    byte parse_sysreg ();

    /// @brief TODO:
    /// @param tok_i
    /// @return
    byte parse_register ();

    /// @brief TODO:
    /// @param tok_i
    /// @param shift
    /// @param shift_amt
    void parse_shift (Emulator32bit::ShiftType &shift, int &shift_amt);

    /// @brief TODO:
    /// @param tok_i
    /// @param opcode
    /// @return
    word parse_format_o (byte opcode);

    /// @brief TODO:
    /// @param tok_i
    /// @param opcode
    /// @return
    word parse_format_o1 (byte opcode);

    /// @brief TODO:
    /// @param tok_i
    /// @param opcode
    /// @return
    word parse_format_o2 (byte opcode);

    /// @brief TODO:
    /// @param tok_i
    /// @param opcode
    /// @return
    word parse_format_o3 (byte opcode);

    /// @brief TODO:
    /// @param tok_i
    /// @param opcode
    /// @return
    word parse_format_m (byte opcode);

    /// @brief TODO:
    /// @param tok_i
    /// @param opcode
    /// @return
    word parse_format_m1 (byte opcode);

    /// @brief TODO:
    /// @param tok_i
    /// @param opcode
    /// @return
    word parse_format_b1 (byte opcode);

    /// @brief TODO:
    /// @param tok_i
    /// @param opcode
    /// @return
    word parse_format_b2 (byte opcode);

    /// @brief TODO:
    /// @param tok_i
    /// @param width
    /// @param atopcode
    /// @return
    word parse_format_atomic (byte width, byte atopcode);

    /// @brief TODO:
    void fill_local ();

    ///
    /// Assembler directives.
    ///

    /// @brief TODO:
    /// @param tok_i
    void _global ();

    /// @brief TODO:
    /// @param tok_i
    void _extern ();

    /// @brief TODO:
    /// @param tok_i
    void _org ();

    /// @brief TODO:
    /// @param tok_i
    void _scope ();

    /// @brief TODO:
    /// @param tok_i
    void _scend ();

    /// @brief TODO:
    /// @param tok_i
    void _advance ();

    /// @brief TODO:
    /// @param tok_i
    void _align ();

    /// @brief TODO:
    /// @param tok_i
    void _section ();

    /// @brief TODO:
    /// @param tok_i
    void _text ();

    /// @brief TODO:
    /// @param tok_i
    void _data ();

    /// @brief TODO:
    /// @param tok_i
    void _bss ();

    /// @brief TODO:
    /// @param tok_i
    void _stop ();

    /// @brief TODO:
    /// @param tok_i
    void _byte ();

    /// @brief TODO:
    /// @param tok_i
    void _dbyte ();

    /// @brief TODO:
    /// @param tok_i
    void _word ();

    /// @brief TODO:
    /// @param tok_i
    void _dword ();

    /// @brief TODO:
    /// @param tok_i
    void _sbyte ();

    /// @brief TODO:
    /// @param tok_i
    void _sdbyte ();

    /// @brief TODO:
    /// @param tok_i
    void _sword ();

    /// @brief TODO:
    /// @param tok_i
    void _sdword ();

    /// @brief TODO:
    /// @param tok_i
    void _char ();

    /// @brief TODO:
    /// @param tok_i
    void _ascii ();

    /// @brief TODO:
    /// @param tok_i
    void _asciz ();

    ///
    /// Instructions.
    ///

    /// @brief TODO:
    /// @param tok_i
    void _hlt ();

    /// @brief TODO:
    /// @param tok_i
    void _nop ();

    /// @brief TODO:
    /// @param tok_i
    void _add ();

    /// @brief TODO:
    /// @param tok_i
    void _sub ();

    /// @brief TODO:
    /// @param tok_i
    void _rsb ();

    /// @brief TODO:
    /// @param tok_i
    void _adc ();

    /// @brief TODO:
    /// @param tok_i
    void _sbc ();

    /// @brief TODO:
    /// @param tok_i
    void _rsc ();

    /// @brief TODO:
    /// @param tok_i
    void _mul ();

    /// @brief TODO:
    /// @param tok_i
    void _umull ();

    /// @brief TODO:
    /// @param tok_i
    void _smull ();

    /// @brief TODO:
    /// @param tok_i
    void _vabs ();

    /// @brief TODO:
    /// @param tok_i
    void _vneg ();

    /// @brief TODO:
    /// @param tok_i
    void _vsqrt ();

    /// @brief TODO:
    /// @param tok_i
    void _vadd ();

    /// @brief TODO:
    /// @param tok_i
    void _vsub ();

    /// @brief TODO:
    /// @param tok_i
    void _vdiv ();

    /// @brief TODO:
    /// @param tok_i
    void _vmul ();

    /// @brief TODO:
    /// @param tok_i
    void _vcmp ();

    /// @brief TODO:
    /// @param tok_i
    void _vsel ();

    /// @brief TODO:
    /// @param tok_i
    void _vcint ();

    /// @brief TODO:
    /// @param tok_i
    void _vcflo ();

    /// @brief TODO:
    /// @param tok_i
    void _vmov ();

    /// @brief TODO:
    /// @param tok_i
    void _and ();

    /// @brief TODO:
    /// @param tok_i
    void _orr ();

    /// @brief TODO:
    /// @param tok_i
    void _eor ();

    /// @brief TODO:
    /// @param tok_i
    void _bic ();

    /// @brief TODO:
    /// @param tok_i
    void _lsl ();

    /// @brief TODO:
    /// @param tok_i
    void _lsr ();

    /// @brief TODO:
    /// @param tok_i
    void _asr ();

    /// @brief TODO:
    /// @param tok_i
    void _ror ();

    /// @brief TODO:
    /// @param tok_i
    void _cmp ();

    /// @brief TODO:
    /// @param tok_i
    void _cmn ();

    /// @brief TODO:
    /// @param tok_i
    void _tst ();

    /// @brief TODO:
    /// @param tok_i
    void _teq ();

    /// @brief TODO:
    /// @param tok_i
    void _mov ();

    /// @brief TODO:
    /// @param tok_i
    void _mvn ();

    /// @brief TODO:
    /// @param tok_i
    void _ldr ();

    /// @brief TODO:
    /// @param tok_i
    void _str ();

    /// @brief TODO:
    /// @param tok_i
    void _ldrb ();

    /// @brief TODO:
    /// @param tok_i
    void _strb ();

    /// @brief TODO:
    /// @param tok_i
    void _ldrh ();

    /// @brief TODO:
    /// @param tok_i
    void _strh ();

    /// @brief TODO:
    /// @param tok_i
    void _msr ();

    /// @brief TODO:
    /// @param tok_i
    void _mrs ();

    /// @brief TODO:
    /// @param tok_i
    void _tlbi ();

    /// @brief TODO:
    /// @param tok_i
    void _swp ();

    /// @brief TODO:
    /// @param tok_i
    void _swpb ();

    /// @brief TODO:
    /// @param tok_i
    void _swph ();

    /// @brief TODO:
    /// @param tok_i
    void _ldadd ();

    /// @brief TODO:
    /// @param tok_i
    void _ldaddb ();

    /// @brief TODO:
    /// @param tok_i
    void _ldaddh ();

    /// @brief TODO:
    /// @param tok_i
    void _ldclr ();

    /// @brief TODO:
    /// @param tok_i
    void _ldclrb ();

    /// @brief TODO:
    /// @param tok_i
    void _ldclrh ();

    /// @brief TODO:
    /// @param tok_i
    void _ldset ();

    /// @brief TODO:
    /// @param tok_i
    void _ldsetb ();

    /// @brief TODO:
    /// @param tok_i
    void _ldseth ();

    /// @brief TODO:
    /// @param tok_i
    void _b ();

    /// @brief TODO:
    /// @param tok_i
    void _bl ();

    /// @brief TODO:
    /// @param tok_i
    void _bx ();

    /// @brief TODO:
    /// @param tok_i
    void _blx ();

    /// @brief TODO:
    /// @param tok_i
    void _swi ();

    /// @brief TODO:
    /// @param tok_i
    void _adrp ();

    /// @brief TODO:
    /// @param tok_i
    void _ret ();

    using DirectiveFunction = void (Assembler::*) ();
    /// @brief Function pointers to process an assembler directive.
    std::unordered_map<Tokenizer::Type, DirectiveFunction> m_directive_handlers = {
        {Tokenizer::ASSEMBLER_GLOBAL, &Assembler::_global},
        {Tokenizer::ASSEMBLER_EXTERN, &Assembler::_extern},
        {Tokenizer::ASSEMBLER_ORG, &Assembler::_org},
        {Tokenizer::ASSEMBLER_SCOPE, &Assembler::_scope},
        {Tokenizer::ASSEMBLER_SCEND, &Assembler::_scend},
        {Tokenizer::ASSEMBLER_ADVANCE, &Assembler::_advance},
        {Tokenizer::ASSEMBLER_ALIGN, &Assembler::_align},
        {Tokenizer::ASSEMBLER_SECTION, &Assembler::_section},
        {Tokenizer::ASSEMBLER_TEXT, &Assembler::_text},
        {Tokenizer::ASSEMBLER_DATA, &Assembler::_data},
        {Tokenizer::ASSEMBLER_BSS, &Assembler::_bss},
        {Tokenizer::ASSEMBLER_STOP, &Assembler::_stop},
        {Tokenizer::ASSEMBLER_BYTE, &Assembler::_byte},
        {Tokenizer::ASSEMBLER_DBYTE, &Assembler::_dbyte},
        {Tokenizer::ASSEMBLER_WORD, &Assembler::_word},
        {Tokenizer::ASSEMBLER_DWORD, &Assembler::_dword},
        {Tokenizer::ASSEMBLER_SBYTE, &Assembler::_sbyte},
        {Tokenizer::ASSEMBLER_SDBYTE, &Assembler::_sdbyte},
        {Tokenizer::ASSEMBLER_SWORD, &Assembler::_sword},
        {Tokenizer::ASSEMBLER_SDWORD, &Assembler::_sdword},
        {Tokenizer::ASSEMBLER_CHAR, &Assembler::_char},
        {Tokenizer::ASSEMBLER_ASCII, &Assembler::_ascii},
        {Tokenizer::ASSEMBLER_ASCIZ, &Assembler::_asciz},
    };

    using InstructionFunction = void (Assembler::*) ();
    /// @brief Function pointers assemble an instruction.
    std::unordered_map<Tokenizer::Type, InstructionFunction> m_instruction_handlers = {
        {Tokenizer::INSTRUCTION_HLT, &Assembler::_hlt},
        {Tokenizer::INSTRUCTION_NOP, &Assembler::_nop},
        {Tokenizer::INSTRUCTION_ADD, &Assembler::_add},
        {Tokenizer::INSTRUCTION_SUB, &Assembler::_sub},
        {Tokenizer::INSTRUCTION_RSB, &Assembler::_rsb},
        {Tokenizer::INSTRUCTION_ADC, &Assembler::_adc},
        {Tokenizer::INSTRUCTION_SBC, &Assembler::_sbc},
        {Tokenizer::INSTRUCTION_RSC, &Assembler::_rsc},
        {Tokenizer::INSTRUCTION_MUL, &Assembler::_mul},
        {Tokenizer::INSTRUCTION_UMULL, &Assembler::_umull},
        {Tokenizer::INSTRUCTION_SMULL, &Assembler::_smull},
        {Tokenizer::INSTRUCTION_VABS, &Assembler::_vabs},
        {Tokenizer::INSTRUCTION_VNEG, &Assembler::_vneg},
        {Tokenizer::INSTRUCTION_VSQRT, &Assembler::_vsqrt},
        {Tokenizer::INSTRUCTION_VADD, &Assembler::_vadd},
        {Tokenizer::INSTRUCTION_VSUB, &Assembler::_vsub},
        {Tokenizer::INSTRUCTION_VDIV, &Assembler::_vdiv},
        {Tokenizer::INSTRUCTION_VMUL, &Assembler::_vmul},
        {Tokenizer::INSTRUCTION_VCMP, &Assembler::_vcmp},
        {Tokenizer::INSTRUCTION_VSEL, &Assembler::_vsel},
        {Tokenizer::INSTRUCTION_VCINT, &Assembler::_vcint},
        {Tokenizer::INSTRUCTION_VCFLO, &Assembler::_vcflo},
        {Tokenizer::INSTRUCTION_VMOV, &Assembler::_vmov},
        {Tokenizer::INSTRUCTION_AND, &Assembler::_and},
        {Tokenizer::INSTRUCTION_ORR, &Assembler::_orr},
        {Tokenizer::INSTRUCTION_EOR, &Assembler::_eor},
        {Tokenizer::INSTRUCTION_BIC, &Assembler::_bic},
        {Tokenizer::INSTRUCTION_LSL, &Assembler::_lsl},
        {Tokenizer::INSTRUCTION_LSR, &Assembler::_lsr},
        {Tokenizer::INSTRUCTION_ASR, &Assembler::_asr},
        {Tokenizer::INSTRUCTION_ROR, &Assembler::_ror},
        {Tokenizer::INSTRUCTION_CMP, &Assembler::_cmp},
        {Tokenizer::INSTRUCTION_CMN, &Assembler::_cmn},
        {Tokenizer::INSTRUCTION_TST, &Assembler::_tst},
        {Tokenizer::INSTRUCTION_TEQ, &Assembler::_teq},
        {Tokenizer::INSTRUCTION_MOV, &Assembler::_mov},
        {Tokenizer::INSTRUCTION_MVN, &Assembler::_mvn},
        {Tokenizer::INSTRUCTION_LDR, &Assembler::_ldr},
        {Tokenizer::INSTRUCTION_STR, &Assembler::_str},
        {Tokenizer::INSTRUCTION_LDRB, &Assembler::_ldrb},
        {Tokenizer::INSTRUCTION_STRB, &Assembler::_strb},
        {Tokenizer::INSTRUCTION_LDRH, &Assembler::_ldrh},
        {Tokenizer::INSTRUCTION_STRH, &Assembler::_strh},

        {Tokenizer::INSTRUCTION_MSR, &Assembler::_msr},
        {Tokenizer::INSTRUCTION_MRS, &Assembler::_mrs},
        {Tokenizer::INSTRUCTION_TLBI, &Assembler::_tlbi},

        {Tokenizer::INSTRUCTION_SWP, &Assembler::_swp},
        {Tokenizer::INSTRUCTION_SWPB, &Assembler::_swpb},
        {Tokenizer::INSTRUCTION_SWPH, &Assembler::_swph},

        {Tokenizer::INSTRUCTION_LDADD, &Assembler::_ldadd},
        {Tokenizer::INSTRUCTION_LDADDB, &Assembler::_ldaddb},
        {Tokenizer::INSTRUCTION_LDADDH, &Assembler::_ldaddh},

        {Tokenizer::INSTRUCTION_LDCLR, &Assembler::_ldclr},
        {Tokenizer::INSTRUCTION_LDCLRB, &Assembler::_ldclrb},
        {Tokenizer::INSTRUCTION_LDCLRH, &Assembler::_ldclrh},

        {Tokenizer::INSTRUCTION_LDSET, &Assembler::_ldset},
        {Tokenizer::INSTRUCTION_LDSETB, &Assembler::_ldsetb},
        {Tokenizer::INSTRUCTION_LDSETH, &Assembler::_ldseth},

        {Tokenizer::INSTRUCTION_B, &Assembler::_b},
        {Tokenizer::INSTRUCTION_BL, &Assembler::_bl},
        {Tokenizer::INSTRUCTION_BX, &Assembler::_bx},
        {Tokenizer::INSTRUCTION_BLX, &Assembler::_blx},
        {Tokenizer::INSTRUCTION_SWI, &Assembler::_swi},
        {Tokenizer::INSTRUCTION_ADRP, &Assembler::_adrp},
        {Tokenizer::INSTRUCTION_RET, &Assembler::_ret},
    };
};