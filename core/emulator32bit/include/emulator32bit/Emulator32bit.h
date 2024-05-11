#pragma once
#ifndef Emulator32bit_H
#define Emulator32bit_H

#include <emulator32bit/Emulator32bitUtil.h>

#include <functional>

#define XZR 30
#define SP 31

class Emulator32bit {
	public:
		Emulator32bit();
		~Emulator32bit();

		struct EmulatorException {
			enum class Type {
				INSTRUCTION
			};
			
			Type type;

			
		};

		struct InstructionException {
			enum class Type {
				INVALID_INSTRUCTION
			};

			Type type;
			word instruction;
		};
		

		/**
		 * Run the emulator for a given number of instructions
		 * 
		 * @param instructions The number of instructions to run, if 0 run until HLT instruction
		 * @param exception The exception raised by the run operation
		 */
		void run(unsigned int instructions, EmulatorException &exception);

	private:
		// general purpose registers
		word _x[30];
		const word _xzr = 0;
		word _sp;
		word _pstate;

		// todo determine if fp registers are needed
		// word fpcr;
		// word fpsr;

		static const int _num_instructions = 51;
		std::function<void(word, EmulatorException&)> _instructions[_num_instructions];

		#define _INSTR(func_name, opcode) \
		void _##func_name(word instr, EmulatorException& exception); \
		static const byte _op_##func_name = opcode;
		

		void execute(word instr, EmulatorException &exception);

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
};

#endif