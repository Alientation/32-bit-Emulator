#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include "assembler/Build.h"
#include "assembler/Tokenizer.h"
#include "util/File.h"
#include "emulator32bit/Emulator32bitUtil.h"

#include <string>
#include <unordered_map>

class Assembler {
	public:
		enum State {
			NOT_ASSEMBLED, ASSEMBLING, ASSEMBLED, ASSEMBLER_ERROR, ASSEMBLER_WARNING,
		};

		Assembler(Process *process, File *processed_file, std::string output_path = "");
		~Assembler();

		void assemble();
		State get_state();

	private:
		Process *m_process;										    /* the build process */

		File *m_inputFile;										    /* the .bi file being assembled */
		File *m_outputFile;										    /* the object file, a .bo file */
		State m_state;											    /* the state of the assembler */
		std::vector<Tokenizer::Token> m_tokens;					    /* the tokens of the input processed file */

		FileWriter *m_writer;									    /* writer for the output file */

		/**
		 * @brief 					Symbols defined in this unit
		 *
		 */
		struct SymbolTableEntry {
			int symbol_name;										/* index into string table */
			word symbol_value;										/* value of symbol */
			enum class BindingInfo {
				LOCAL=0, GLOBAL=1, WEAK=2
			} binding_info;											/* type of symbol */
			int section;											/* index into section table, -1 indicates no section */
		};

		struct SectionHeader {
			int section_name;										/* index into string table */
			enum class Type {
				UNDEFINED, TEXT, DATA, BSS, SYMTAB, REL_TEXT, REL_DATA, REL_BSS, DEBUG, STRTAB,
			} type;													/* type of section */
			word section_size;										/* size of section in bytes */
			word entry_size;										/* size of entry in section */
		};

		struct RelocationEntry {
			word offset;											/* offset from beginning of section to the symbol */
			int symbol;												/* index into symbol table */
			enum class Type {
				UNDEFINED,
				R_EMU32_O_LO12, R_EMU32_ADRP_HI20,					/* Format O instructions and ADRP */
				R_EMU32_MOV_LO19, R_EMU32_MOV_HI13,					/* MOV/MVN instructions */
				R_EMU32_B_OFFSET22,									/* Branch offset, +/- 24 bit value (last 2 bits are 0) */
			} type;													/* type of relocation */
			word shift;												/* constant to be added to the value of the symbol */
		};

		std::vector<std::string> strings;							/* stores all strings */
		std::unordered_map<std::string, int> string_table;			/* maps strings to index in the table*/
		std::unordered_map<int, SymbolTableEntry> symbol_table;		/* maps string index to symbol */
		std::vector<RelocationEntry> rel_text;						/* references to symbols that need to be relocated */
		std::vector<RelocationEntry> rel_data;						/* For now, no purpose */
		std::vector<RelocationEntry> rel_bss;						/* For now, no purpose */
		std::vector<SectionHeader> section_table;					/* section headers */

		std::vector<word> text_section;								/* instructions stored in .text section */
		std::vector<byte> data_section;								/* data stored in .data section */
		word bss_section;											/* size of .bss section */

		enum class Section {
			NONE, DATA, BSS, TEXT
		} current_section = Section::NONE;							/* Which section is being assembled currently */
		int current_section_index = 0;								/* Index into section table */

		int total_scopes = 0;
		std::vector<int> scopes;									/* Nested scopes */

		void add_symbol(std::string symbol, word value, SymbolTableEntry::BindingInfo binding_info, int section);
		word parse_expression(int& tokenI);
		byte parse_register(int& tokenI);
		void parse_shift(int& tokenI, int& shift, int& shift_amt);

		word parse_format_o(int& tokenI, byte opcode);
		word parse_format_o1(int& tokenI, byte opcode);
		word parse_format_o2(int& tokenI, byte opcode);
		word parse_format_o3(int& tokenI, byte opcode);

		word parse_format_m(int& tokenI, byte opcode);
		word parse_format_m1(int& tokenI, byte opcode);
		word parse_format_m2(int& tokenI, byte opcode);

		word parse_format_b1(int& tokenI, byte opcode);
		word parse_format_b2(int& tokenI, byte opcode);

		void fill_local();

		// these are the same as the preprocessor helper methods.. see if we can use tokenizer instead to store these duplicate methods
		void skipTokens(int& tokenI, const std::string& regex);
        void skipTokens(int& tokenI, const std::set<Tokenizer::Type>& tokenTypes);
		bool expectToken(int tokenI, const std::string& errorMsg);
        bool expectToken(int tokenI, const std::set<Tokenizer::Type>& tokenTypes, const std::string& errorMsg);
        bool isToken(int tokenI, const std::set<Tokenizer::Type>& tokenTypes, const std::string& errorMsg = "Assembler::isToken() - Unexpected end of file");
        bool inBounds(int tokenI);
        Tokenizer::Token& consume(int& tokenI, const std::string& errorMsg = "Assembler::assemble() - Unexpected end of file");
        Tokenizer::Token& consume(int& tokenI, const std::set<Tokenizer::Type>& expectedTypes, const std::string& errorMsg = "Assembler::consume() - Unexpected token");

