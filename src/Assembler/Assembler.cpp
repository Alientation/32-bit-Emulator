#include "Assembler.h"

int main() {
	std::vector<std::string> files = {"..\\src\\Assembly\\temp.basm"};
	Assembler assembler(files);
}


Assembler::Assembler(std::vector<std::string> files) {
	this->files = files;

	// tokenize each file
	for (std::string file : files) {
		tokenize(file);
	}

	preprocess();

	// parse each file the first time to build symbol table
	for (std::string file : files) {
		parse(file);
	}

	// link each file
	linker();

	// assemble each file
	assemble();
}


/**
 * Creates a label at the correct scope and maps it to the symbol table.
 * 
 * @param labelname The name of the label to create.
 */
void Assembler::defineLabel(std::string labelname, Word value) {
	// check if label has already been defined in the current scope
	Scope& tempScope = *(this->currentScope);
	while(true) {
		if (tempScope.symbols.find(labelname) != tempScope.symbols.end()) {
			error(MULTIPLE_DEFINITION_ERROR, std::stringstream() << "Label Already Defined: " << labelname);
		}

		// we are at the filescope and the label has not been defined
		if (tempScope.parent == nullptr) {
			break;
		}
	}

	// create a new label
	tempScope.symbols.insert(std::pair<std::string, Symbol*>(labelname, new Symbol(labelname, value, SYMBOL_VALUE_RELATIVE)));
}


/**
 * Creates a new local scope with the current scope being the parent of the new scope
 * 
 * @throws INTERNAL_ERROR If the current token has already been processed. This means we are processing tokens that have already been processed.
 */
void Assembler::startScope() {
	// check if scope has already been defined before. This means we are processing 
	// tokens that have already been processed.
	if (currentObjectFile->scopeMap.find(currentTokenI) != currentObjectFile->scopeMap.end()) {
		error(INTERNAL_ERROR, std::stringstream() << "Scope has already been processed " 
			<< currentObjectFile->tokens[currentTokenI].errorstring());
	}

	// create a new local scope
	Scope* localScope = new Scope(currentScope);
	currentObjectFile->scopeMap[currentTokenI] = localScope;
	currentScope = localScope;
}

/**
 * Ends the current scope and returns back to its parent scope.
 * 
 * @throws INVALID_TOKEN_ERROR If the current scope has no parent scope. This means we are at the global scope which has
 * no parent scope.
 */
void Assembler::endScope() {
	// check if the current scope has no parent scope. This means we are at the global scope which has
    // no parent scope.
	if (currentScope->parent == nullptr) {
		error(INVALID_TOKEN_ERROR, std::stringstream() << "Scope Mismatch: Scope Not Opened " 
			<< currentObjectFile->tokens[currentTokenI].errorstring());
	}
	
	// close out the current scope
	currentScope = currentScope->parent;
}


