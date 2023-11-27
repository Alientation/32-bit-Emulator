#include "PreprocessorV3.h"
#include <../src/util/Logger.h>

#include <regex>
#include <fstream>
#include <filesystem>

/**
 * Constructs a preprocessor object with the given file.
 * 
 * @param process the build process object
 * @param file the file to preprocess
 * @param outputFilePath the path to the output file, default is the inputfile path with .bi extension
 */
Preprocessor::Preprocessor(Process* process, File* inputFile, std::string outputFilePath) {
    this->process = process;
    this->inputFile = inputFile;

	if (outputFilePath.empty()) {
        outputFile = new File(inputFile->getFileName(), PROCESSED_EXTENSION, inputFile->getFileDirectory(), true);
	} else {
        outputFile = new File(outputFilePath, true);
	}

	if (!process->isValidSourceFile(inputFile)) {
		log(ERROR, std::stringstream() << "Preprocessor::Preprocessor() - Invalid source file: " << inputFile->getExtension());
		return;
	}

	state = State::UNPROCESSED;

	tokenize();
}

/**
 * Tokenizes the input file into a list of tokens
 */
void Preprocessor::tokenize() {
	log(DEBUG, std::stringstream() << "Preprocessor::tokenize() - Tokenizing file: " << inputFile->getFileName());
	if (state != State::UNPROCESSED) {
		log(ERROR, std::stringstream() << "Preprocessor::tokenize() - Preprocessor is not in the UNPROCESSED state");
		return;
	}
	FileReader reader(inputFile);
	std::string source_code = reader.readAll();
	reader.close();

	tokens.clear();
	while (source_code.size() > 0) {
		// try to match regex
		bool matched = false;
		for (std::pair<std::string, Token::Type> regexPair : TOKEN_SPEC) {
			std::string regex = regexPair.first;
			Token::Type type = regexPair.second;
			std::regex token_regex(regex);
			std::smatch match;
			if (std::regex_search(source_code, match, token_regex)) {
				// matched regex
				std::string token_value = match.str();
				tokens.push_back(Token(type, token_value));
				source_code = match.suffix();
				matched = true;

                // log(LOG, std::stringstream() << "Preprocessor::tokenize() - Token " << tokens.size()-1 << ": " << tokens.back().toString());
				break;
			}
		}

		if (!matched) {
			log(ERROR, std::stringstream() << "Preprocessor::tokenize() - Could not match regex to source code: " << source_code);
			return;
		}
	}

	// print out tokens
	log(DEBUG, std::stringstream() << "Preprocessor::tokenize() - Tokenized file: " << inputFile->getFileName());
	for (int i = 0; i < tokens.size(); i++) {
		log(DEBUG, std::stringstream() << "Preprocessor::tokenize() - Token[" << i << "]=" << tokens[i].toString());
	}
}


/**
 * Destructs a preprocessor object
 */
Preprocessor::~Preprocessor() {
    delete outputFile;
}

/**
 * Preprocesses the file
 */
void Preprocessor::preprocess() {
	log(DEBUG, std::stringstream() << "Preprocessor::preprocess() - Preprocessing file: " << inputFile->getFileName());

	if (state != State::UNPROCESSED) {
		log(ERROR, std::stringstream() << "Preprocessor::preprocess() - Preprocessor is not in the UNPROCESSED state");
		return;
	}
	state = State::PROCESSING;

    // clearing output file
    std::ofstream ofs;
    ofs.open(outputFile->getFilePath(), std::ofstream::out | std::ofstream::trunc);
    ofs.close();

    // create writer
    writer = new FileWriter(outputFile);

	// parses the tokens
	for (int i = 0; i < tokens.size(); ) {
		Token& token = tokens[i];
        log(DEBUG, std::stringstream() << "Preprocessor::preprocess() - Processing token " << i << ": " << token.toString());
		
		if (directives.find(token.type) != directives.end()) {
			(this->*directives[token.type])(i);
		} else {
			writer->writeString(consume(i));
		}
	}

	state = State::PROCESSED_SUCCESS;
	writer->close();

	log(DEBUG, std::stringstream() << "Preprocessor::preprocess() - Preprocessed file: " << inputFile->getFileName());
}


/**
 * Skips tokens that match the given regex.
 * 
 * @param regex the regex to match
 * @param tokenI the index of the current token
 */