		void _global(int& tokenI);
		void _extern(int& tokenI);
		void _org(int& tokenI);
		void _scope(int& tokenI);
		void _scend(int& tokenI);
		void _advance(int& tokenI);
		void _align(int& tokenI);
		void _section(int& tokenI);
		void _text(int& tokenI);
		void _data(int& tokenI);
		void _bss(int& tokenI);
		void _stop(int& tokenI);

		void _hlt(int& tokenI);
		void _add(int& tokenI);
		void _sub(int& tokenI);
		void _rsb(int& tokenI);
		void _adc(int& tokenI);
		void _sbc(int& tokenI);
		void _rsc(int& tokenI);
		void _mul(int& tokenI);
		void _umull(int& tokenI);
		void _smull(int& tokenI);
		void _vabs_f32(int& tokenI);
		void _vneg_f32(int& tokenI);
		void _vsqrt_f32(int& tokenI);
		void _vadd_f32(int& tokenI);
		void _vsub_f32(int& tokenI);
		void _vdiv_f32(int& tokenI);
		void _vmul_f32(int& tokenI);
		void _vcmp_f32(int& tokenI);
		void _vsel_f32(int& tokenI);
		void _vcint_u32_f32(int& tokenI);
		void _vcint_s32_f32(int& tokenI);
		void _vcflo_u32_f32(int& tokenI);
		void _vcflo_s32_f32(int& tokenI);
		void _vmov_f32(int& tokenI);
		void _and(int& tokenI);
		void _orr(int& tokenI);
		void _eor(int& tokenI);
		void _bic(int& tokenI);
		void _lsl(int& tokenI);
		void _lsr(int& tokenI);
		void _asr(int& tokenI);
		void _ror(int& tokenI);
		void _cmp(int& tokenI);
		void _cmn(int& tokenI);
		void _tst(int& tokenI);
		void _teq(int& tokenI);
		void _mov(int& tokenI);
		void _mvn(int& tokenI);
		void _ldr(int& tokenI);
		void _str(int& tokenI);
		void _swp(int& tokenI);
		void _ldrb(int& tokenI);
		void _strb(int& tokenI);
		void _swpb(int& tokenI);
		void _ldrh(int& tokenI);
		void _strh(int& tokenI);
		void _swph(int& tokenI);
		void _b(int& tokenI);
		void _bl(int& tokenI);
		void _bx(int& tokenI);
		void _blx(int& tokenI);
		void _swi(int& tokenI);

		void _adrp(int& tokenI);

		void _ret(int& tokenI);

		typedef void (Assembler::*DirectiveFunction)(int& tokenI);
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
		};
		typedef void (Assembler::*InstructionFunction)(int& tokenI);
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
			{Tokenizer::INSTRUCTION_VABS_F32, &Assembler::_vabs_f32},
			{Tokenizer::INSTRUCTION_VNEG_F32, &Assembler::_vneg_f32},
			{Tokenizer::INSTRUCTION_VSQRT_F32, &Assembler::_vsqrt_f32},
			{Tokenizer::INSTRUCTION_VADD_F32, &Assembler::_vadd_f32},
			{Tokenizer::INSTRUCTION_VSUB_F32, &Assembler::_vsub_f32},
			{Tokenizer::INSTRUCTION_VDIV_F32, &Assembler::_vdiv_f32},
			{Tokenizer::INSTRUCTION_VMUL_F32, &Assembler::_vmul_f32},
			{Tokenizer::INSTRUCTION_VCMP_F32, &Assembler::_vcmp_f32},
			{Tokenizer::INSTRUCTION_VSEL_F32, &Assembler::_vsel_f32},
			{Tokenizer::INSTRUCTION_VCINT_U32_F32, &Assembler::_vcint_u32_f32},
			{Tokenizer::INSTRUCTION_VCINT_S32_F32, &Assembler::_vcint_s32_f32},
			{Tokenizer::INSTRUCTION_VCFLO_U32_F32, &Assembler::_vcflo_u32_f32},
			{Tokenizer::INSTRUCTION_VCFLO_S32_F32, &Assembler::_vcflo_s32_f32},
			{Tokenizer::INSTRUCTION_VMOV_F32, &Assembler::_vmov_f32},
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