void Assembler::preprocess() {
	// expand out macros here before parsing to simplify complexities
	// therefore it is here that we need to look for included files, marked extern/global macros, and map macros
	// remove original macro source from source file afterwards
	
	for (std::string filename : files) {
		currentObjectFile = objectFilesMap[filename];
		currentScope = currentObjectFile->filescope;

		int countNestedScopes = 0;

		// iterate through each token in the file
		for (currentTokenI = 0; currentTokenI < currentObjectFile->tokens.size(); currentTokenI++) {
			Token& token = currentObjectFile->tokens[currentTokenI];

			if (token.string == ".scope") {
				countNestedScopes++;
			} else if (token.string == ".scend") {
				countNestedScopes--;
			} else if (token.string == ".include" || token.string == ".macro" || token.string == ".global" || token.string == ".extern") {
				if (countNestedScopes > 0) {
					error(INTERNAL_ERROR, std::stringstream() << token.string << " Defined in Local Scope " << token.errorstring());
				}
				(this->*processDirective[directiveMap[token.string]])();
			}
		}
	}

	// fill in extern macros
	for (std::string filename : files) {
		currentObjectFile = objectFilesMap[filename];

		for (std::pair<std::string,std::set<int>> externMacro : currentObjectFile->markedExternMacros) {
			for (int parameterCount : externMacro.second) {
				// find the macro definition in one of the included files
				bool found = false;
				Macro* targetMacro = nullptr;
				for (std::string includedFile : currentObjectFile->includedFiles) {
					ObjectFile* includedObjectFile = objectFilesMap[includedFile];
					if (includedObjectFile->markedGlobalMacros.find(externMacro.first) == includedObjectFile->markedGlobalMacros.end()) {
						continue; // macro name not present
					} else if (includedObjectFile->markedGlobalMacros.at(externMacro.first).find(parameterCount) != includedObjectFile->markedGlobalMacros.at(externMacro.first).end()) {
						continue; // macro with specified parameter count not present
					}

					if (found) {
						error(MULTIPLE_DEFINITION_ERROR, std::stringstream() << "Extern Macro Defined in Multiple Files: " << externMacro.first);
					}
					found = true;
					targetMacro = includedObjectFile->filescope->macros.at(externMacro.first);

					// check if target macro exists
					if (targetMacro == nullptr) {
						error(INVALID_TOKEN_ERROR, std::stringstream() << "Extern Macro Not Defined: " << externMacro.first);
					} else if (targetMacro->macros.find(parameterCount) == targetMacro->macros.end()) { 
						// check if target macro has the correct number of parameters
						error(INVALID_TOKEN_ERROR, std::stringstream() << "Extern Macro Parameter Count Mismatch: " << externMacro.first);
					}
				}

				if (!found) {
					error(INVALID_TOKEN_ERROR, std::stringstream() << "Extern Macro Not Defined: " << externMacro.first);
				}

				// add the macro to the current object file
				currentObjectFile->filescope->macros.insert(std::pair<std::string, Macro*>(externMacro.first, targetMacro));
			}
		}
	}


	// expand out macros
	for (std::string filename : files) {
		currentObjectFile = objectFilesMap[filename];
		currentScope = currentObjectFile->filescope;

		for (currentTokenI = 0; currentTokenI < currentObjectFile->tokens.size(); currentTokenI++) {
			Token& token = currentObjectFile->tokens[currentTokenI];

			if (token.string == ".invoke") {
				(this->*processDirective[directiveMap[token.string]])();
			}
		}
	}

}


/**
 * First parse through tokens to construct symbol table and memory mappings.
 * 
 * 
 * 
 * @param filename The name of the file to parse.
 * 
 * @throws INTERNAL_ERROR If the file has not been tokenized, a token was empty, or a scope was not closed.
 * @throws UNRECOGNIZED_TOKEN_ERROR If a token is not a label, directive, or instruction.
 * @throws INVALID_TOKEN_ERROR If an instruction was defined in a non-text segment or an instruction had an invalid operand.
 */
