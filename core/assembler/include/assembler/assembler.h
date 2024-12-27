#pragma once
#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include "assembler/object_file.h"
#include "assembler/build.h"
#include "assembler/tokenizer.h"
#include "util/file.h"
#include "emulator32bit/emulator32bit_util.h"

#include <string>
#include <unordered_map>

class Assembler
{
	public:
		enum State {
			NOT_ASSEMBLED, ASSEMBLING, ASSEMBLED, ASSEMBLER_ERROR, ASSEMBLER_WARNING,
		};

		Assembler(Process *process, File processed_file, const std::string& output_path = "");

		File assemble();
		State get_state();

	private:
		Process *m_process;										    /* the build process */

		File m_inputFile;										    /* the .bi file being assembled */
		File m_outputFile;										    /* the object file, a .bo file */
		State m_state;											    /* the state of the assembler */
		std::vector<Tokenizer::Token> m_tokens;					    /* the tokens of the input processed file */

		ObjectFile m_obj;

		enum class Section {
			NONE, DATA, BSS, TEXT
		} current_section = Section::NONE;							/* Which section is being assembled currently */
		int current_section_index = 0;								/* Index into section table */

		int total_scopes = 0;
		std::vector<int> scopes;									/* Nested scopes */

		dword parse_expression(size_t& tok_i);
		std::vector<dword> parse_arguments(size_t& tok_i);
		byte parse_register(size_t& tok_i);
		void parse_shift(size_t& tok_i, int& shift, int& shift_amt);

		word parse_format_o(size_t& tok_i, byte opcode);
		word parse_format_o1(size_t& tok_i, byte opcode);
		word parse_format_o2(size_t& tok_i, byte opcode);
		word parse_format_o3(size_t& tok_i, byte opcode);

		word parse_format_m(size_t& tok_i, byte opcode);
		word parse_format_m1(size_t& tok_i, byte opcode);
		word parse_format_m2(size_t& tok_i, byte opcode);

		word parse_format_b1(size_t& tok_i, byte opcode);
		word parse_format_b2(size_t& tok_i, byte opcode);

		void fill_local();

		// these are the same as the preprocessor helper methods.. see if we can use tokenizer instead to store these duplicate methods
		void skip_tokens(size_t& tok_i, const std::string& regex);
        void skip_tokens(size_t& tok_i, const std::set<Tokenizer::Type>& tokenTypes);
		bool expect_token(size_t tok_i, const std::string& errorMsg);
        bool expect_token(size_t tok_i, const std::set<Tokenizer::Type>& tokenTypes, const std::string& errorMsg);
        bool is_token(size_t tok_i, const std::set<Tokenizer::Type>& tokenTypes, const std::string& errorMsg = "assembler::is_token() - Unexpected end of file");
        bool in_bounds(size_t tok_i);
        Tokenizer::Token& consume(size_t& tok_i, const std::string& errorMsg = "assembler::consume() - Unexpected end of file");
        Tokenizer::Token& consume(size_t& tok_i, const std::set<Tokenizer::Type>& expectedTypes, const std::string& errorMsg = "assembler::consume() - Unexpected token");

		void _global(size_t& tok_i);
		void _extern(size_t& tok_i);
		void _org(size_t& tok_i);
		void _scope(size_t& tok_i);
		void _scend(size_t& tok_i);
		void _advance(size_t& tok_i);
		void _align(size_t& tok_i);
		void _section(size_t& tok_i);
		void _text(size_t& tok_i);
		void _data(size_t& tok_i);
		void _bss(size_t& tok_i);
		void _stop(size_t& tok_i);
		void _byte(size_t& tok_i);
		void _dbyte(size_t& tok_i);
		void _word(size_t& tok_i);
		void _dword(size_t& tok_i);
		void _sbyte(size_t& tok_i);
		void _sdbyte(size_t& tok_i);
		void _sword(size_t& tok_i);
		void _sdword(size_t& tok_i);
		void _char(size_t& tok_i);
		void _ascii(size_t& tok_i);
		void _asciz(size_t& tok_i);