void Preprocessor::skipTokens(int& tokenI, std::string regex) {
	while (tokenI < tokens.size() && std::regex_match(tokens[tokenI].value, std::regex(regex))) {
		tokenI++;
	}
}

/**
 * Expects the next token to exist
 * 
 * @param tokenI the index of the expected token
 * @param errorMsg the error message to throw if the token does not exist
 */
void Preprocessor::expectToken(int& tokenI, std::string errorMsg) {
	if (tokenI >= tokens.size()) {
		log(ERROR, std::stringstream() << errorMsg);
	}
}

/**
 * Consumes the current token
 * 
 * @param tokenI the index of the current token
 * @param errorMsg the error message to throw if the token does not exist
 * 
 * @returns the value of the consumed token
 */
std::string Preprocessor::consume(int& tokenI, std::string errorMsg) {
    expectToken(tokenI, errorMsg);
    return tokens[tokenI++].value;
}

/**
 * Consumes the current token and checks it matches the given types
 * 
 * @param tokenI the index of the current token
 * @param expectedTypes the expected types of the token
 * @param errorMsg the error message to throw if the token does not have the expected type
 * 
 * @returns the value of the consumed token
 */
std::string Preprocessor::consume(int& tokenI, std::set<Token::Type> expectedTypes, std::string errorMsg) {
    expectToken(tokenI, errorMsg);
    if (expectedTypes.find(tokens[tokenI].type) == expectedTypes.end()) {
        log(ERROR, std::stringstream() << errorMsg);
    }
    return tokens[tokenI++].value;
}


/**
 * Inserts the file contents into the current file.
 * 
 * USAGE: #include "filepath"|<filepath>
 * 
 * "filepath": looks for files located in the current directory.
 * <filepath>: prioritizes files located in the include directory, if not found, looks in the
 * current directory.
 * 
 * @param tokenI the index of the include token
 */
void Preprocessor::_include(int& tokenI) {
	tokenI++;
	skipTokens(tokenI, "[ \t]");
	expectToken(tokenI, "Preprocessor::_include() - Missing include filename");

	std::string fullPathFromWorkingDirectory;

    if (tokens[tokenI].type == Token::LITERAL_STRING) {
        // local include
        std::string localFilePath = tokens[tokenI].value.substr(1, tokens[tokenI].value.length() - 2);
		fullPathFromWorkingDirectory = inputFile->getFileDirectory() + File::SEPARATOR + localFilePath;
    } else {
        // expect <"...">
        consume(tokenI, {Token::OPERATOR_LOGICAL_LESS_THAN}, "Preprocessor::_include() - Missing <");
        std::string systemFilePath = consume(tokenI, {Token::LITERAL_STRING}, "Preprocessor::_include() - Missing include filename");
        consume(tokenI, {Token::OPERATOR_LOGICAL_GREATER_THAN}, "Preprocessor::_include() - Missing >");

        // check if file exists in system include directories
        for (Directory* directory : process->getSystemDirectories()) {
            if (directory->subfileExists(systemFilePath)) {
                fullPathFromWorkingDirectory = directory->getDirectoryPath() + File::SEPARATOR + systemFilePath;
                break;
            }
        }
	}

	// process included file
	File* includeFile = new File(fullPathFromWorkingDirectory);
	if (!includeFile->exists()) {
		log(ERROR, std::stringstream() << "Preprocessor::_include() - Include file does not exist: " << fullPathFromWorkingDirectory);
		return;
	}

	// instead of writing all the contents to the output file, simply tokenize the file and insert into the current token list
	Preprocessor includedPreprocessor(process, includeFile, outputFile->getFilePath());
    delete includeFile;
	
	// yoink the tokens from the included file and insert
	tokens.insert(tokens.begin() + tokenI, includedPreprocessor.tokens.begin(), includedPreprocessor.tokens.end());
}

/**
 * Defines a macro symbol with n arguments and optionally a return type.
 * 
 * USAGE: #macro [symbol]([arg1 ?: TYPE, arg2 ?: TYPE,..., argn ?: TYPE]) ?: TYPE
 * 
 * If a return type is specified and the macro definition does not return a value an error is thrown.
 * There cannot be a macro definition within this macro definition.
 * Note that the macro symbol is separate from label symbols and will not be pressent after preprocessing.
 * 
 * @param tokenI The index of the macro token
 */