void Assembler::parse(std::string filename) {
	// ensure file has been tokenized and mapped to an object file
	if (objectFilesMap.find(filename) == objectFilesMap.end()) {
		error(INTERNAL_ERROR, std::stringstream() << "File Not Tokenized: " << filename);
	}
	status = PARSING;

	// set tracker variables that allows some processes to be offloaded to other functions
	currentObjectFile = objectFilesMap[filename];
	currentScope = currentObjectFile->filescope;
	currentSegment = currentObjectFile->segmentMap[SEGMENT_TEXT][""];
	isRelativeMemory = true;


	// iterate through each token in the file
	for (currentTokenI = 0; currentTokenI < currentObjectFile->tokens.size(); currentTokenI++) {
		Token& token = currentObjectFile->tokens[currentTokenI];

		log(LOG, std::stringstream() << BOLD << BOLD_WHITE << "Token: " << token.string << RESET);

		// Tokens should not be empty. This is a sign of a bug in the tokenizer
		if (token.string.size() == 0) {
			error(INTERNAL_ERROR, std::stringstream() << "Empty Token " << token.errorstring());
		}

		// check if token is a label
		if (token.string.back() == ':') {
			// remove the ':' from the end of the label
			std::string labelName = token.string.substr(0, token.string.size() - 1);
			defineLabel(labelName, currentSegment->programCounter);
			continue;
		}
		
		// check if token is a directive
		if (directiveMap.find(token.string) != directiveMap.end()) {
			DirectiveType directiveType = directiveMap[token.string];

			// offload to some other function to do
			(this->*processDirective[directiveType])();
			
			// no other tokens should follow a directive in the same line
			EXPECT_NO_OPERAND();
			continue;
		}
		
		// check if token is a cpu instruction
		if (instructionMap.find(token.string) != instructionMap.end()) {
			// instructions must be in text segment
			if (currentSegment->type != SEGMENT_TEXT) {
				error(INVALID_TOKEN_ERROR, std::stringstream() << "Instruction must be defined in a TEXT segment " 
					<< token.errorstring());
			}

			// get operand if the instruction has any
			std::string operand = "";
			if (HAS_OPERAND()) {
				operand = currentObjectFile->tokens[++currentTokenI].string;
			}

			// check if operand is valid
			AddressingMode addressingMode = getAddressingMode(token.string, operand);
			if (addressingMode == NO_ADDRESSING_MODE) {
				error(INVALID_TOKEN_ERROR, std::stringstream() << "Invalid Operand Addressing Mode " << token.errorstring());
			}

			// track instruction in memory
			writeByte(instructionMap[token.string][addressingMode]);
			writeBytes(0, addressingModeOperandBytes[addressingMode]);

			// instruction should not have any additional operands or tokens following it
			EXPECT_NO_OPERAND();
			continue;
		}
		
		
		error(UNRECOGNIZED_TOKEN_ERROR, std::stringstream() << "Unrecognized Token: " << token.errorstring());
	}

	// check if all scopes were closed
	if (currentScope != currentObjectFile->filescope) {
		error(INTERNAL_ERROR, std::stringstream() << "Scope Mismatch: Scope Not Closed");
	}
}


void Assembler::linker() {
	// fill in values for symbols and macros
	status = LINKING;
}


void Assembler::assemble() {
	// write to memory mapping
	status = ASSEMBLING;
}

void Assembler::writeToFile() {

}



/**
 * Tokenize the file and store the tokens in the objectFilesMap.
 * 
 * @param filename The name of the file to tokenize.
 */
