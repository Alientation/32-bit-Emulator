#pragma once
#ifndef Emulator32bit_H
#define Emulator32bit_H

#include <emulator32bit/Emulator32bitUtil.h>
#include <emulator32bit/Memory.h>
#include <emulator32bit/SystemBus.h>

/**
 * @brief					IDs for special registers
 *
 */
#define SP 30				/* Stack Pointer*/
#define XZR 31				/* Zero Register */

/**
 * @brief 					Flag bit locations in _pstate register
 *
 */
#define N_FLAG 0			/* Negative Flag */
#define Z_FLAG 1			/* Zero Flag */
#define C_FLAG 2			/* Carry Flag */
#define V_FLAG 3			/* Overflow Flag */

/**
 * @brief					Which bit of the instruction determines whether flags will be updated
 *
 */
#define S_BIT 25			/* Update Flag Bit */

/**
 * @brief					Shift codes
 *
 */
#define LSL 0				/* Logical Shift Left */
#define LSR 1				/* Logical Shift Right */
#define ASR 2				/* Arithmetic Shift Right */
#define ROR 3				/* Rotate Right */

/**
 * @brief 					State of Emulator
 *
 */
class Emulator32bit {
	public:
		Emulator32bit();
		Emulator32bit(word ram_mem_size, word ram_mem_start, const byte rom_data[], word rom_mem_size, word rom_mem_start);

		struct EmulatorException {
			enum class Type {
				AOK,
				BAD_INSTR,
				HALT,
				BAD_REG
			};

			Type type = Type::AOK;
			word instr = 0;
			SystemBus::SystemBusException sys_bus_exception;
			Memory::MemoryReadException mem_read_exception;
			Memory::MemoryWriteException mem_write_exception;

			bool isOK();
		};

		enum class ConditionCode {
			EQ = 0,						/* Equal		: Z==1 */
			NE = 1,						/* Not Equal	: Z==0 */
			CS = 2, HS = 2,				/* Unsigned higher or same	: C==1 */
			CC = 3, LO = 3,				/* Unsigned lower			: C==0 */
			MI = 4,						/* Negative		: N==1 */
			PL = 5,						/* Nonnegative	: N==0 */
			VS = 6,						/* Signed overflow 			: V==1 */
			VC = 7,						/* No signed overflow		: V==0 */
			HI = 8,						/* Unsigned higher			: (C==1) && (Z==0) */
			LS = 9,						/* Unsigned lower or same 	: (C==0) || (Z==0) */
			GE = 10,					/* Signed greater than or equal	: N==V */
			LT = 11,					/* Signed less than				: N!=V */
			GT = 12,					/* Signed greater than			: (Z==0) && (N==V) */
			LE = 13,					/* Signed less than or equal 	: (Z==1) || (N!=V) */
			AL = 14,					/* Always Executed				: NONE */
			NV = 15,					/* Never Executed 				: NONE */
		};

		static const word RAM_MEM_SIZE;
		static const word RAM_MEM_START;
		static const word ROM_MEM_SIZE;
		static const word ROM_MEM_START;
		static const byte ROM_DATA[];

		SystemBus system_bus;

		/**
		 * Run the emulator for a given number of instructions
		 *
		 * @param instructions The number of instructions to run, if 0 run until HLT instruction
		 * @param exception The exception raised by the run operation
		 */
		void run(unsigned int instructions, EmulatorException &exception);
		void run(unsigned int instructions);

		void reset();
		void set_NZCV(bool N, bool Z, bool C, bool V);

		// general purpose registers
		word _x[31];
		word _pc;
		word _pstate;


		// todo determine if fp registers are needed
		// word fpcr;
		// word fpsr;

	private:
		static const int _num_instructions = 64;
		// std::function<void(word, EmulatorException&)> _instructions[_num_instructions];
		typedef void (Emulator32bit::*InstructionFunction)(word, EmulatorException&);
		InstructionFunction _instructions[_num_instructions];

		// note, stringstreams cannot use the static const for some reason
		#define _INSTR(func_name, opcode) \
		private: void _##func_name(word instr, EmulatorException& exception); \
		public: static const byte _op_##func_name = opcode;

		void execute(word instr, EmulatorException &exception);

		bool check_cond(word pstate, byte cond);

