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

		Preprocessor(File file);
		~Preprocessor();

		void preprocess();
		State getState();
	private:
		File* inputFile;
		File* outputFile;
		State state;

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
			{"include", &Preprocessor::_include},
			{"macro", &Preprocessor::_macro},
			{"macret", &Preprocessor::_macret},
			{"macend", &Preprocessor::_macend},
			{"invoke", &Preprocessor::_invoke},
			{"define", &Preprocessor::_define},
			{"ifdef", &Preprocessor::_ifdef},
			{"ifndef", &Preprocessor::_ifndef},
			{"else", &Preprocessor::_else},
			{"elsedef", &Preprocessor::_elsedef},
			{"elsendef", &Preprocessor::_elsendef},
			{"endif", &Preprocessor::_endif},
			{"undef", &Preprocessor::_undef}
		};
};


#endif