void Assembler::tokenize(std::string filename) {
	log(LOG, std::stringstream() << BOLD << BOLD_WHITE << "Reading File: " << filename << RESET);
	status = TOKENIZING;

	// ensure we have not already read this file
	if (objectFilesMap.find(filename) != objectFilesMap.end()) {
		error(MULTIPLE_DEFINITION_ERROR, std::stringstream() << "File Already Read: " << filename);
	}

	// read all characters from file to a string
    if (split(filename, '.').back() != "basm") {
        error(FILE_ERROR, std::stringstream() << "Unrecognized File Extension: " << split(filename, '.').back());
    }

    std::ifstream file(filename, std::ifstream::in);
    std::string sourceCode;

    // check if file exists
    if (!file) {
        error(FILE_ERROR, std::stringstream() << "File Does Not Exist: " << filename);
    }

    char mychar;
    while (file) {
        mychar = file.get();
        sourceCode += mychar;
    }
    file.close();

    log(LOG, std::stringstream() << BOLD << BOLD_WHITE << "Source Code\n" << RESET << sourceCode);
    log(LOG, std::stringstream() << BOLD << BOLD_GREEN << "Read File: " << filename << RESET);


	// now tokenize the file
	log(LOG_TOKENIZER, std::stringstream() << BOLD << BOLD_WHITE << "Tokenizing Source Code" << RESET);
	std::vector<Token> tokens;

	// current token being constructed
    std::string currentToken = "";

    // is the current token a comment and what type
    bool isSingleLineComment = false;
    bool isMultiLineComment = false;

    // the index of the first character from the original source code of the current token being constructed
    int currentTokenStart = -1;

    // the line the current token is on in the original source code
    int lineNumber = 1;

    // default true for first column on each line. Every subsequent column should have a preceeding tab character
    bool readyForNextToken = true;

    // iterate through each character in the source code
    for (int charLocation = 0; charLocation < sourceCode.size(); charLocation++) {
        char character = sourceCode[charLocation];
        
        // keep track of the current line number
        if (character == '\n') {
            lineNumber++;
        }

        // skip whitespace until we find a token to tokenize, this will trim any leading whitespace
        if (readyForNextToken && std::isspace(character)) {
            continue;
        }

        // add the current token to tokens
        currentToken += character;

        // check to end current built token
        if (!isMultiLineComment && (character == '\n' || character == '\t' || charLocation == sourceCode.size() - 1)) {
            // end current token, trim any trailing whitespace
            currentToken = trim(currentToken);

            // check if not a comment
            if (!isSingleLineComment) {
                tokens.push_back(Token(currentToken, currentTokenStart, character == '\n' ? lineNumber - 1 : lineNumber));
                log(LOG_TOKENIZER, std::stringstream() << CYAN << "Token\t" << RESET << "[" << currentToken << "]");
            } else {
                log(LOG_TOKENIZER, std::stringstream() << GREEN << "Comment\t" << RESET << "[" << currentToken << "]");
            }

            // reset current token
            currentToken.clear();
            currentTokenStart = -1;

            // prepare for next token
            readyForNextToken = true;
            
            // end single line comment if a new line was reached
            if (character == '\n') {
                isSingleLineComment = false;
            }
            continue;
        }

        // found first non-whitespace character of a token
        if (readyForNextToken) {
            // mark current character index
            currentTokenStart = charLocation;
            readyForNextToken = false;

            // mark token as a comment if it starts with a ';'
            if (character == ';') {
                isSingleLineComment = true;
            }
        }

        // check if token is start of multi line comment denoted by ;*
        if (isSingleLineComment && currentToken.size() == 2 && character == '*') {
            isMultiLineComment = true;
            isSingleLineComment = false;
        }

        // check if multi line comment is ending denoted by *;
        if (isMultiLineComment && currentToken.size() >= 4 && character == ';' 
            && currentToken[currentToken.size() - 2] == '*') {
            isMultiLineComment = false;
            
            std::vector<std::string> list = split(currentToken,'\n');
            log(LOG_TOKENIZER, std::stringstream() << GREEN << "Comments\t" << RESET << "[" << tostring(list) << "]");

            // end current token
            currentToken.clear();
            currentTokenStart = -1;

            readyForNextToken = true;
        }
    }

    // check if multi line comment was never closed
    if (isMultiLineComment) {
        error(MISSING_TOKEN_ERROR, std::stringstream() << "Multiline comment is not closed by \'*;\' ");
    }

    // check if current token has not been processed
    if (currentToken.size() != 0) {
        error(INTERNAL_ERROR, std::stringstream() << "Current token has not been processed");
    }

	// add list of tokens to map to filename
	objectFilesMap[filename] = new ObjectFile(tokens);

	log(LOG_TOKENIZER, std::stringstream() << BOLD << BOLD_GREEN << "Tokenized\t" << RESET << tostring(tokens));
}



/**
 * Simulates writing a byte to file by first tracking the byte in the assembler
 * 
 * @param value The value to write to the file
 */
