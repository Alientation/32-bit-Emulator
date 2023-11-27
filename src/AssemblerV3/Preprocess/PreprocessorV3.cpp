#include "PreprocessorV3.h"
#include <../src/util/Logger.h>

#include <regex>
#include <fstream>
#include <filesystem>

/**
 * Constructs a preprocessor object with the given file.
 * 
 * @param process the build process object.
 * @param file the file to preprocess.
 * @param outputFilePath the path to the output file, default is the inputfile path with .bi extension.
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

	Tokenizer::tokenize(inputFile, tokens);
}

/**
 * Destructs a preprocessor object.
 */
Preprocessor::~Preprocessor() {
    delete outputFile;
}

/**
 * Preprocesses the file.
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
		Tokenizer::Token& token = tokens[i];
        log(DEBUG, std::stringstream() << "Preprocessor::preprocess() - Processing token " << i << ": " << token.toString());
		
		if (directives.find(token.type) != directives.end()) {
			(this->*directives[token.type])(i);
		} else {
			writer->writeString(consume(i).value);
		}
	}

	state = State::PROCESSED_SUCCESS;
	writer->close();

	log(DEBUG, std::stringstream() << "Preprocessor::preprocess() - Preprocessed file: " << inputFile->getFileName());

    // log macros
    for (std::pair<std::string, Macro> macroPair : macros) {
        log(DEBUG, std::stringstream() << "Preprocessor::preprocess() - Macro: " << macroPair.second.toString());
    }
}


/**
 * Skips tokens that match the given regex.
 * 
 * @param regex the regex to match.
 * @param tokenI the index of the current token.
 */
void Preprocessor::skipTokens(int& tokenI, const std::string& regex) {
	while (tokenI < tokens.size() && std::regex_match(tokens[tokenI].value, std::regex(regex))) {
		tokenI++;
	}
}

/**
 * Skips tokens that match the given types.
 * 
 * @param tokenI the index of the current token.
 * @param tokenTypes the types to match.
 */
void Preprocessor::skipTokens(int& tokenI, const std::set<Tokenizer::Type>& tokenTypes) {
    while (tokenI < tokens.size() && tokenTypes.find(tokens[tokenI].type) != tokenTypes.end()) {
        tokenI++;
    }
}

/**
 * Expects the current token to exist.
 * 
 * @param tokenI the index of the expected token.
 * @param errorMsg the error message to throw if the token does not exist.
 */
bool Preprocessor::expectToken(int& tokenI, const std::string& errorMsg) {
	if (tokenI >= tokens.size()) {
		log(ERROR, std::stringstream() << errorMsg);
	}
    return true;
}

bool Preprocessor::expectToken(int& tokenI, const std::set<Tokenizer::Type>& expectedTypes, const std::string& errorMsg) {
    if (tokenI >= tokens.size()) {
        log(ERROR, std::stringstream() << errorMsg);
    } else if (expectedTypes.find(tokens[tokenI].type) == expectedTypes.end()) {
        log(ERROR, std::stringstream() << errorMsg);
    }
    return true;
}

/**
 * Returns whether the current token matches the given types.
 * 
 * @param tokenI the index of the current token.
 * @param tokenTypes the types to match.
 * 
 * @return true if the current token matches the given types.
 */
bool Preprocessor::isToken(int& tokenI, const std::set<Tokenizer::Type>& tokenTypes, const std::string& errorMsg) {
    expectToken(tokenI, errorMsg);
    return tokenTypes.find(tokens[tokenI].type) != tokenTypes.end();
}

/**
 * Consumes the current token.
 * 
 * @param tokenI the index of the current token.
 * @param errorMsg the error message to throw if the token does not exist.
 * 
 * @returns the value of the consumed token.
 */
Tokenizer::Token& Preprocessor::consume(int& tokenI, const std::string& errorMsg) {
    expectToken(tokenI, errorMsg);
    return tokens[tokenI++];
}

