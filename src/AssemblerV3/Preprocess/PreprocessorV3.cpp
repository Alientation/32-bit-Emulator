#include "PreprocessorV3.h"

#include "Logger.h"


/**
 * Constructs a preprocessor object with the given file.
 * 
 * @param file the file to preprocess
 */
Preprocessor::Preprocessor(Process* process, File* file) {
	this->process = process;
	this->inputFile = file;
	this->outputFile = new File(file->getFileName(), "i", file->getFileDirectory());

	if (!process->isValidSourceFile(inputFile)) {
		log(ERROR, std::stringstream() << "Preprocessor::Preprocessor() - Invalid source file: " << inputFile->getExtension());
		return;
	}

	state = State::UNPROCESSED;

	reader = new FileReader(inputFile);
	writer = new FileWriter(outputFile);
}

/**
 * Destructs a preprocessor object
 */
Preprocessor::~Preprocessor() {
	inputFile->~File();
}

/**
 * Preprocesses the file
 */
void Preprocessor::preprocess() {
	if (state != State::UNPROCESSED) {
		log(ERROR, std::stringstream() << "Preprocessor::preprocess() - Preprocessor is not in the UNPROCESSED state");
		return;
	}
	state = State::PROCESSING;

	// processes tokens from the input file
	curToken = "";
	while (reader->hasNextByte()) {
		char byte = reader->readByte();
		if (std::isspace(byte)) {
			preprocessToken();
			writer->writeByte(byte);
		} else if (!reader->hasNextByte()) {
			preprocessToken();
		} else {
			curToken += byte;
		}
	}

	state = State::PROCESSED_SUCCESS;
	writer->close();
	reader->close();
}

/**
 * Processes a token extracted from the preprocessor. A token is a sequence of characters that
 * is separated by whitespace.
 */
void Preprocessor::preprocessToken() {
	if (directives.find(curToken) != directives.end()) {
		(this->*directives[curToken])();
	} else if (symbols.find(curToken) != symbols.end()) {
		writer->writeString(symbols[curToken]);
	} else {
		writer->writeString(curToken);
	}

	curToken.clear();
}


/**
 * Inserts the file contents into the current file.
 * 
 * USAGE: #include "filename"|<filename>
 * 
 * "filename": looks for files located in the current directory.
 * <filename>: prioritizes files located in the include directory, if not found, looks in the
 * current directory.
 */
void Preprocessor::_include() {

}

/**
 * Defines a macro symbol with n arguments and optionally a return type.
 * 
 * USAGE: #macro [symbol]([arg1 ?: TYPE, arg2 ?: TYPE,..., argn ?: TYPE]) ?: TYPE
 * 
 * If a return type is specified and the macro definition does not return a value an error is thrown.
 * There cannot be a macro definition within this macro definition.
 * Note that the macro symbol is separate from label symbols and will not be pressent after preprocessing.
 */
void Preprocessor::_macro() {

}

/**
 * Stops processing the macro and returns the value of the expression.
 * 
 * USAGE: #macret [?expression]
 * 
 * If the macro does not have a return type the macret must return nothing.
 * If the macro has a return type the macret must return a value of that type
 */
void Preprocessor::_macret() {

}

/**
 * Closes a macro definition.
 * 
 * USAGE: #macend
 * 
 * If a macro is not closed an error is thrown.
 */
void Preprocessor::_macend() {

}

/**
 * Invokes the macro with the given arguments.
 * 
 * USAGE: #invoke [symbol]([arg1, arg2,..., argn]) [?symbol]
 * 
 * If provided an output symbol, the symbol will be associated with the return value of the macro.
 * If the macro does not return a value but an output symbol is provided, an error is thrown.
 */
void Preprocessor::_invoke() {

}

/**
 * Associates the symbol with a value
 * 
 * USAGE: #define [symbol] [?value]
 * 
 * Replaces all instances of symbol with the value
 * If value is not specified, the default is 0
 */
void Preprocessor::_define() {

}

/**
 * Determines whether to include the following text based on whether the symbol is defined
 * 
 * USAGE: #ifdef [symbol]
 * 
 * Must be closed by a #endif
 */
void Preprocessor::_ifdef() {

}

/**
 * Determines whether to include the following text based on whether the symbol is not defined
 * 
 * USAGE: #ifndef [symbol]
 * 
 * Must be closed by a #endif
 */
void Preprocessor::_ifndef() {

}

/**
 * Counterpart to #ifdef and #ifndef, only includes the following text if the previous #ifdef or #ifndef
 * was not included
 * 
 * USAGE: #else
 * 
 * Must be preceded by a #ifdef or #ifndef and closed by a #endif
 */
void Preprocessor::_else() {

}

/**
 * Counterpart to #ifdef and #ifndef, only includes the following text if the previous #ifdef or #ifndef
 * was not included and the symbol is defined
 * 
 * USAGE: #elsedef [symbol]
 * 
 * Must be preceded by a #ifdef or #ifndef and closed by a #endif
 */
void Preprocessor::_elsedef() {

}

/**
 * Counterpart to #ifdef and #ifndef, only includes the following text if the previous #ifdef or #ifndef
 * was not included and the symbol is not defined
 * 
 * USAGE: #elsendef [symbol]
 * 
 * Must be preceded by a #ifdef or #ifndef and closed by a #endif
 */
void Preprocessor::_elsendef() {

}

/**
 * Closes a #ifdef, #ifndef, #else, #elsedef, or #elsendef
 * 
 * USAGE: #endif
 * 
 * Must be preceded by a #ifdef, #ifndef, #else, #elsedef, or #elsendef
 */
void Preprocessor::_endif() {

}

/**
 * Undefines a symbol defined by #define
 * 
 * USAGE: #undefine [symbol]
 * 
 * This will still work if the symbol was never defined previously
 */
void Preprocessor::_undefine() {

}


/**
 * Returns the state of the preprocessor
 * 
 * @return the state of the preprocessor
 */
Preprocessor::State Preprocessor::getState() {
	return state;
}