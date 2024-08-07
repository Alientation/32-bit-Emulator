#pragma once
#ifndef LINKER_H
#define LINKER_H

#include "assembler/object_file.h"

/*
	Linker script

	Keep things simple

	allow addresses of each section to be specified
		- specify whether it is physical or virtual memory addresses
	allow entry point symbol to be defined

*/

class Linker
{
	public:
		Linker(std::vector<ObjectFile> obj_files, File exe_file);
		Linker(std::vector<ObjectFile> obj_files, File exe_file, File ld_file);

	private:
		std::vector<ObjectFile> m_obj_files;

		File m_exe_file;
		File m_ld_file;

		struct Token
		{
			enum class Type
			{
				WHITESPACE,
				ENTRY, SECTIONS,
				TEXT, DATA, BSS,
				SECTION_COUNTER,
				LITERAL_NUMBER_BINARY, LITERAL_NUMBER_OCTAL, LITERAL_NUMBER_DECIMAL, LITERAL_NUMBER_HEXADECIMAL,
				OPEN_PARENTHESIS, CLOSE_PARENTHESIS,
				SEMI_COLON, COMMA, EQUAL, AT,
				SYMBOL,
			};

			Type type;
			std::string val;

			Token(Type type, std::string val);
		};

		static const std::vector<std::pair<std::string,Token::Type>> TOKEN_SPEC;

		std::vector<Token> m_tokens;

		void link();
		void tokenize();
};

#endif /* LINKER_H */