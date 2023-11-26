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
Preprocessor::Preprocessor(Process* process, File* file, std::string outputFilePath) {
	this->process.reset(process);
	this->inputFile.reset(file);

	if (outputFilePath.empty()) {
		this->outputFile.reset(new File(file->getFileName(), PROCESSED_EXTENSION, file->getFileDirectory()));
	} else {
		this->outputFile.reset(new File(outputFilePath));
	}

	if (!process->isValidSourceFile(inputFile.get())) {
		log(ERROR, std::stringstream() << "Preprocessor::Preprocessor() - Invalid source file: " << inputFile->getExtension());
		return;
	}

	state = State::UNPROCESSED;
	writer.reset(new FileWriter(outputFile.get()));

	// tokenize();
	new_tokenize();
}

/**
 * Newer tokenizer
 */
void Preprocessor::new_tokenize() {
	log(DEBUG, std::stringstream() << "Preprocessor::Preprocessor() - Tokenizing file: " << inputFile->getFileName());
	if (state != State::UNPROCESSED) {
		log(ERROR, std::stringstream() << "Preprocessor::Preprocessor() - Preprocessor is not in the UNPROCESSED state");
		return;
	}
	std::shared_ptr<FileReader> reader(new FileReader(inputFile.get()));
	std::string source_code = reader->readAll();
	reader->close();

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
				break;
			}
		}

		if (!matched) {
			log(ERROR, std::stringstream() << "Preprocessor::Preprocessor() - Could not match regex to source code: " << source_code);
			return;
		}
	}

	// print out tokens
	log(DEBUG, std::stringstream() << "Preprocessor::Preprocessor() - Tokenized file: " << inputFile->getFileName());
	for (int i = 0; i < tokens.size(); i++) {
		log(DEBUG, std::stringstream() << "Preprocessor::Preprocessor() - Token[" << i << "]=" << tokens[i].toString());
	}
}

/**
 * Tokenizes the input file. This is an internal function.
 */
void Preprocessor::tokenize() {
	log(DEBUG, std::stringstream() << "Preprocessor::Preprocessor() - Tokenizing file: " << inputFile->getFileName());

	if (state != State::UNPROCESSED) {
		log(ERROR, std::stringstream() << "Preprocessor::Preprocessor() - Preprocessor is not in the UNPROCESSED state");
		return;
	}

	std::shared_ptr<FileReader> reader(new FileReader(inputFile.get()));

	// tokenizes the input file
	std::string tok = "";
	bool isQuoted = false;
	char quotedChar = '"';

	// read the file
	while (reader->hasNextByte()) {
		char byte = reader->readByte();

		// handle quotes
		if (!isQuoted && (byte == '"' || byte == '<')) { // NOTE: this does not work with escape characters
			isQuoted = true;
			quotedChar = byte;
		} else if (isQuoted && byte == quotedChar) {
			isQuoted = false;
		}

		if (std::isspace(byte) && !isQuoted) {
			// whitespace character not surrounded by quotes
			tokens.push_back(Token(Token::Type::TEXT, tok));
			tokens.push_back(Token(Token::Type::WHITESPACE, std::string(1, byte)));
			tok = "";
		} else if (!reader->hasNextByte()) {
			tok += byte;
			tokens.push_back(Token(Token::Type::TEXT, tok));
		} else {
			tok += byte;
		}
	}

	// unclosed quotes
	if (isQuoted) {
		log(ERROR, std::stringstream() << "Preprocessor::Preprocessor() - Unclosed quotes: " << quotedChar);
		return;
	}

	// print out tokens
	log(DEBUG, std::stringstream() << "Preprocessor::Preprocessor() - Tokenized file: " << inputFile->getFileName());
	for (int i = 0; i < tokens.size(); i++) {
		Token& token = tokens[i];

		if (token.type == token.WHITESPACE) {
			log(DEBUG, std::stringstream() << "Preprocessor::Preprocessor() - Token[" << i << "]: " << std::to_string(token.value[0]) << " (" << token.type << ")");
		} else {
			log(DEBUG, std::stringstream() << "Preprocessor::Preprocessor() - Token[" << i << "]: " << token.value << " (" << token.type << ")");
		}
	}

	reader->close();
}


/**
 * Destructs a preprocessor object
 */
Preprocessor::~Preprocessor() {
	inputFile->~File();
	outputFile->~File();
	writer->~FileWriter();
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

	// parses the tokens
	for (int i = 0; i < tokens.size(); i++) {
		Token& token = tokens[i];
		
		if (token.type == token.WHITESPACE) {
			writer->writeString(token.value);
		} else if (token.type == token.TEXT) {
			if (token.value[0] == '#' && directives.find(token.value) != directives.end()) {
				// this is a preprocessor directive
				(this->*directives[token.value])(i);
			} else {
				// this is not a preprocessor directive
				writer->writeString(token.value);
			}
		} else {
			log(ERROR, std::stringstream() << "Preprocessor::preprocess() - Invalid token type: " << token.type);
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

	std::string filepath = tokens[tokenI].value;
	std::string fullPathFromWorkingDirectory;

	if (filepath[0] == '"') {
		// local include
		if (filepath.back() != '"') {
			log(ERROR, std::stringstream() << "Preprocessor::_include() - Unclosed quotes around filepath: " << filepath);
			return;
		}

		std::string localFilePath = filepath.substr(1, filepath.length() - 2);
		if (!File::isValidFilePath(localFilePath)) {
			log(ERROR, std::stringstream() << "Preprocessor::_include() - Invalid include filepath: " << localFilePath);
			return;		
		}

		fullPathFromWorkingDirectory = inputFile->getFileDirectory() + File::SEPARATOR + localFilePath;
	} else if (filepath[0] == '<') {
		// this is a system include
		if (filepath.back() != '>') {
			log(ERROR, std::stringstream() << "Preprocessor::_include() - Unclosed angle brackets around filepath: " << filepath);
			return;
		}

		std::string systemFilePath = filepath.substr(1, filepath.length() - 2);
		
		// check if the system file path exists relative to any of the system directories passed to the build process
	} else {
		log(ERROR, std::stringstream() << "Preprocessor::_include() - Invalid include filepath. Missing quotes: " << filepath);
		return;
	}

	// process included file
	std::shared_ptr<File> includeFile(new File(fullPathFromWorkingDirectory));
	if (!includeFile->exists()) {
		log(ERROR, std::stringstream() << "Preprocessor::_include() - Include file does not exist: " << fullPathFromWorkingDirectory);
		return;
	}

	// instead of writing all the contents to the output file, simply tokenize the file and insert into the current token list
	Preprocessor includedPreprocessor = Preprocessor(process.get(), includeFile.get(), outputFile->getFilePath());
	
	// yoink the tokens from the included file and insert
	tokens.insert(tokens.begin() + tokenI + 1, includedPreprocessor.tokens.begin(), includedPreprocessor.tokens.end());
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