void Preprocessor::_macro(int& tokenI) {
	tokenI++;
	skipTokens(tokenI, "[ \t]");
	expectToken(tokenI, "Preprocessor::_macro() - Missing macro name");
	std::string macroName = tokens[tokenI].value;

	tokenI++;
	skipTokens(tokenI, "[ \t\n]");
	expectToken(tokenI, "Preprocessor::_macro() - Missing macro arguments");
}

/**
 * Stops processing the macro and returns the value of the expression.
 * 
 * USAGE: #macret [?expression]
 * 
 * If the macro does not have a return type the macret must return nothing.
 * If the macro has a return type the macret must return a value of that type
 * 
 * @param tokenI The index of the macro return token
 */
void Preprocessor::_macret(int& tokenI) {

}

/**
 * Closes a macro definition.
 * 
 * USAGE: #macend
 * 
 * If a macro is not closed an error is thrown.
 * 
 * @param tokenI The index of the macro end token
 */
void Preprocessor::_macend(int& tokenI) {

}

/**
 * Invokes the macro with the given arguments.
 * 
 * USAGE: #invoke [symbol]([arg1, arg2,..., argn]) [?symbol]
 * 
 * If provided an output symbol, the symbol will be associated with the return value of the macro.
 * If the macro does not return a value but an output symbol is provided, an error is thrown.
 * 
 * @param tokenI The index of the invoke token
 */
void Preprocessor::_invoke(int& tokenI) {

}

/**
 * Associates the symbol with a value
 * 
 * USAGE: #define [symbol] [?value]
 * 
 * Replaces all instances of symbol with the value
 * If value is not specified, the default is 0
 * 
 * @param tokenI The index of the define token
 */
void Preprocessor::_define(int& tokenI) {

}

/**
 * Determines whether to include the following text based on whether the symbol is defined
 * 
 * USAGE: #ifdef [symbol]
 * 
 * Must be closed by a #endif
 * 
 * @param tokenI The index of the ifdef token
 */
void Preprocessor::_ifdef(int& tokenI) {

}

/**
 * Determines whether to include the following text based on whether the symbol is not defined
 * 
 * USAGE: #ifndef [symbol]
 * 
 * Must be closed by a #endif
 * 
 * @param tokenI The index of the ifndef token
 */
void Preprocessor::_ifndef(int& tokenI) {

}

/**
 * Counterpart to #ifdef and #ifndef, only includes the following text if the previous #ifdef or #ifndef
 * was not included
 * 
 * USAGE: #else
 * 
 * Must be preceded by a #ifdef or #ifndef and closed by a #endif
 * 
 * @param tokenI The index of the else token
 */
void Preprocessor::_else(int& tokenI) {

}

/**
 * Counterpart to #ifdef and #ifndef, only includes the following text if the previous #ifdef or #ifndef
 * was not included and the symbol is defined
 * 
 * USAGE: #elsedef [symbol]
 * 
 * Must be preceded by a #ifdef or #ifndef and closed by a #endif
 * 
 * @param tokenI The index of the elsedef token
 */
void Preprocessor::_elsedef(int& tokenI) {

}

/**
 * Counterpart to #ifdef and #ifndef, only includes the following text if the previous #ifdef or #ifndef
 * was not included and the symbol is not defined
 * 
 * USAGE: #elsendef [symbol]
 * 
 * Must be preceded by a #ifdef or #ifndef and closed by a #endif
 * 
 * @param tokenI The index of the elsendef token
 */
void Preprocessor::_elsendef(int& tokenI) {

}

/**
 * Closes a #ifdef, #ifndef, #else, #elsedef, or #elsendef
 * 
 * USAGE: #endif
 * 
 * Must be preceded by a #ifdef, #ifndef, #else, #elsedef, or #elsendef
 * 
 * @param tokenI The index of the endif token
 */
void Preprocessor::_endif(int& tokenI) {

}

/**
 * Undefines a symbol defined by #define
 * 
 * USAGE: #undefine [symbol]
 * 
 * This will still work if the symbol was never defined previously
 * 
 * @param tokenI The index of the undefine token
 */
void Preprocessor::_undefine(int& tokenI) {

}


/**
 * Returns the state of the preprocessor
 * 
 * @return the state of the preprocessor
 */
Preprocessor::State Preprocessor::getState() {
	return state;
}