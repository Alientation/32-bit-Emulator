#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include "assembler/Build.h"
#include "assembler/Tokenizer.h"
#include "util/File.h"

#include <string>
#include <map>

class Assembler {
	public:
		enum State {
			NOT_ASSEMBLED, ASSEMBLING, ASSEMBLED, ASSEMBLER_ERROR,
		};

		Assembler(Process *process, File *processed_file, std::string output_path = "");
		~Assembler();

		void assemble();
		State get_state();

	private:
		Process *m_process;										    // the build process

		File *m_inputFile;										    // the .bi file being assembled
		File *m_outputFile;										    // the object file, a .bo file
		State m_state;											    // the state of the assembler
		std::vector<Tokenizer::Token> m_tokens;					    // the tokens of the input processed file

		FileWriter *m_writer;									    // writer for the output file

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
		void _equ(int& tokenI);
		void _org(int& tokenI);
		void _scope(int& tokenI);
		void _scend(int& tokenI);
		void _advance(int& tokenI);
		void _align(int& tokenI);
		void _data(int& tokenI);
		void _bss(int& tokenI);
		void _stop(int& tokenI);

		typedef void (Assembler::*DirectiveFunction)(int& tokenI);
		std::map<Tokenizer::Type,DirectiveFunction> directives = {
			{Tokenizer::ASSEMBLER_GLOBAL, &Assembler::_global},
			{Tokenizer::ASSEMBLER_EXTERN, &Assembler::_extern},
			{Tokenizer::ASSEMBLER_EQU, &Assembler::_equ},
			{Tokenizer::ASSEMBLER_ORG, &Assembler::_org},
			{Tokenizer::ASSEMBLER_SCOPE, &Assembler::_scope},
			{Tokenizer::ASSEMBLER_SCEND, &Assembler::_scend},
			{Tokenizer::ASSEMBLER_ADVANCE, &Assembler::_advance},
			{Tokenizer::ASSEMBLER_ALIGN, &Assembler::_align},
			{Tokenizer::ASSEMBLER_DATA, &Assembler::_data},
			{Tokenizer::ASSEMBLER_BSS, &Assembler::_bss},
			{Tokenizer::ASSEMBLER_STOP, &Assembler::_stop},
		};
		typedef void (Assembler::*InstructionFunction)(int& tokenI);
		std::map<Tokenizer::Type,InstructionFunction> instructions = {

		};
};

#endif