void Assembler::writeByte(Byte value) {
	std::map<Word, MemorySegment*>& memoryMap = 
			isRelativeMemory ? currentSegment->relativeMemoryMap : currentSegment->absoluteMemoryMap;

	// write the byte
	// check if we are writing to a new memory segment
	Word currentProgramCounter = currentSegment->programCounter;
	if (memoryMap.find(currentProgramCounter - 1) == memoryMap.end()) {
        // check to make sure we are not overwriting other memory segments
        for (auto it = memoryMap.begin(); it != memoryMap.end(); it++) {
            MemorySegment& otherSegment = *(*it).second;
            if (otherSegment.startAddress <= currentProgramCounter && otherSegment.getEndAddress() >= currentProgramCounter) {
                // warn we are overwriting memory
                warn(WARN, std::stringstream() << "Overwriting Memory [" << otherSegment.prettyStringifyStartAddress() << "," 
                        << otherSegment.prettyStringifyEndAddress() << "] with [" << prettyStringifyValue(stringifyHex(currentProgramCounter)) << "]");
            }
        }

        // create a new memory segment
        MemorySegment* memorySegment = new MemorySegment(currentProgramCounter);
        memorySegment->bytes.push_back(value);
        memoryMap.insert(std::pair<Word, MemorySegment*>(currentProgramCounter, memorySegment));
    } else {
        // get previous memory segment that ends right before the current program counter
        MemorySegment& memorySegment = *memoryMap.at(currentProgramCounter - 1);
        memoryMap.erase(memoryMap.find(currentProgramCounter - 1));
        memorySegment.bytes.push_back(value);
        
        // check if we should combine memory segments if they are touching
        // loop through each memory segment
        for (auto it = memoryMap.begin(); it != memoryMap.end(); it++) {
            MemorySegment& otherSegment = *(*it).second;

            // check if the current memory segment is overwriting the another memory segment
            if (otherSegment.startAddress >= memorySegment.startAddress && otherSegment.startAddress <= currentProgramCounter) {
                // warn we are starting to overwrite memory
                warn(WARN, std::stringstream() << "Overwriting Memory [" << otherSegment.prettyStringifyStartAddress() << "," 
                        << otherSegment.prettyStringifyEndAddress() << "] with [" << 
                        memorySegment.prettyStringifyStartAddress() << "," << memorySegment.prettyStringifyEndAddress() << "]");
            }

            // check if the current memory segment is touching the next memory segment
            if (otherSegment.startAddress == currentProgramCounter + 1) {
                // combine the touching memory segments
                MemorySegment& nextMemorySegment = *(*it).second;
                memorySegment.bytes.insert(memorySegment.bytes.end(), nextMemorySegment.bytes.begin(), nextMemorySegment.bytes.end());
                
                // remove the other memory segment (the one that is following the current memory segment) from the memory map
                memoryMap.erase(it);
                break;
            }
        }

        // map the last byte of the current memory segment to the memory map
        memoryMap.insert(std::pair<Word, MemorySegment*>(memorySegment.getEndAddress(), &memorySegment));
    }

    // wrote one byte to current program counter
    currentSegment->programCounter++;
}

/**
 * Writes two bytes in specified endian order to file
 * 
 * @param value The value to write to the file
 * @param lowEndian If true, the low byte is written first, otherwise the high byte is written first
 */
void Assembler::writeTwoBytes(u16 value, bool lowEndian) {
    writeBytes(value, 2, lowEndian);
}

/**
 * Writes four bytes in specified endian order to file
 * 
 * @param value The value to write to the file
 * @param lowEndian If true, the low byte is written first, otherwise the high byte is written first
 */
void Assembler::writeWord(Word value, bool lowEndian) {
    writeBytes(value, 4, lowEndian);
}

/**
 * Writes four bytes in specified endian order to file
 * 
 * @param value The value to write to the file
 * @param lowEndian If true, the low byte is written first, otherwise the high byte is written first
 */
void Assembler::writeTwoWords(u64 value, bool lowEndian) {
    writeBytes(value, 8, lowEndian);
}

/**
 * Writes the specified number of bytes in specified endian order to file
 * 
 * @param value The value to write to the file
 * @param bytes The number of bytes to write
 * @param lowEndian If true, the low byte is written first, otherwise the high byte is written first
 */
void Assembler::writeBytes(u64 value, Byte bytes, bool lowEndian) {
    if (bytes > 8) {
        error(INTERNAL_ERROR, std::stringstream() << "Cannot write more than 8 bytes");
    } else if (bytes == 0) {
        warn(WARN, std::stringstream() << "Writing 0 bytes");
        return;
    }

    if (lowEndian) {
        for (int i = 0; i < bytes; i++) {
            writeByte(value & 0xFF);
            value >>= 8;
        }
    } else {
        u64 mask = 0xFF << (8 * (bytes - 1));
        for (int i = 0; i < bytes; i++) {
            writeByte((value & mask) >> (8 * (bytes - 1 - i)));
        }
    }
}