		word read_reg(byte reg, EmulatorException &exception);
		word calc_mem_addr(word xn, sword offset, byte addr_mode, EmulatorException& exception);
		void write_reg(byte reg, word val, EmulatorException &exception);
		void handle_exception(EmulatorException &exception);

		// instruction handling
		_INSTR(hlt, 0b000000)

		_INSTR(add, 0b000001)
		_INSTR(sub, 0b000010)
		_INSTR(rsb, 0b000011)
		_INSTR(adc, 0b000100)
		_INSTR(sbc, 0b000101)
		_INSTR(rsc, 0b000110)
		_INSTR(mul, 0b000111)
		_INSTR(umull, 0b001000)
		_INSTR(smull, 0b001001)

		_INSTR(vabs_f32, 0b001010)
		_INSTR(vneg_f32, 0b001011)
		_INSTR(vsqrt_f32, 0b001100)
		_INSTR(vadd_f32, 0b001101)
		_INSTR(vsub_f32, 0b001110)
		_INSTR(vdiv_f32, 0b001111)
		_INSTR(vmul_f32, 0b010000)
		_INSTR(vcmp_f32, 0b010001)
		_INSTR(vsel_f32, 0b010010)
		_INSTR(vcint_f32, 0b010011)
		_INSTR(vcflo_f32, 0b010100)
		_INSTR(vmov_f32, 0b010101)

		_INSTR(and, 0b010110)
		_INSTR(orr, 0b010111)
		_INSTR(eor, 0b011000)
		_INSTR(bic, 0b011001)
		_INSTR(lsl, 0b011010)
		_INSTR(lsr, 0b011011)
		_INSTR(asr, 0b011100)
		_INSTR(ror, 0b011101)

		_INSTR(cmp, 0b011110)
		_INSTR(cmn, 0b011111)
		_INSTR(tst, 0b100000)
		_INSTR(teq, 0b100001)

		_INSTR(mov, 0b100010)
		_INSTR(mvn, 0b100011)

		_INSTR(ldr, 0b100100)
		_INSTR(ldrb, 0b100101)
		_INSTR(ldrh, 0b100110)
		_INSTR(str, 0b100111)
		_INSTR(strb, 0b101000)
		_INSTR(strh, 0b101001)
		_INSTR(swp, 0b101010)
		_INSTR(swpb, 0b101011)
		_INSTR(swph, 0b101100)
		_INSTR(b, 0b101101)
		_INSTR(bl, 0b101110)
		_INSTR(bx, 0b101111)
		_INSTR(blx, 0b110000)
		_INSTR(swi, 0b110001)
		// _INSTR(nop_, 0b110010)
		// _INSTR(nop_, 0b110011)
		// _INSTR(nop_, 0b110100)
		// _INSTR(nop_, 0b110101)
		// _INSTR(nop_, 0b110110)
		// _INSTR(nop_, 0b110111)
		// _INSTR(nop_, 0b111000)
		// _INSTR(nop_, 0b111001)
		// _INSTR(nop_, 0b111010)
		// _INSTR(nop_, 0b111011)
		// _INSTR(nop_, 0b111100)
		// _INSTR(nop_, 0b111101)
		// _INSTR(nop_, 0b111110)

		_INSTR(nop, 0b111111)

		#undef _INSTR

	public:
		// help assemble instructions
		static word asm_hlt();
		static word asm_format_o(byte opcode, bool s, int xd, int xn, int imm14);
		static word asm_format_o(byte opcode, bool s, int xd, int xn, int xm, int shift, int imm5);
		static word asm_format_o1(byte opcode, int xd, int xn, bool imm, int xm, int imm5);
		static word asm_format_o2(byte opcode, bool s, int xlo, int xhi, int xn, int xm);
		static word asm_format_m(byte opcode, bool sign, int xt, int xn, int xm, int shift, int imm5, int adr);
		static word asm_format_m(byte opcode, bool sign, int xt, int xn, int simm12, int adr);
		static word asm_format_m1(byte opcode, int xd, int xn, int xm);
		static word asm_format_b1(byte opcode, ConditionCode cond, sword simm22);
		static word asm_format_b2(byte opcode, ConditionCode cond, int xd);

		static word asm_nop();
};

#endif