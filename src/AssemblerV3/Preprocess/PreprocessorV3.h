#include <../src/util/File.h>
#include <../src/AssemblerV3/Build.h>

#include <string>
#include <map>
#include <functional>

#ifndef PREPROCESSORV3_H
#define PREPROCESSORV3_H

class Preprocessor;
class Preprocessor {
	public:
		enum State {
			UNPROCESSED, PROCESSING, PROCESSED_SUCCESS, PROCESSED_ERROR
		};

		Preprocessor(Process* process, File* inputFile, std::string outputFilePath = "");	// constructs a preprocessor object with the given file
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
                UNKNOWN,

				TEXT, WHITESPACE_SPACE, WHITESPACE_TAB, WHITESPACE_NEWLINE, WHITESPACE, 
                COMMENT_SINGLE_LINE, COMMENT_MULTI_LINE,

				// PREPROCESSOR DIRECTIVES
				PREPROCESSOR_INCLUDE, 
				PREPROCESSOR_MACRO, PREPROCESSOR_MACRET, PREPROCESSOR_MACEND, PREPROCESSOR_INVOKE, 
				PREPROCESSOR_DEFINE, PREPROCESSOR_UNDEF, 
				PREPROCESSOR_IFDEF, PREPROCESSOR_IFNDEF, 
				PREPROCESSOR_ELSE, PREPROCESSOR_ELSEDEF, PREPROCESSOR_ELSENDEF, 
				PREPROCESSOR_ENDIF,

                // VARIABLE TYPES
                VARIABLE_TYPE_BYTE, VARIABLE_TYPE_DBYTE, VARIABLE_TYPE_WORD, VARIABLE_TYPE_DWORD,
                VARIABLE_TYPE_CHAR, VARIABLE_TYPE_STRING, VARIABLE_TYPE_FLOAT, VARIABLE_TYPE_DOUBLE,
                VARIABLE_TYPE_BOOLEAN,

				// ASSEMBLER DIRECTIVES
				ASSEMBLER_EQU, // TODO:
				ASSEMBLER_ORG,

                NUMBER_SIGN,
				LITERAL_NUMBER_BINARY, LITERAL_NUMBER_OCTAL, LITERAL_NUMBER_DECIMAL, LITERAL_NUMBER_HEXADECIMAL, 
                LITERAL_CHAR, LITERAL_STRING,

				SYMBOL, 
				COLON, COMMA, SEMICOLON,
				OPEN_PARANTHESIS, CLOSE_PARANTHESIS, OPEN_BRACKET, CLOSE_BRACKET, OPEN_BRACE, CLOSE_BRACE,

