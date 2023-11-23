#include <../src/AssemblerV3/File.h>

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

		Preprocessor(File file);					// constructs a preprocessor object with the given file
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


		File* inputFile;							// the input file
		File* outputFile;							// the output file
		State state;								// the state of the preprocessor

		FileReader* reader;							// reader for the input file
		FileWriter* writer;							// writer for the output file

		std::string currentPreprocessedToken;		// current token being preproceesed
		std::map<std::string, std::string> symbols;	// defined symbols
		std::map<std::string, Macro> macros;		// defined macros

		void preprocessToken();

		void _include();
		void _macro();
		void _macret();
		void _macend();
		void _invoke();
		void _define();
		void _ifdef();
		void _ifndef();
		void _else();
		void _elsedef();
		void _elsendef();
		void _endif();
		void _undef();

		typedef void (Preprocessor::*PreprocessorFunction)();
		std::map<std::string,PreprocessorFunction> preprocessorDirectives = {
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
			{"#undef", &Preprocessor::_undef}
		};
};


#endif