#include <../src/AssemblerV3/File.h>
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

		Preprocessor(Process* process, File* file);	// constructs a preprocessor object with the given file
		~Preprocessor();							// destructs a preprocessor object

		void preprocess();							// preprocesses the file
		State getState();							// returns the state of the preprocessor
	
	private:
		struct Macro {
			struct Argument {
				std::string argName;
				std::string argType;
			};

			std::string macroName;
			std::vector<Argument> macroArguments;
			std::string returnType;

			std::string macroBody;
		};

		struct Token {
			enum Type {
				TEXT, WHITESPACE
			};

			Type type;
			std::string value;

			Token(Type type, std::string value) {
				this->type = type;
				this->value = value;
			}
		};

		Process* process;

		File* inputFile;							// the input file
		File* outputFile;							// the output file
		State state;								// the state of the preprocessor
		std::vector<Token> tokens;					// the tokens of the input file

		FileWriter* writer;							// writer for the output file

		std::map<std::string, std::string> symbols;	// defined symbols
		std::map<std::string, Macro> macros;		// defined macros

		void tokenize();
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