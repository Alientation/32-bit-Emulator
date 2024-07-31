#pragma once
#ifndef PREPROCESSOR_H
#define PREPROCESSOR_H

#include "assembler/build.h"
#include "assembler/tokenizer.h"
#include "util/file.h"

#include <string>
#include <stack>
#include <map>
#include <functional>

class Preprocessor
{
	public:
		enum State {
			UNPROCESSED, PROCESSING, PROCESSED_SUCCESS, PROCESSED_ERROR
		};

		Preprocessor(Process *process, const File& input_file, const std::string& output_file_path = "");	// constructs a preprocessor object with the given file
		~Preprocessor();							// destructs a preprocessor object

		File preprocess();							// preprocesses the file
		State get_state();							// returns the state of the preprocessor

	private:
        struct Argument {
            std::string name;
            Tokenizer::Type type;

            Argument(std::string name, Tokenizer::Type type);
            Argument(std::string name);
        };

		struct Macro {
            std::string name;
            std::vector<Argument> args;
            Tokenizer::Type return_type;

            std::vector<Tokenizer::Token> definition;

            Macro(std::string name);

            std::string to_string();
            std::string header();
		};

        struct Symbol {
            std::string name;
            std::vector<std::string> parameters;
            std::vector<Tokenizer::Token> value;

            Symbol(std::string name, std::vector<std::string> parameters, std::vector<Tokenizer::Token> value);
        };


		Process *m_process;										    // the build process

		File m_input_file;										    // the .basm or .binc file being preprocessed
		File m_output_file;										    // the output file of the processed file, usually a .bi file
		State m_state;											    // the state of the preprocessor
		std::vector<Tokenizer::Token> m_tokens;					    // the tokens of the input file

		std::stack<std::pair<std::string, Macro*>> m_macro_stack;    // the current processing macro stack with the output symbol and macro

        std::map<std::string, std::map<int, Symbol>> m_def_symbols; // defined symbols
		std::map<std::string, Macro*> m_macros;					    // defined macros

		std::vector<Macro*> macros_with_header(std::string macro_name, std::vector<std::vector<Tokenizer::Token>> args);

		void skip_tokens(int& tok_i, const std::string& regex);
        void skip_tokens(int& tok_i, const std::set<Tokenizer::Type>& tok_types);
		bool expect_token(int tok_i, const std::string& error_msg);
        bool expect_token(int tok_i, const std::set<Tokenizer::Type>& tok_types, const std::string& error_msg);
        bool is_token(int tok_i, const std::set<Tokenizer::Type>& tok_types, const std::string& error_msg = "Preprocessor::is_token() - Unexpected end of file");
        bool in_bounds(int tok_i);
        Tokenizer::Token& consume(int& tok_i, const std::string& error_msg = "Preprocessor::consume() - Unexpected end of file");
        Tokenizer::Token& consume(int& tok_i, const std::set<Tokenizer::Type>& expected_types, const std::string& error_msg = "Preprocessor::consume() - Unexpected token");

		void _include(int& tok_i);		// todo, make angled brackets <...> capture everything inside as a token to not have to surround inside with a string
		void _macro(int& tok_i);
		void _macret(int& tok_i);
		void _macend(int& tok_i);
		void _invoke(int& tok_i);
		void _define(int& tok_i);
        void _cond_on_def(int& tok_i);
        void _cond_on_value(int& tok_i);
		void _else(int& tok_i);
		void _endif(int& tok_i);
		void _undefine(int& tok_i);

        void cond_block(int& tok_i, bool cond_met);
        bool is_symbol_def(std::string symbol_name, int num_params);

		typedef void (Preprocessor::*PreprocessorFunction)(int& tok_i);
		std::map<Tokenizer::Type,PreprocessorFunction> preprocessors = {
			{Tokenizer::PREPROCESSOR_INCLUDE, &Preprocessor::_include},
			{Tokenizer::PREPROCESSOR_MACRO, &Preprocessor::_macro},
			{Tokenizer::PREPROCESSOR_MACRET, &Preprocessor::_macret},
			{Tokenizer::PREPROCESSOR_MACEND, &Preprocessor::_macend},
			{Tokenizer::PREPROCESSOR_INVOKE, &Preprocessor::_invoke},
			{Tokenizer::PREPROCESSOR_DEFINE, &Preprocessor::_define},

			{Tokenizer::PREPROCESSOR_IFDEF, &Preprocessor::_cond_on_def},
			{Tokenizer::PREPROCESSOR_IFNDEF, &Preprocessor::_cond_on_def},

            {Tokenizer::PREPROCESSOR_IFEQU, &Preprocessor::_cond_on_value},
            {Tokenizer::PREPROCESSOR_IFNEQU, &Preprocessor::_cond_on_value},
            {Tokenizer::PREPROCESSOR_IFLESS, &Preprocessor::_cond_on_value},
            {Tokenizer::PREPROCESSOR_IFMORE, &Preprocessor::_cond_on_value},

			{Tokenizer::PREPROCESSOR_ELSE, &Preprocessor::_else},

			{Tokenizer::PREPROCESSOR_ELSEDEF, &Preprocessor::_cond_on_def},
			{Tokenizer::PREPROCESSOR_ELSENDEF, &Preprocessor::_cond_on_def},

            {Tokenizer::PREPROCESSOR_ELSEEQU, &Preprocessor::_cond_on_value},
            {Tokenizer::PREPROCESSOR_ELSENEQU, &Preprocessor::_cond_on_value},
            {Tokenizer::PREPROCESSOR_ELSELESS, &Preprocessor::_cond_on_value},
            {Tokenizer::PREPROCESSOR_ELSEMORE, &Preprocessor::_cond_on_value},

			{Tokenizer::PREPROCESSOR_ENDIF, &Preprocessor::_endif},
			{Tokenizer::PREPROCESSOR_UNDEF, &Preprocessor::_undefine}
		};
};


#endif /* PREPROCESSOR_H */