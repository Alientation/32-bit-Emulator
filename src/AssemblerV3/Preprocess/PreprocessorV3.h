#include <../src/util/File.h>
#include <../src/AssemblerV3/Build.h>
#include "../src/util/AssemblyType.h"

#include <string>
#include <map>
#include <memory>
#include <functional>

#ifndef PREPROCESSORV3_H
#define PREPROCESSORV3_H

class Preprocessor;
class Preprocessor {
	public:
		enum State {
			UNPROCESSED, PROCESSING, PROCESSED_SUCCESS, PROCESSED_ERROR
		};

		Preprocessor(Process* process, File* file, std::string outputFilePath = "");	// constructs a preprocessor object with the given file
		~Preprocessor();							// destructs a preprocessor object

		void preprocess();							// preprocesses the file
		State getState();							// returns the state of the preprocessor
	
	private:
		/**
		 * Base source code character set
		 * 
		 * a-z A-Z 0-9 _ { } [ ] ( ) < > % : ; . , ? * + - / ^ & | ~ ! = " ' \ # @ $
		 */
		struct Token {
			enum Type {
				TEXT, WHITESPACE_SPACE, WHITESPACE_TAB, WHITESPACE_NEWLINE, WHITESPACE,

				// PREPROCESSOR DIRECTIVES
				PREPROCESSOR_INCLUDE, 
				PREPROCESSOR_MACRO, PREPROCESSOR_MACRET, PREPROCESSOR_MACEND, PREPROCESSOR_INVOKE, 
				PREPROCESSOR_DEFINE, PREPROCESSOR_UNDEF, 
				PREPROCESSOR_IFDEF, PREPROCESSOR_IFNDEF, 
				PREPROCESSOR_ELSE, PREPROCESSOR_ELSEDEF, PREPROCESSOR_ELSENDEF, 
				PREPROCESSOR_ENDIF,

				// ASSEMBLER DIRECTIVES
				ASSEMBLER_EQU, // TODO:
				ASSEMBLER_ORG,

				LITERAL_NUMBER, LITERAL_STRING,
				SYMBOL, SYMBOL_SPECIAL, 
				COLON, COMMA, SEMICOLON,
				OPEN_PARANTHESIS, CLOSE_PARANTHESIS, OPEN_BRACKET, CLOSE_BRACKET, OPEN_BRACE, CLOSE_BRACE,
				OPERATOR
			};

			inline static const std::map<Type, std::string> TYPE_NAME = {
				{TEXT, "TEXT"}, {WHITESPACE_SPACE, "WHITESPACE_SPACE"}, {WHITESPACE_TAB, "WHITE_SPACE_TAB"}, {WHITESPACE_NEWLINE, "WHITESPACE_NEWLINE"},

				{PREPROCESSOR_INCLUDE, "PREPROCESSOR_INCLUDE"},
				{PREPROCESSOR_MACRO, "PREPROCESSOR_MACRO"}, {PREPROCESSOR_MACRET, "PREPROCESSOR_MACRET"}, 
				{PREPROCESSOR_MACEND, "PREPROCESSOR_MACEND"}, {PREPROCESSOR_INVOKE, "PREPROCESSOR_INVOKE"},
				{PREPROCESSOR_DEFINE, "PREPROCESSOR_DEFINE"}, {PREPROCESSOR_UNDEF, "PREPROCESSOR_UNDEF"},
				{PREPROCESSOR_IFDEF, "PREPROCESSOR_IFDEF"}, {PREPROCESSOR_IFNDEF, "PREPROCESSOR_IFNDEF"},
				{PREPROCESSOR_ELSE, "PREPROCESSOR_ELSE"}, {PREPROCESSOR_ELSEDEF, "PREPROCESSOR_ELSEDEF"}, 
				{PREPROCESSOR_ELSENDEF, "PREPROCESSOR_ELSENDEF"},
				{PREPROCESSOR_ENDIF, "PREPROCESSOR_ENDIF"},

				{ASSEMBLER_EQU, "ASSEMBLER_EQU"},
				{ASSEMBLER_ORG, "ASSEMBLER_ORG"},

				{LITERAL_NUMBER, "LITERAL_NUMBER"}, {LITERAL_STRING, "LITERAL_STRING"},
				{SYMBOL, "SYMBOL"}, {SYMBOL_SPECIAL, "SYMBOL_SPECIAL"}, 
				{COLON, "COLON"}, {COMMA, "COMMA"}, {SEMICOLON, "SEMICOLON"},
				{OPEN_PARANTHESIS, "OPEN_PARANTHESIS"}, {CLOSE_PARANTHESIS, "CLOSE_PARANTHESIS"}, 
				{OPEN_BRACKET, "OPEN_BRACKET"}, {CLOSE_BRACKET, "CLOSE_BRACKET"}, 
				{OPEN_BRACE, "OPEN_BRACE"}, {CLOSE_BRACE, "CLOSE_BRACE"},
				{OPERATOR, "OPERATOR"}
			};

			Type type;
			std::string value;

			Token(Type type, std::string value) {
				this->type = type;
				this->value = value;
			}

			std::string toString() {
				if (type == WHITESPACE_SPACE || type == WHITESPACE_TAB || type == WHITESPACE_NEWLINE) {
					std::string toString = TYPE_NAME.at(type) + ":";
					for (auto i = 0; i < value.length(); i++) {
						toString += " " + std::to_string(value[i]);
					}
					return toString;
				}

				return TYPE_NAME.at(type) + ": " + value;
			}
		};