                OPERATOR_ADDITION, OPERATOR_SUBTRACTION, OPERATOR_MULTIPLICATION, OPERATOR_DIVISION, OPERATOR_MODULUS,
                OPERATOR_BITWISE_LEFT_SHIFT, OPERATOR_BITWISE_RIGHT_SHIFT, 
                OPERATOR_BITWISE_XOR, OPERATOR_BITWISE_AND, OPERATOR_BITWISE_OR, OPERATOR_BITWISE_COMPLEMENT,
                OPERATOR_LOGICAL_NOT, OPERATOR_LOGICAL_EQUAL, OPERATOR_LOGICAL_NOT_EQUAL,
                OPERATOR_LOGICAL_LESS_THAN, OPERATOR_LOGICAL_GREATER_THAN, 
                OPERATOR_LOGICAL_LESS_THAN_OR_EQUAL, OPERATOR_LOGICAL_GREATER_THAN_OR_EQUAL,
                OPERATOR_LOGICAL_OR, OPERATOR_LOGICAL_AND,
			};

			inline static const std::map<Type, std::string> TYPE_NAME = {
                {UNKNOWN, "UNKNOWN"},

				{TEXT, "TEXT"}, 
                {WHITESPACE_SPACE, "WHITESPACE_SPACE"}, {WHITESPACE_TAB, "WHITE_SPACE_TAB"}, {WHITESPACE_NEWLINE, "WHITESPACE_NEWLINE"},
                {COMMENT_SINGLE_LINE, "COMMENT_SINGLE_LINE"}, {COMMENT_MULTI_LINE, "COMMENT_MULTI_LINE"},

				{PREPROCESSOR_INCLUDE, "PREPROCESSOR_INCLUDE"},
				{PREPROCESSOR_MACRO, "PREPROCESSOR_MACRO"}, {PREPROCESSOR_MACRET, "PREPROCESSOR_MACRET"}, 
				{PREPROCESSOR_MACEND, "PREPROCESSOR_MACEND"}, {PREPROCESSOR_INVOKE, "PREPROCESSOR_INVOKE"},
				{PREPROCESSOR_DEFINE, "PREPROCESSOR_DEFINE"}, {PREPROCESSOR_UNDEF, "PREPROCESSOR_UNDEF"},
				{PREPROCESSOR_IFDEF, "PREPROCESSOR_IFDEF"}, {PREPROCESSOR_IFNDEF, "PREPROCESSOR_IFNDEF"},
				{PREPROCESSOR_ELSE, "PREPROCESSOR_ELSE"}, {PREPROCESSOR_ELSEDEF, "PREPROCESSOR_ELSEDEF"}, 
				{PREPROCESSOR_ELSENDEF, "PREPROCESSOR_ELSENDEF"},
				{PREPROCESSOR_ENDIF, "PREPROCESSOR_ENDIF"},

                {VARIABLE_TYPE_BYTE, "VARIABLE_TYPE_BYTE"}, {VARIABLE_TYPE_DBYTE, "VARIABLE_TYPE_DBYTE"},
                {VARIABLE_TYPE_WORD, "VARIABLE_TYPE_WORD"}, {VARIABLE_TYPE_DWORD, "VARIABLE_TYPE_DWORD"},
                {VARIABLE_TYPE_CHAR, "VARIABLE_TYPE_CHAR"}, {VARIABLE_TYPE_STRING, "VARIABLE_TYPE_STRING"},
                {VARIABLE_TYPE_FLOAT, "VARIABLE_TYPE_FLOAT"}, {VARIABLE_TYPE_DOUBLE, "VARIABLE_TYPE_DOUBLE"},
                {VARIABLE_TYPE_BOOLEAN, "VARIABLE_TYPE_BOOLEAN"},

				{ASSEMBLER_EQU, "ASSEMBLER_EQU"},
				{ASSEMBLER_ORG, "ASSEMBLER_ORG"},

                {NUMBER_SIGN, "NUMBER_SIGN"},
                {LITERAL_NUMBER_BINARY, "LITERAL_NUMBER_BINARY"}, {LITERAL_NUMBER_OCTAL, "LITERAL_NUMBER_OCTAL"},
                {LITERAL_NUMBER_DECIMAL, "LITERAL_NUMBER_DECIMAL"}, {LITERAL_NUMBER_HEXADECIMAL, "LITERAL_NUMBER_HEXADECIMAL"},
				{LITERAL_CHAR, "LITERAL_CHAR"}, {LITERAL_STRING, "LITERAL_STRING"},
				{SYMBOL, "SYMBOL"}, 
				{COLON, "COLON"}, {COMMA, "COMMA"}, {SEMICOLON, "SEMICOLON"},
				{OPEN_PARANTHESIS, "OPEN_PARANTHESIS"}, {CLOSE_PARANTHESIS, "CLOSE_PARANTHESIS"}, 
				{OPEN_BRACKET, "OPEN_BRACKET"}, {CLOSE_BRACKET, "CLOSE_BRACKET"}, 
				{OPEN_BRACE, "OPEN_BRACE"}, {CLOSE_BRACE, "CLOSE_BRACE"},

                {OPERATOR_ADDITION, "OPERATOR_ADDITION"}, {OPERATOR_SUBTRACTION, "OPERATOR_SUBTRACTION"},
                {OPERATOR_MULTIPLICATION, "OPERATOR_MULTIPLICATION"}, {OPERATOR_DIVISION, "OPERATOR_DIVISION"},
                {OPERATOR_MODULUS, "OPERATOR_MODULUS"}, {OPERATOR_BITWISE_LEFT_SHIFT, "OPERATOR_BITWISE_LEFT_SHIFT"},
                {OPERATOR_BITWISE_RIGHT_SHIFT, "OPERATOR_BITWISE_RIGHT_SHIFT"}, {OPERATOR_BITWISE_XOR, "OPERATOR_BITWISE_XOR"},
                {OPERATOR_BITWISE_AND, "OPERATOR_BITWISE_AND"}, {OPERATOR_BITWISE_OR, "OPERATOR_BITWISE_OR"},
                {OPERATOR_BITWISE_COMPLEMENT, "OPERATOR_BITWISE_COMPLEMENT"}, {OPERATOR_LOGICAL_NOT, "OPERATOR_LOGICAL_NOT"},
                {OPERATOR_LOGICAL_EQUAL, "OPERATOR_LOGICAL_EQUAL"}, {OPERATOR_LOGICAL_NOT_EQUAL, "OPERATOR_LOGICAL_NOT_EQUAL"},
                {OPERATOR_LOGICAL_LESS_THAN, "OPERATOR_LOGICAL_LESS_THAN"}, {OPERATOR_LOGICAL_GREATER_THAN, "OPERATOR_LOGICAL_GREATER_THAN"},
                {OPERATOR_LOGICAL_LESS_THAN_OR_EQUAL, "OPERATOR_LOGICAL_LESS_THAN_OR_EQUAL"}, {OPERATOR_LOGICAL_GREATER_THAN_OR_EQUAL, "OPERATOR_LOGICAL_GREATER_THAN_OR_EQUAL"},
                {OPERATOR_LOGICAL_OR, "OPERATOR_LOGICAL_OR"}, {OPERATOR_LOGICAL_AND, "OPERATOR_LOGICAL_AND"},
            };

            inline static const std::set<Type> WHITESPACES = {
                WHITESPACE_SPACE, WHITESPACE_TAB, WHITESPACE_NEWLINE
            };

            inline static const std::set<Type> COMMENTS = {
                COMMENT_SINGLE_LINE, COMMENT_MULTI_LINE
            };

            inline static const std::set<Type> PREPROCESSOR_DIRECTIVES = {
                PREPROCESSOR_INCLUDE, PREPROCESSOR_MACRO, PREPROCESSOR_MACRET, PREPROCESSOR_MACEND, PREPROCESSOR_INVOKE, 
                PREPROCESSOR_DEFINE, PREPROCESSOR_UNDEF, PREPROCESSOR_IFDEF, PREPROCESSOR_IFNDEF, PREPROCESSOR_ELSE, 
                PREPROCESSOR_ELSEDEF, PREPROCESSOR_ELSENDEF, PREPROCESSOR_ENDIF
            };

            inline static const std::set<Type> VARIABLE_TYPES = {
                VARIABLE_TYPE_BYTE, VARIABLE_TYPE_DBYTE, VARIABLE_TYPE_WORD, VARIABLE_TYPE_DWORD,
                VARIABLE_TYPE_CHAR, VARIABLE_TYPE_STRING, VARIABLE_TYPE_FLOAT, VARIABLE_TYPE_DOUBLE,
                VARIABLE_TYPE_BOOLEAN
            };

            inline static const std::set<Type> ASSEMBLER_DIRECTIVES = {
                ASSEMBLER_EQU, ASSEMBLER_ORG
            };

            inline static const std::set<Type> LITERAL_NUMBERS = {
                LITERAL_NUMBER_BINARY, LITERAL_NUMBER_OCTAL, LITERAL_NUMBER_DECIMAL, LITERAL_NUMBER_HEXADECIMAL
            };

            inline static const std::set<Type> LITERAL_VALUES = {
                LITERAL_NUMBER_BINARY, LITERAL_NUMBER_OCTAL, LITERAL_NUMBER_DECIMAL, LITERAL_NUMBER_HEXADECIMAL,
                LITERAL_CHAR, LITERAL_STRING
            };

            inline static const std::set<Type> OPERATORS = {
                OPERATOR_ADDITION, OPERATOR_SUBTRACTION, OPERATOR_MULTIPLICATION, OPERATOR_DIVISION, OPERATOR_MODULUS,
                OPERATOR_BITWISE_LEFT_SHIFT, OPERATOR_BITWISE_RIGHT_SHIFT, OPERATOR_BITWISE_XOR, OPERATOR_BITWISE_AND, 
                OPERATOR_BITWISE_OR, OPERATOR_BITWISE_COMPLEMENT, OPERATOR_LOGICAL_NOT, OPERATOR_LOGICAL_EQUAL, 
                OPERATOR_LOGICAL_NOT_EQUAL, OPERATOR_LOGICAL_LESS_THAN, OPERATOR_LOGICAL_GREATER_THAN, 
                OPERATOR_LOGICAL_LESS_THAN_OR_EQUAL, OPERATOR_LOGICAL_GREATER_THAN_OR_EQUAL, OPERATOR_LOGICAL_OR, 
                OPERATOR_LOGICAL_AND
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
				} else if (type == COMMENT_SINGLE_LINE || type == COMMENT_MULTI_LINE) {
                    return TYPE_NAME.at(type);
                }

				return TYPE_NAME.at(type) + ": " + value;
			}
		};

		inline static const std::vector<std::pair<std::string, Token::Type>> TOKEN_SPEC = {
			{"^ ", Token::WHITESPACE_SPACE}, {"^\\t", Token::WHITESPACE_TAB}, {"^\\n", Token::WHITESPACE_NEWLINE},
			{"^[\\s^[ \\n\\t]]+", Token::WHITESPACE},
            {"^;\\*[^*]*\\*+(?:[^;*][^*]*\\*+)*;", Token::COMMENT_MULTI_LINE}, {"^;.*", Token::COMMENT_SINGLE_LINE},
			{"^\\{", Token::OPEN_BRACE}, {"^\\}", Token::CLOSE_BRACE},
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

            {"^BYTE(?=\\s)", Token::VARIABLE_TYPE_BYTE}, {"^DBYTE(?=\\s)", Token::VARIABLE_TYPE_DBYTE},
            {"^WORD(?=\\s)", Token::VARIABLE_TYPE_WORD}, {"^DWORD(?=\\s)", Token::VARIABLE_TYPE_DWORD},

			{"^\\.equ(?=\\s)", Token::ASSEMBLER_EQU},
			{"^\\.org(?=\\s)", Token::ASSEMBLER_ORG},

            {"^#", Token::NUMBER_SIGN},
            {"^%[0-1]+", Token::LITERAL_NUMBER_BINARY},
            {"^@[0-7]+", Token::LITERAL_NUMBER_OCTAL}, 
            {"^[0-9]+", Token::LITERAL_NUMBER_DECIMAL},
            {"^\\$[0-9a-fA-F]+", Token::LITERAL_NUMBER_HEXADECIMAL},

			{"^\'.\'", Token::LITERAL_CHAR}, {"^\".*\"", Token::LITERAL_STRING},
			{"^[a-zA-Z_][a-zA-Z0-9_]*", Token::SYMBOL},

            
			{"^\\+", Token::OPERATOR_ADDITION}, {"^\\-", Token::OPERATOR_SUBTRACTION}, 
            {"^\\*", Token::OPERATOR_MULTIPLICATION}, {"^\\/", Token::OPERATOR_DIVISION}, 
            {"^\\%", Token::OPERATOR_MODULUS},
            {"^\\|\\|", Token::OPERATOR_LOGICAL_OR}, {"^\\&\\&", Token::OPERATOR_LOGICAL_AND},
			{"^\\<\\<", Token::OPERATOR_BITWISE_LEFT_SHIFT}, {"^\\>\\>", Token::OPERATOR_BITWISE_RIGHT_SHIFT},
			{"^\\^", Token::OPERATOR_BITWISE_XOR}, {"^\\&", Token::OPERATOR_BITWISE_AND}, 
            {"^\\|", Token::OPERATOR_BITWISE_OR}, {"^~", Token::OPERATOR_BITWISE_COMPLEMENT},
            {"^==", Token::OPERATOR_LOGICAL_EQUAL}, {"^!=", Token::OPERATOR_LOGICAL_NOT_EQUAL},
			{"^!", Token::OPERATOR_LOGICAL_NOT}, 
            {"^\\<=", Token::OPERATOR_LOGICAL_LESS_THAN_OR_EQUAL}, {"^\\>=", Token::OPERATOR_LOGICAL_GREATER_THAN_OR_EQUAL},
            {"^\\<", Token::OPERATOR_LOGICAL_LESS_THAN}, {"^\\>", Token::OPERATOR_LOGICAL_GREATER_THAN},
		};

        struct Argument {
            std::string name;
            Token::Type type;

            Argument(std::string name, Token::Type type) {
                this->name = name;
                this->type = type;
            }

            Argument(std::string name) {
                this->name = name;
                this->type = Token::UNKNOWN;
            }
        };

		struct Macro {
            std::string name;
            std::vector<Argument> arguments;
            Token::Type returnType;

            std::vector<Token> definition;

            Macro(std::string name) {
                this->name = name;
                this->returnType = Token::UNKNOWN;
            }
		};

		Process* process;			// the build process

		File* inputFile;			// the input file
		File* outputFile;			// the output file
		State state;								// the state of the preprocessor
		std::vector<Token> tokens;					// the tokens of the input file

		FileWriter* writer;			// writer for the output file

		std::map<std::string, std::string> symbols;	// defined symbols
		std::map<std::string, Macro> macros;		// defined macros

		void tokenize();
		void skipTokens(int& tokenI, const std::string& regex);
        void skipTokens(int& tokenI, const std::set<Token::Type>& tokenTypes);
		bool expectToken(int& tokenI, const std::string& errorMsg);
        bool expectToken(int& tokenI, const std::set<Token::Type>& tokenTypes, const std::string& errorMsg);
        bool isToken(int& tokenI, const std::set<Token::Type>& tokenTypes, const std::string& errorMsg = "Preprocessor::isToken() - Unexpected end of file");
        Token& consume(int& tokenI, const std::string& errorMsg = "Preprocessor::consume() - Unexpected end of file");
        Token& consume(int& tokenI, const std::set<Token::Type>& expectedTypes, const std::string& errorMsg = "Preprocessor::consume() - Unexpected token");

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
		std::map<Token::Type,PreprocessorFunction> directives = {
			{Token::PREPROCESSOR_INCLUDE, &Preprocessor::_include},
			{Token::PREPROCESSOR_MACRO, &Preprocessor::_macro},
			{Token::PREPROCESSOR_MACRET, &Preprocessor::_macret},
			{Token::PREPROCESSOR_MACEND, &Preprocessor::_macend},
			{Token::PREPROCESSOR_INVOKE, &Preprocessor::_invoke},
			{Token::PREPROCESSOR_DEFINE, &Preprocessor::_define},
			{Token::PREPROCESSOR_IFDEF, &Preprocessor::_ifdef},
			{Token::PREPROCESSOR_IFNDEF, &Preprocessor::_ifndef},
			{Token::PREPROCESSOR_ELSE, &Preprocessor::_else},
			{Token::PREPROCESSOR_ELSEDEF, &Preprocessor::_elsedef},
			{Token::PREPROCESSOR_ELSENDEF, &Preprocessor::_elsendef},
			{Token::PREPROCESSOR_ENDIF, &Preprocessor::_endif},
			{Token::PREPROCESSOR_UNDEF, &Preprocessor::_undefine}
		};
};


#endif