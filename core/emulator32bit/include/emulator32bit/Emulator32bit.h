#pragma once
#ifndef Emulator32bit_H
#define Emulator32bit_H

class Emulator32bit {
	public:
		Emulator32bit();
		~Emulator32bit();

		enum EmulatorException {
			EXCEPTION_INVALID_INSTRUCTION
		};

		/**
		 * Run the emulator for a given number of instructions
		 * 
		 * @param instructions The number of instructions to run, if 0 run until HLT instruction
		 * @param exception The exception raised by the run operation
		 */
		void run(const unsigned int instructions, EmulatorException *exception);
};

#endif