		inline static const std::vector<std::pair<std::string, Token::Type>> TOKEN_SPEC = {
			{"^ ", Token::WHITESPACE_SPACE}, {"^\\t", Token::WHITESPACE_TAB}, {"^\\n", Token::WHITESPACE_NEWLINE},
			{"^[\\s^[ \n\t]]+", Token::WHITESPACE},
			{"^\\{", Token::OPEN_BRACE}, 		{"^\\}", Token::CLOSE_BRACE},
			{"^\\[", Token::OPEN_BRACKET}, 	{"^\\]", Token::CLOSE_BRACKET},
			{"^\\(", Token::OPEN_PARANTHESIS},{"^\\)", Token::CLOSE_PARANTHESIS},
			{"^,", Token::COMMA}, {"^:", Token::COLON}, {"^;", Token::SEMICOLON},

			{"^#include(?=\\s)", Token::PREPROCESSOR_INCLUDE},
			{"^#macro(?=\\s)", Token::PREPROCESSOR_MACRO},
			{"^#macret(?=\\s)", Token::PREPROCESSOR_MACRET},
			{"^#macend(?=\\s)", Token::PREPROCESSOR_MACEND},
			{"^#invoke(?=\\s)", Token::PREPROCESSOR_INVOKE},
			{"^#define(?=\\s)", Token::PREPROCESSOR_DEFINE},
			{"^#undef(?=\\s)", Token::PREPROCESSOR_UNDEF},
			{"^#ifdef(?=\\s)", Token::PREPROCESSOR_IFDEF},
			{"^#ifndef(?=\\s)", Token::PREPROCESSOR_IFNDEF},
			{"^#else(?=\\s)", Token::PREPROCESSOR_ELSE},
			{"^#elsedef(?=\\s)", Token::PREPROCESSOR_ELSEDEF},
			{"^#elsendef(?=\\s)", Token::PREPROCESSOR_ELSENDEF},
			{"^#endif(?=\\s)", Token::PREPROCESSOR_ENDIF},

			{"^\\.equ(?=\\s)", Token::ASSEMBLER_EQU},
			{"^\\.org(?=\\s)", Token::ASSEMBLER_ORG},			

			{"^#?[%|0|$]?[0-9a-fA-F]+", Token::LITERAL_NUMBER},
			{"^\".*\"", Token::LITERAL_STRING},
			{"^[a-zA-Z_][a-zA-Z0-9_]*", Token::SYMBOL},
			{"^@[a-zA-Z_][a-zA-Z0-9_]*", Token::SYMBOL_SPECIAL},
			{"^\\+", Token::OPERATOR}, {"^\\-", Token::OPERATOR}, {"^\\*", Token::OPERATOR}, {"^\\/", Token::OPERATOR}, {"^\\%", Token::OPERATOR},
			{"^\\<\\<", Token::OPERATOR}, {"^\\>\\>", Token::OPERATOR},
			{"^\\^", Token::OPERATOR}, {"^\\&", Token::OPERATOR}, {"^\\|", Token::OPERATOR}, {"^~", Token::OPERATOR},
			{"^!", Token::OPERATOR}, {"^==", Token::OPERATOR},
			{"^\\<?=", Token::OPERATOR}, {"^\\>?=", Token::OPERATOR},
		};

		struct Macro {
			struct Argument {
				std::string argName;
				VariableType argType;
			};

			std::string macroName;
			std::vector<Argument> macroArguments;
			VariableType returnType;

			std::vector<Token> macroBody;
		};

		std::shared_ptr<Process> process;			// the build process

		std::shared_ptr<File> inputFile;			// the input file
		std::shared_ptr<File> outputFile;			// the output file
		State state;								// the state of the preprocessor
		std::vector<Token> tokens;					// the tokens of the input file

		std::shared_ptr<FileWriter> writer;			// writer for the output file

		std::map<std::string, std::string> symbols;	// defined symbols
		std::map<std::string, Macro> macros;		// defined macros

		void tokenize();
		void new_tokenize();
		void skipTokens(int& tokenI, std::string regex);
		void expectToken(int& tokenI, std::string errorMsg);

		void _include(int& tokenI);
		void _macro(int& tokenI);
		void _macret(int& tokenI);
		void _macend(int& tokenI);
		void _invoke(int& tokenI);
		void _define(int& tokenI);
		void _ifdef(int& tokenI);
		void _ifndef(int& tokenI);
		void _else(int& tokenI);
		void _elsedef(int& tokenI);
		void _elsendef(int& tokenI);
		void _endif(int& tokenI);
		void _undefine(int& tokenI);

		typedef void (Preprocessor::*PreprocessorFunction)(int& tokenI);
		std::map<std::string,PreprocessorFunction> directives = {
			{"#include", &Preprocessor::_include},
			{"#macro", &Preprocessor::_macro},
			{"#macret", &Preprocessor::_macret},
			{"#macend", &Preprocessor::_macend},
			{"#invoke", &Preprocessor::_invoke},
			{"#define", &Preprocessor::_define},
			{"#ifdef", &Preprocessor::_ifdef},
			{"#ifndef", &Preprocessor::_ifndef},
			{"#else", &Preprocessor::_else},
			{"#elsedef", &Preprocessor::_elsedef},
			{"#elsendef", &Preprocessor::_elsendef},
			{"#endif", &Preprocessor::_endif},
			{"#undef", &Preprocessor::_undefine}
		};
};


#endif