		void _hlt(size_t& tok_i);
		void _add(size_t& tok_i);
		void _sub(size_t& tok_i);
		void _rsb(size_t& tok_i);
		void _adc(size_t& tok_i);
		void _sbc(size_t& tok_i);
		void _rsc(size_t& tok_i);
		void _mul(size_t& tok_i);
		void _umull(size_t& tok_i);
		void _smull(size_t& tok_i);
		void _vabs(size_t& tok_i);
		void _vneg(size_t& tok_i);
		void _vsqrt(size_t& tok_i);
		void _vadd(size_t& tok_i);
		void _vsub(size_t& tok_i);
		void _vdiv(size_t& tok_i);
		void _vmul(size_t& tok_i);
		void _vcmp(size_t& tok_i);
		void _vsel(size_t& tok_i);
		void _vcint(size_t& tok_i);
		void _vcflo(size_t& tok_i);
		void _vmov(size_t& tok_i);
		void _and(size_t& tok_i);
		void _orr(size_t& tok_i);
		void _eor(size_t& tok_i);
		void _bic(size_t& tok_i);
		void _lsl(size_t& tok_i);
		void _lsr(size_t& tok_i);
		void _asr(size_t& tok_i);
		void _ror(size_t& tok_i);
		void _cmp(size_t& tok_i);
		void _cmn(size_t& tok_i);
		void _tst(size_t& tok_i);
		void _teq(size_t& tok_i);
		void _mov(size_t& tok_i);
		void _mvn(size_t& tok_i);
		void _ldr(size_t& tok_i);
		void _str(size_t& tok_i);
		void _swp(size_t& tok_i);
		void _ldrb(size_t& tok_i);
		void _strb(size_t& tok_i);
		void _swpb(size_t& tok_i);
		void _ldrh(size_t& tok_i);
		void _strh(size_t& tok_i);
		void _swph(size_t& tok_i);
		void _b(size_t& tok_i);
		void _bl(size_t& tok_i);
		void _bx(size_t& tok_i);
		void _blx(size_t& tok_i);
		void _swi(size_t& tok_i);

		void _adrp(size_t& tok_i);

		void _ret(size_t& tok_i);

		typedef void (Assembler::*DirectiveFunction)(size_t& tok_i);
		std::unordered_map<Tokenizer::Type,DirectiveFunction> directives = {
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
		typedef void (Assembler::*InstructionFunction)(size_t& tok_i);
		std::unordered_map<Tokenizer::Type,InstructionFunction> instructions = {
			{Tokenizer::INSTRUCTION_HLT, & Assembler::_hlt},
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
			{Tokenizer::INSTRUCTION_SWP, &Assembler::_swp},
			{Tokenizer::INSTRUCTION_LDRB, &Assembler::_ldrb},
			{Tokenizer::INSTRUCTION_STRB, &Assembler::_strb},
			{Tokenizer::INSTRUCTION_SWPB, &Assembler::_swpb},
			{Tokenizer::INSTRUCTION_LDRH, &Assembler::_ldrh},
			{Tokenizer::INSTRUCTION_STRH, &Assembler::_strh},
			{Tokenizer::INSTRUCTION_SWPH, &Assembler::_swph},
			{Tokenizer::INSTRUCTION_B, &Assembler::_b},
			{Tokenizer::INSTRUCTION_BL, &Assembler::_bl},
			{Tokenizer::INSTRUCTION_BX, &Assembler::_bx},
			{Tokenizer::INSTRUCTION_BLX, &Assembler::_blx},
			{Tokenizer::INSTRUCTION_SWI, &Assembler::_swi},
			{Tokenizer::INSTRUCTION_ADRP, &Assembler::_adrp},
			{Tokenizer::INSTRUCTION_RET, &Assembler::_ret},
		};
};

#endif