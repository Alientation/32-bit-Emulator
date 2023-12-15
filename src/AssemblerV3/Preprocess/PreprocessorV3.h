#include <../src/util/File.h>
#include <../src/AssemblerV3/Build.h>
#include <../src/AssemblerV3/Tokenizer.h>

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
        struct Argument {
            std::string name;
            Tokenizer::Type type;

            Argument(std::string name, Tokenizer::Type type) {
                this->name = name;
                this->type = type;
            }

            Argument(std::string name) {
                this->name = name;
                this->type = Tokenizer::UNKNOWN;
            }
        };

		struct Macro {
            std::string name;
            std::vector<Argument> arguments;
            Tokenizer::Type returnType;

            std::vector<Tokenizer::Token> definition;

            Macro(std::string name) {
                this->name = name;
                this->returnType = Tokenizer::UNKNOWN;
            }

            std::string toString() {
                std::string toString = header() + "\n";
                for (auto i = 0; i < arguments.size(); i++) {
                    toString += "[" + std::to_string(i) + "]: " + arguments[i].name + ": " + Tokenizer::TYPE_TO_NAME_MAP.at(arguments[i].type);
                }
                toString += "-> " + Tokenizer::TYPE_TO_NAME_MAP.at(returnType) + "\n{\n";

                for (int i = 0; i < definition.size(); i++) {
                    toString += definition[i].value;
                }

                return toString + "\n}";
            }

            std::string header() {
                std::string header;
                header += name + "@(";

                for (auto i = 0; i < arguments.size(); i++) {
                    header += Tokenizer::TYPE_TO_NAME_MAP.at(arguments[i].type);
                    if (i < arguments.size() - 1) {
                        header += ",";
                    }
                }  

                return header + "):" + Tokenizer::TYPE_TO_NAME_MAP.at(returnType);
            }
		};

		Process* process;			// the build process

		File* inputFile;			// the input file
		File* outputFile;			// the output file
		State state;								// the state of the preprocessor
		std::vector<Tokenizer::Token> tokens;					// the tokens of the input file

		FileWriter* writer;			// writer for the output file

		std::map<std::string, std::string> symbols;	// defined symbols
		std::map<std::string, Macro> macros;		// defined macros

		void skipTokens(int& tokenI, const std::string& regex);
        void skipTokens(int& tokenI, const std::set<Tokenizer::Type>& tokenTypes);
		bool expectToken(int& tokenI, const std::string& errorMsg);
        bool expectToken(int& tokenI, const std::set<Tokenizer::Type>& tokenTypes, const std::string& errorMsg);
        bool isToken(int& tokenI, const std::set<Tokenizer::Type>& tokenTypes, const std::string& errorMsg = "Preprocessor::isToken() - Unexpected end of file");
        Tokenizer::Token& consume(int& tokenI, const std::string& errorMsg = "Preprocessor::consume() - Unexpected end of file");
        Tokenizer::Token& consume(int& tokenI, const std::set<Tokenizer::Type>& expectedTypes, const std::string& errorMsg = "Preprocessor::consume() - Unexpected token");

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
		std::map<Tokenizer::Type,PreprocessorFunction> preprocessors = {
			{Tokenizer::PREPROCESSOR_INCLUDE, &Preprocessor::_include},
			{Tokenizer::PREPROCESSOR_MACRO, &Preprocessor::_macro},
			{Tokenizer::PREPROCESSOR_MACRET, &Preprocessor::_macret},
			{Tokenizer::PREPROCESSOR_MACEND, &Preprocessor::_macend},
			{Tokenizer::PREPROCESSOR_INVOKE, &Preprocessor::_invoke},
			{Tokenizer::PREPROCESSOR_DEFINE, &Preprocessor::_define},
			{Tokenizer::PREPROCESSOR_IFDEF, &Preprocessor::_ifdef},
			{Tokenizer::PREPROCESSOR_IFNDEF, &Preprocessor::_ifndef},
			{Tokenizer::PREPROCESSOR_ELSE, &Preprocessor::_else},
			{Tokenizer::PREPROCESSOR_ELSEDEF, &Preprocessor::_elsedef},
			{Tokenizer::PREPROCESSOR_ELSENDEF, &Preprocessor::_elsendef},
			{Tokenizer::PREPROCESSOR_ENDIF, &Preprocessor::_endif},
			{Tokenizer::PREPROCESSOR_UNDEF, &Preprocessor::_undefine}
		};
};


#endif