/**
 * Consumes the current token and checks it matches the given types.
 * 
 * @param tokenI the index of the current token.
 * @param expectedTypes the expected types of the token.
 * @param errorMsg the error message to throw if the token does not have the expected type.
 * 
 * @returns the value of the consumed token.
 */
Tokenizer::Token& Preprocessor::consume(int& tokenI, const std::set<Tokenizer::Type>& expectedTypes, const std::string& errorMsg) {
    expectToken(tokenI, errorMsg);
    if (expectedTypes.find(tokens[tokenI].type) == expectedTypes.end()) {
        log(ERROR, std::stringstream() << errorMsg);
    }
    return tokens[tokenI++];
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
 * @param tokenI the index of the include token.
 */
void Preprocessor::_include(int& tokenI) {
	consume(tokenI);
	skipTokens(tokenI, "[ \t]");
	expectToken(tokenI, "Preprocessor::_include() - Missing include filename.");

	std::string fullPathFromWorkingDirectory;

    if (tokens[tokenI].type == Tokenizer::LITERAL_STRING) {
        // local include
        std::string localFilePath = tokens[tokenI].value.substr(1, tokens[tokenI].value.length() - 2);
		fullPathFromWorkingDirectory = inputFile->getFileDirectory() + File::SEPARATOR + localFilePath;
    } else {
        // expect <"...">
        consume(tokenI, {Tokenizer::OPERATOR_LOGICAL_LESS_THAN}, "Preprocessor::_include() - Missing '<'.");
        std::string systemFilePath = consume(tokenI, {Tokenizer::LITERAL_STRING}, "Preprocessor::_include() - Expected string literal.").value;
        consume(tokenI, {Tokenizer::OPERATOR_LOGICAL_GREATER_THAN}, "Preprocessor::_include() - Missing '>'.");

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
 * @param tokenI The index of the macro token.
 */
void Preprocessor::_macro(int& tokenI) {
	consume(tokenI);
	skipTokens(tokenI, "[ \t]");
	std::string macroName = consume(tokenI, {Tokenizer::SYMBOL}, "Preprocessor::_macro() - Expected macro name.").value;
    Macro macro = Macro(macroName);

    // start of declaration arguments
	skipTokens(tokenI, "[ \t\n]");
	consume(tokenI, {Tokenizer::OPEN_PARANTHESIS}, "Preprocessor::_macro() - Expected '('.");

    // parse arguments
    while (!isToken(tokenI, {Tokenizer::CLOSE_PARANTHESIS}, "Preprocessor::_macro() - Expected macro header.")) {
        skipTokens(tokenI, "[ \t\n]");
        std::string argName = consume(tokenI, {Tokenizer::SYMBOL}, "Preprocessor::_macro() - Expected argument name.").value;

        // parse argument type if there is one
        skipTokens(tokenI, "[ \t\n]");
        if (isToken(tokenI, {Tokenizer::COLON})) {
            consume(tokenI);
            skipTokens(tokenI, "[ \t\n]");
            macro.arguments.push_back(Argument(argName, consume(tokenI, Tokenizer::VARIABLE_TYPES, "Preprocessor::_macro() - Expected argument type").type));
        } else {
            macro.arguments.push_back(Argument(argName));
        }

        // parse comma or expect closing parenthesis
        skipTokens(tokenI, "[ \t\n]");
        if (isToken(tokenI, {Tokenizer::COMMA})) {
            consume(tokenI);
        } else {
            expectToken(tokenI, {Tokenizer::CLOSE_PARANTHESIS}, "Preprocessor::_macro() - Expected ')'.");
            break;
        }
    }

    // consume the closing parenthesis
    consume(tokenI, {Tokenizer::CLOSE_PARANTHESIS}, "Preprocessor::_macro() - Expected ')'.");
    skipTokens(tokenI, "[ \t\n]");

    // parse return type if there is one
    if (isToken(tokenI, {Tokenizer::COLON})) {
        consume(tokenI);
        skipTokens(tokenI, "[ \t\n]");
        macro.returnType = consume(tokenI, Tokenizer::VARIABLE_TYPES, "Preprocessor::_macro() - Expected return type.").type;
    }

    // parse macro definition
    while (!isToken(tokenI, {Tokenizer::PREPROCESSOR_MACEND}, "Preprocessor::_macro() - Expected macro definition." )) {
        macro.definition.push_back(consume(tokenI));
    }
    consume(tokenI, {Tokenizer::PREPROCESSOR_MACEND}, "Preprocessor::_macro() - Expected '#macend'.");

    // check if macro declaration is unique
    if (macros.find(macro.header()) != macros.end()) {
        log(ERROR, std::stringstream() << "Preprocessor::_macro() - Macro already defined: " << macro.header());
        return;
    }

    // add macro to list of macros
    macros.insert(std::pair<std::string,Macro>(macro.header(), macro));
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
    // should never reach this. This should be consumed by the _macro function.
    log(ERROR, std::stringstream() << "Preprocessor::_macend() - Unexpected macro end token.");
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
 * Replaces all instances of symbol with the value.
 * If value is not specified, the default is 0.
 * 
 * @param tokenI The index of the define token.
 */
void Preprocessor::_define(int& tokenI) {

}

/**
 * Determines whether to include the following text based on whether the symbol is defined.
 * 
 * USAGE: #ifdef [symbol]
 * 
 * Must be closed by a #endif
 * 
 * @param tokenI The index of the ifdef token.
 */
void Preprocessor::_ifdef(int& tokenI) {

}

/**
 * Determines whether to include the following text based on whether the symbol is not defined.
 * 
 * USAGE: #ifndef [symbol]
 * 
 * Must be closed by a #endif.
 * 
 * @param tokenI The index of the ifndef token.
 */
void Preprocessor::_ifndef(int& tokenI) {

}

/**
 * Counterpart to #ifdef and #ifndef, only includes the following text if the previous #ifdef or #ifndef
 * was not included.
 * 
 * USAGE: #else
 * 
 * Must be preceded by a #ifdef or #ifndef and closed by a #endif.
 * 
 * @param tokenI The index of the else token.
 */
void Preprocessor::_else(int& tokenI) {

}

/**
 * Counterpart to #ifdef and #ifndef, only includes the following text if the previous #ifdef or #ifndef
 * was not included and the symbol is defined.
 * 
 * USAGE: #elsedef [symbol]
 * 
 * Must be preceded by a #ifdef or #ifndef and closed by a #endif.
 * 
 * @param tokenI The index of the elsedef token.
 */
void Preprocessor::_elsedef(int& tokenI) {

}

/**
 * Counterpart to #ifdef and #ifndef, only includes the following text if the previous #ifdef or #ifndef
 * was not included and the symbol is not defined.
 * 
 * USAGE: #elsendef [symbol]
 * 
 * Must be preceded by a #ifdef or #ifndef and closed by a #endif.
 * 
 * @param tokenI The index of the elsendef token.
 */
void Preprocessor::_elsendef(int& tokenI) {

}

/**
 * Closes a #ifdef, #ifndef, #else, #elsedef, or #elsendef.
 * 
 * USAGE: #endif
 * 
 * Must be preceded by a #ifdef, #ifndef, #else, #elsedef, or #elsendef.
 * 
 * @param tokenI The index of the endif token.
 */
void Preprocessor::_endif(int& tokenI) {

}

/**
 * Undefines a symbol defined by #define.
 * 
 * USAGE: #undefine [symbol]
 * 
 * This will still work if the symbol was never defined previously.
 * 
 * @param tokenI The index of the undefine token.
 */
void Preprocessor::_undefine(int& tokenI) {

}


/**
 * Returns the state of the preprocessor.
 * 
 * @return the state of the preprocessor.
 */
Preprocessor::State Preprocessor::getState() {
	return state;
}