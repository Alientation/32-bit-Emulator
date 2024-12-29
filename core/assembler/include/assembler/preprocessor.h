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

		/**
		 * Constructs a preprocessor object with the given file.
		 *
		 * @param process the build process object.
		 * @param file the file to preprocess.
		 * @param outputFilePath the path to the output file, default is the inputfile path with .bi extension.
		 */
		Preprocessor(Process *process, const File& input_file, const std::string& output_file_path = "");
		~Preprocessor();

		File preprocess();
		State get_state();

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


		Process *m_process;

		// the .basm or .binc file being preprocessed
		File m_input_file;

		Tokenizer tokenizer;

		// the output file of the processed file, usually a .bi file
		File m_output_file;
		State m_state;

		// the current processing macro stack with the output symbol and macro
		std::stack<std::pair<std::string, Macro*>> m_macro_stack;

        std::map<std::string, std::map<int, Symbol>> m_def_symbols;
		std::map<std::string, Macro*> m_macros;

		std::vector<Macro*> macros_with_header(std::string macro_name,
											   std::vector<std::vector<Tokenizer::Token>> args);

		/* TODO:, make angled brackets <...> capture everything inside as a token to
		 not have to surround inside with a string */
		void _include();
		void _macro();
		void _macret();
		void _macend();
		void _invoke();
		void _define();
        void _cond_on_def();
        void _cond_on_value();
		void _else();
		void _endif();
		void _undefine();

        void cond_block(bool cond_met);
        bool is_symbol_def(std::string symbol_name, int num_params);

		typedef void (Preprocessor::*PreprocessorFunction)();
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