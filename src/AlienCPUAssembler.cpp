#include "AlienCPUAssembler.h"

#include <any>
#include <iostream>
#include <fstream>
#include <sstream>

int main() {
    AlienCPU cpu;
    AlienCPUAssembler assembler(cpu, true);

    assembler.assembleFile("..\\src\\Assembly\\temp.basm");
}


/**
 * Constructs a new AlienCPUAssembler for the given AlienCPU.
 * 
 * @param cpu The AlienCPU to assemble for.
 * @param debugOn Whether to print debug information.
 */
AlienCPUAssembler::AlienCPUAssembler(AlienCPU& cpu, bool debugOn) : cpu(cpu), debugOn(debugOn) {}


/**
 * Resets the internal state of the assembler.
 */
void AlienCPUAssembler::reset() {
    status = STARTING;
    outputFile = DEFAULT_OUTPUT_FILE;
    sourceCode.clear();
    tokens.clear();
    parsedTokens.clear();
    segmentName.clear();
    segmentType = DEFAULT_SEGMENT_TYPE;
    segments.clear();
    currentProgramCounter = DEFAULT_STARTING_ADDRESS;
}


/**
 * Writes the tracked bytes to a binary file 
 */
void AlienCPUAssembler::writeToFile() {
    // for now, just print memory map
    printMemoryMap();

    // write to file
}


/**
 * Assembles an assembly file source code into machine code and writes to a binary file.
 * 
 * @param filename The name of the assembly file to assemble.
 */
void AlienCPUAssembler::assembleFile(std::string filename) {
    log(LOG, std::stringstream() << BOLD << BOLD_WHITE << "Reading File: " << filename << RESET);

    // read all characters from file to a string
    if (split(filename, '.').back() != "basm") {
        error(FILE_ERROR, *NULL_TOKEN, std::stringstream() << "Unrecognized File Extension: " << split(filename, '.').back());
    }

    std::ifstream file(filename, std::ifstream::in);
    std::string source;

    // check if file exists
    if (!file) {
        error(FILE_ERROR, *NULL_TOKEN, std::stringstream() << "File Does Not Exist: " << filename);
    }

    char mychar;
    while (file) {
        mychar = file.get();
        source += mychar;
    }
    file.close();

    log(LOG, std::stringstream() << BOLD << BOLD_WHITE << "Source Code\n" << RESET << source);
    log(LOG, std::stringstream() << BOLD << BOLD_GREEN << "Read File: " << filename << RESET);

	// assemble the source code
	assemble(source);
}


/**
 * Assembles the given assembly source code into machine code and writes to a binary file.
 * 
 * Documentation on the assembly language can be found SOMEWHERE
 * 
 * @param source The assembly source code to assemble into machine code.
 */
void AlienCPUAssembler::assemble(std::string source) {
    log(LOG, std::stringstream() << BOLD << BOLD_WHITE << "Assembling..." << RESET);

    // reset assembler state to be ready for a new assembly
    reset();
    sourceCode = source;
    
    // convert source code into list of tokens
    status = TOKENIZING;
    tokenize();

    // print out each token
	log(LOG, std::stringstream() << BOLD << "Tokens" << RESET << "\n");
    int curLine = tokens.size() > 0 ? tokens[0].lineNumber : 1;
    for (Token& token : tokens) {
        if (token.lineNumber > curLine) {
            std::cout << std::endl;
        }
        std::cout << token.string << "\t";
        curLine = token.lineNumber;
    }
    std::cout << std::endl;
	log(LOG, std::stringstream() << BOLD << "END" << RESET);

    // parse each token to create label mappings
    status = PARSING;
    log(LOG_PARSING, std::stringstream() << BOLD << BOLD_WHITE << "Parsing Tokens" << RESET);
    passTokens();
    log(LOG_PARSING, std::stringstream() << BOLD << BOLD_GREEN << "Parsed Tokens" << RESET);

    // print out each parsed token and its associated memory address
    printParsedTokens();
    parsedTokens.clear();

    // assemble the parsed tokens into machine code and evaluate any unevaluated expressions
    status = ASSEMBLING;
    log(LOG_ASSEMBLING, std::stringstream() << BOLD << BOLD_WHITE << "Assembling Tokens" << RESET);
    passTokens();
    log(LOG_ASSEMBLING, std::stringstream() << BOLD << BOLD_GREEN << "Assembled Tokens" << RESET);

    // write to file
    log(LOG, std::stringstream() << BOLD << BOLD_WHITE << "Writing To File" << RESET);
    writeToFile();
    log(LOG, std::stringstream() << BOLD << BOLD_GREEN << "Wrote To File" << RESET);

    // done assembling
    status = ASSEMBLED;
    log(LOG, std::stringstream() << BOLD << BOLD_GREEN << "Successfully Assembled!" << RESET);
}


/**
 * Creates a label at the correct scope and map it to the symbol table.
 * 
 * @param labelName The name of the label to create.
 * @param value The value of the label.
 * @param allowMultipleDefinitions Whether to allow multiple definitions of the label.
 * 
 * @throws INVALID_TOKEN_ERROR if the label name is invalid.
 * @throws MULTIPLE_DEFINITION_ERROR if the label is already defined in the current scope.
 */
void AlienCPUAssembler::defineLabel(std::string labelName, Word value, bool allowMultipleDefinitions) {
	// check that label name only contains alphanumeric characters and '_' and is not empty
	if (labelName.empty()) {
		error(INVALID_TOKEN_ERROR, tokens[currentTokenI], std::stringstream() << "Invalid Label Name: " << labelName);
	}

	for (char c : labelName) {
		if (!std::isalnum(c) && c != '_') {
			error(INVALID_TOKEN_ERROR, tokens[currentTokenI], std::stringstream() << "Invalid Label Name: " << labelName);
		}
	}

	// check what scope we are in
    bool isLocal = labelName[0] == '_';

    // check if label is already defined in the current scope if we are in the first parsing phase
    if (!allowMultipleDefinitions) {
        if (status == PARSING && isLocal && (*currentScope).labels.find(labelName) != (*currentScope).labels.end()) {
            error(MULTIPLE_DEFINITION_ERROR, tokens[currentTokenI], std::stringstream() << "Multiple Definition of a Local Label");
        } else if (status == PARSING && !isLocal && (*globalScope).labels.find(labelName) != (*globalScope).labels.end()) {
            error(MULTIPLE_DEFINITION_ERROR, tokens[currentTokenI], std::stringstream() << "Multiple Definition of a Global Label");
        }
    }

    // add label to symbol table
    if (isLocal) {
        (*currentScope).labels[labelName] = value;
        parsedTokens.push_back(ParsedToken(tokens[currentTokenI], TOKEN_LOCAL_LABEL, value));
    } else {
        (*globalScope).labels[labelName] = value;
        parsedTokens.push_back(ParsedToken(tokens[currentTokenI], TOKEN_GLOBAL_LABEL, value));
    }
}


/**
 * Creates a new local scope with the current scope being the parent of the new scope.
 * 
 * @throws INTERNAL_ERROR if the assembler is not in the parsing phase.
 * @throws MULTIPLE_DEFINITION_ERROR if the scope is already defined at the current program counter.
 */
void AlienCPUAssembler::startScope() {
    // check if first pass
    if (status == PARSING) {
        Scope* localScope = new Scope(currentScope, currentProgramCounter);
        if (scopeMap.find(currentProgramCounter) != scopeMap.end()) {
            error(MULTIPLE_DEFINITION_ERROR, tokens[currentTokenI], std::stringstream() 
                << "Scope already defined at program counter: " << currentProgramCounter);
        }
        scopeMap.insert(std::pair<Word, Scope*>(currentProgramCounter, localScope));
        currentScope = localScope;
    } else if (status == ASSEMBLING) {
        // scope has to have already been defined
        if (scopeMap.find(currentProgramCounter) == scopeMap.end()) {
            error(INTERNAL_ERROR, tokens[currentTokenI], std::stringstream() 
                << "Scope not defined at program counter: " << currentProgramCounter);
        }

        currentScope = scopeMap.at(currentProgramCounter);
    } else {
        error(INTERNAL_ERROR, tokens[currentTokenI], std::stringstream() << "Invalid assembler status: " << status);
    }
}

/**
 * Ends the current scope and return back to the parent scope.
 * 
 * @throws INVALID_TOKEN_ERROR if the current scope has no parent scope.
 */
void AlienCPUAssembler::endScope() {
    // check if the current scope has no parent scope. This means we are at the global scope which has
    // no parent scope.
    if (currentScope->parent == nullptr) {
        error(INVALID_TOKEN_ERROR, tokens[currentTokenI], std::stringstream() << "Cannot end scope, no parent scope.");
    }

    currentScope = currentScope->parent;
}


/**
 * Iterates through the tokens list and performs the appropriate task for the current assembler status.
 * 
 * If the assembler's status is PARSING, then any label tokens are added to the symbol table.
 * If the assembler's status is ASSEMBLING, then everything is written to binary file
 * 
 * @throws INTERNAL_ERROR if the assembler is in an invalid state.
 * @throws UNRECOGNIZED_TOKEN_ERROR if the assembler encounters an unrecognized token.
 * @throws MISSING_TOKEN_ERROR if the assembler encounters a missing token.
 * @throws INVALID_TOKEN_ERROR if the assembler encounters an invalid token.
 */
void AlienCPUAssembler::passTokens() {
    // memory segment currently writing to
    segmentName = "";
    segmentType = TEXT_SEGMENT;

    //Program counter for the data and text segments of the program
    currentProgramCounter = 0;
    
    // parse each token
    for (currentTokenI = 0; currentTokenI < tokens.size(); currentTokenI++) {
        Token& token = tokens[currentTokenI];

        // check if the token is a directive
        if (token.string[0] == '.') {
            if (directiveMap.find(token.string) == directiveMap.end()) {
                error(UNRECOGNIZED_TOKEN_ERROR, token, std::stringstream() << "Unrecognized Directive");
            }

            DirectiveType type = directiveMap.at(token.string);
            parsedTokens.push_back(ParsedToken(token, TOKEN_DIRECTIVE));

            // process directive, offload to some other function to do
            (this->*processDirective[type])();

            // directives have to be on their own line
            EXPECT_NO_OPERAND();
            continue;
        }

        // check if token is label
        if (token.string[token.string.size() - 1] == ':') {
            std::string labelName = token.string.substr(0, token.string.size() - 1);
            defineLabel(labelName, currentProgramCounter);
            continue;
        }

        // check if token is instruction
        if (instructionMap.find(token.string) != instructionMap.end()) {
            // check if in proper segment
            if (segmentType != TEXT_SEGMENT) {
                error(INVALID_TOKEN_ERROR, token, std::stringstream() << "Instruction must be defined in the text segment");
            }

            // no more tokens to parse that are on the same line
            // so either this has no operands or it is missing operands
            if (!HAS_OPERAND()) {
                // check if instruction is valid if there are no operands
                AddressingMode addressingMode = NO_ADDRESSING_MODE;
                if (validInstruction(token.string, IMPLIED)) {
                    addressingMode = IMPLIED;
                }

                if (validInstruction(token.string, ACCUMULATOR)) {
                    if (addressingMode != NO_ADDRESSING_MODE) {
                        error(INTERNAL_ERROR, token, std::stringstream() << "Multiple Possible Addressing Modes");
                    }
                    addressingMode = ACCUMULATOR;
                }

                // invalid instruction
                if (addressingMode == NO_ADDRESSING_MODE) {
                    error(MISSING_TOKEN_ERROR, token, std::stringstream() << "Missing Instruction Operand");
                }
                
                // valid instruction
                log(LOG_PARSING, std::stringstream() << GREEN << "Instruction\t" << RESET << "[" << token.string << "]");
                parsedTokens.push_back(ParsedToken(token, TOKEN_INSTRUCTION, currentProgramCounter, addressingMode));

                writeByte(instructionMap[token.string][addressingMode]);
            } else {
                // parse the operand token to get the addressing mode
                // the operand token is guaranteed to be on the same line as the instruction token
                currentTokenI++;
                Token& operandToken = tokens[currentTokenI];
                AddressingMode addressingMode = getAddressingMode(token, operandToken);

                // check if operand could potentially be an expression.
                if (addressingMode == NO_ADDRESSING_MODE) {
                    // for the first pass, assume it is an expression. Evaluate it in the second pass.
                    // if it is an expression, the addressing mode would default to IMMEDIATE or 
                    // RELATIVE depending on what is valid
                    if (validInstruction(token.string, IMMEDIATE)) {
                        addressingMode = IMMEDIATE;
                    }

                    if (validInstruction(token.string, RELATIVE)) {
                        if (addressingMode != NO_ADDRESSING_MODE) {
                            error(INTERNAL_ERROR, token, std::stringstream() << "Multiple Possible Addressing Modes");
                        }

                        addressingMode = RELATIVE;
                    }

                    // no possible addressing mode that takes in an expression
                    if (addressingMode == NO_ADDRESSING_MODE) {
                        error(INVALID_TOKEN_ERROR, operandToken, std::stringstream() << "Invalid Addressing Mode For Instruction " << token.string);
                    }
                }

                // valid instruction with operand
                log(LOG_PARSING, std::stringstream() << GREEN << "Instruction\t" << RESET << "[" << token.string << "] [" << operandToken.string << "]");
                parsedTokens.push_back(ParsedToken(token, TOKEN_INSTRUCTION, currentProgramCounter, addressingMode));
                writeByte(instructionMap[token.string][addressingMode]);

                // only evaluate the operand bytes if we are in the second pass
                parsedTokens.push_back(ParsedToken(operandToken, TOKEN_INSTRUCTION_OPERAND, currentProgramCounter));
                if (status == PARSING) {
                    writeBytes(0, addressingModeOperandBytes.at(addressingMode));
                } else if (status == ASSEMBLING) {
                    writeBytes(parseValue(operandToken.string), addressingModeOperandBytes.at(addressingMode));
                } else {
                    error(INTERNAL_ERROR, token, std::stringstream() << "Invalid Assembler Status");
                }

                EXPECT_NO_OPERAND();
            }
            continue;
        }

        // unrecognized token
        error(UNRECOGNIZED_TOKEN_ERROR, token, std::stringstream() << "Unrecognized Token While Parsing");
    }

    // end of pass
    // some important checks to ensure user is not doing anything stupid
    if (currentScope != globalScope) {
        // this means we are still in a scope that has not been closed
        error(MISSING_TOKEN_ERROR, *NULL_TOKEN, std::stringstream() << "Scope defined at " 
                << (*currentScope).prettyStringifyMemoryAddress() << "is not closed.");
    }
}


/**
 * Evaluates an expression of a token operand
 * 
 * @param token The token to evaluate the expression of
 * @return The value of the expression
 */
u64 AlienCPUAssembler::evaluateExpression(Token token) {
    // TODO: complete this please


    return 0;
}


/**
 * Extracts the value of a token operand
 * 
 * @param token The token to extract the value from
 * @return The value of the token operand
 * 
 * @throws INTERNAL_ERROR if the assembler is not in the parsing phase
 * @throws INVALID_TOKEN_ERROR if the token operand is invalid
 * @throws UNRECOGNIZED_TOKEN_ERROR if the token operand is unrecognized
 */
u64 AlienCPUAssembler::parseValue(const std::string token) {
    if (token.empty()) {
        error(INTERNAL_ERROR, tokens[currentTokenI], std::stringstream() << "Invalid empty value to parse: " << token);
    }

    // remove the value symbol if it is present
    std::string value = token[0] == '#' ? token.substr(1) : token;
    
    // check if value is empty
    if (value.empty()) {
        error(INVALID_TOKEN_ERROR, tokens[currentTokenI], std::stringstream() << "Invalid value to parse: " << value);
    }


    // check if it references a non numeric value
    if (!isHexadecimalNumber(value.substr(1)) && !isNumber(value)) {
        // evaluate the expression
        return evaluateExpression(tokens[currentTokenI]);

        error(INVALID_TOKEN_ERROR, tokens[currentTokenI], std::stringstream() << "Unsupported Non-numeric Value: " << value);
    }


    // hexadecimal
    if (value[0] == '$') {
        // check it contains only 0-9 or A-F characters
        std::string numericValue = value.substr(1); // get rid of the '$' symbol
        u64 number = 0;
        std::string::const_iterator it = numericValue.begin();
        while (it != numericValue.end() && (std::isdigit(*it) || 
                (std::isalpha(*it) && std::toupper(*it) >= 'A' && std::toupper(*it) <= 'F'))) {
            number *= 16;
            if (std::isdigit(*it)) {
                number += (*it) - '0';
            } else {
                number += 10 + (std::toupper(*it) - 'A');
            }
            
            ++it;
        }
        
        // contains other characters
        if (it != numericValue.end()) {
            error(INVALID_TOKEN_ERROR, tokens[currentTokenI], std::stringstream() << "Invalid Hexadecimal Digit '" << *it << "': " << numericValue);
        }

        // proper hexadecimal value
        return number;
    } else if (value[0] == '0') {
        // check it contains only 0-7 characters
        std::string numericValue = value.substr(1); // get rid of the '0' symbol
        u64 number = 0;
        std::string::const_iterator it = numericValue.begin();
        while (it != numericValue.end() && (*it) >= '0' && (*it) <= '7') {
            number *= 8;
            number += (*it) - '0';
            ++it;
        }
        
        // contains other characters
        if (it != numericValue.end()) {
            error(INVALID_TOKEN_ERROR, tokens[currentTokenI], std::stringstream() << "Invalid Octal Digit '" << *it << "': " << numericValue);
        }

        // proper octal value
        return number;
    } else if (value[0] == '%') {
        // check it contains only 0-1 characters
        std::string numericValue = value.substr(1); // get rid of the '%' symbol
        u64 number = 0;
        std::string::const_iterator it = numericValue.begin();
        while (it != numericValue.end() && (*it) >= '0' && (*it) <= '1') {
            number *= 2;
            number += (*it) - '0';
            ++it;
        }
        
        // contains other characters
        if (it != numericValue.end()) {
            error(INVALID_TOKEN_ERROR, tokens[currentTokenI], std::stringstream() << "Invalid Binary Digit '" << *it << "': " << numericValue);
        }

        // proper binary value
        return number;
    } else if (isNumber(value)) {
        // must be a number of some base
        u64 number = 0;
        std::string::const_iterator it = value.begin();
        while (it != value.end() && (*it) >= '0' && (*it) <= '9') {
            number *= 10;
            number += (*it) - '0';
            ++it;
        }

        // contains other characters
        if (it != value.end()) {
            error(INVALID_TOKEN_ERROR, tokens[currentTokenI], std::stringstream() << "Invalid Decimal Digit '" << *it << "': " << value);
        }

        // proper decimal value
        return number;
    }

    error(UNRECOGNIZED_TOKEN_ERROR, tokens[currentTokenI], std::stringstream() << "Unsupported Numeric Value: " << value);
    return 0;
}




/**
 * Converts the source code into a list of tokens to be parsed
 * 
 * Tokens are separated by at least a tab character with leading and trailing whitespaces removed.
 * 
 * Single line and Multi line comments are supported. Single line comments are denoted by a ';' and converts
 * every character from that point to the immediate end of line character '\n' into a comment. Multi line comments
 * are denoted by a ';*' and converts every character from that point to the closing multi line comment character
 * '*;' into a comment.
 * 
 * No further processing is done on the tokens at this point.
 * 
 * @throws MISSING_TOKEN_ERROR if a multi line comment is not closed by '*;'
 * @throws INTERNAL_ERROR if the current token has not been processed
 */
void AlienCPUAssembler::tokenize() {
    log(LOG_TOKENIZING, std::stringstream() << BOLD << BOLD_WHITE << "Tokenizing Source Code" << RESET);
    
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
                log(LOG_TOKENIZING, std::stringstream() << CYAN << "Token\t" << RESET << "[" << currentToken << "]");
            } else {
                log(LOG_TOKENIZING, std::stringstream() << GREEN << "Comment\t" << RESET << "[" << currentToken << "]");
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
            log(LOG_TOKENIZING, std::stringstream() << GREEN << "Comments\t" << RESET << "[" << tostring(list) << "]");

            // end current token
            currentToken.clear();
            currentTokenStart = -1;

            readyForNextToken = true;
        }
    }

    // check if multi line comment was never closed
    if (isMultiLineComment) {
        error(MISSING_TOKEN_ERROR, Token(currentToken, currentTokenStart, lineNumber), 
                std::stringstream() << "Multiline comment is not closed by \'*;\'");
    }

    // check if current token has not been processed
    if (currentToken.size() != 0) {
        error(INTERNAL_ERROR, Token(currentToken, currentTokenStart, lineNumber),
                std::stringstream() << "Current token has not been processed");
    }

    log(LOG_TOKENIZING, std::stringstream() << BOLD << BOLD_GREEN << "Tokenized\t" << RESET << tostring(tokens));
}



/**
 * Converts the given operand to an addressing mode.
 * 
 * TODO: can probably use regex to simplify this
 * 
 * @param tokenInstruction The instruction that the operand is associated with.
 * @param tokenOperand The operand to convert.
 * @return The addressing mode of the given operand.
 */
AddressingMode AlienCPUAssembler::getAddressingMode(Token tokenInstruction, Token tokenOperand) {
    std::string operand = tokenOperand.string;
    if (operand.empty()) {
        // error(ERROR, tokenOperand, std::stringstream() << "Invalid operand to convert: " << operand);
        return NO_ADDRESSING_MODE;
    }

    // check if operand is an immediate or relative value
    if (operand[0] == '#') {
        u64 parsedValue = parseValue(operand);
        if (parsedValue > 0xFFFFFFFF) {
            // error(ERROR, tokenOperand, std::stringstream() << "Invalid immediate value: " << operand);
            return NO_ADDRESSING_MODE;
        }

        if (validInstruction(tokenInstruction.string, IMMEDIATE)) {
            return IMMEDIATE;
        } else if (validInstruction(tokenInstruction.string, RELATIVE)) {
            return RELATIVE;
        } else {
            return NO_ADDRESSING_MODE;
        }
    }

    // check if operand is zeropage or absolute
    if (operand[0] == '%' || operand[0] == '0' || operand[0] == '$' || isNumber(operand)) {
        u64 parsedValue = parseValue(operand);
        if (parsedValue > 0xFFFFFFFF) {
            return NO_ADDRESSING_MODE;
        }

        if (parsedValue <= 0xFFFF && validInstruction(tokenInstruction.string, ZEROPAGE)) {
            return ZEROPAGE;
        } else if (validInstruction(tokenInstruction.string, ABSOLUTE)) {
            return ABSOLUTE;
        } else {
            return NO_ADDRESSING_MODE;    
        }
    }

    // check if operand is zeropage,x or zeropage,y or absolute,x or absolute,y
    std::vector<std::string> splitByComma = split(operand, ',');
    if (splitByComma.size() == 2 && isHexadecimalNumber(splitByComma[0]) && (splitByComma[1] == "x" || splitByComma[1] == "y")) {
        bool isX = splitByComma[1] == "x";

        Token valueToken = Token(tokenOperand);
        valueToken.string = splitByComma[0];

        u64 parsedValue = parseValue(operand);
        if (parsedValue > 0xFFFFFFFF) {
            return NO_ADDRESSING_MODE;
        }

        if (parsedValue <= 0xFFFF && isX && validInstruction(tokenInstruction.string, ZEROPAGE_XINDEXED)) {
            return ZEROPAGE_XINDEXED;
        } else if (isX && validInstruction(tokenInstruction.string, ABSOLUTE_XINDEXED)) {
            return ABSOLUTE_XINDEXED;
        } else if (parsedValue <= 0xFFFF && !isX && validInstruction(tokenInstruction.string, ZEROPAGE_YINDEXED)) {
            return ZEROPAGE_YINDEXED;
        } else if (!isX && validInstruction(tokenInstruction.string, ABSOLUTE_YINDEXED)) {
            return ABSOLUTE_YINDEXED;
        } else {
            return NO_ADDRESSING_MODE;
        }
    }

    // check if operand is indirect
    if (operand[0] == '(' && operand[operand.size() - 1] == ')' && isHexadecimalNumber(operand.substr(1,operand.size() - 2))) {
        Token valueToken = Token(tokenOperand);
        valueToken.string = operand.substr(1,operand.size() - 2);

        u64 parsedValue = parseValue(operand);
        if (parsedValue > 0xFFFFFFFF) {
            return NO_ADDRESSING_MODE;
        }

        return INDIRECT;
    }

    // check if operand is x indexed indirect
    if (splitByComma.size() == 2 && splitByComma[0].size() > 1 && splitByComma[1].size() == 2
        && splitByComma[0][0] == '(' && splitByComma[1][splitByComma[1].size() - 1] == ')'
        && isHexadecimalNumber(splitByComma[0].substr(1)) && splitByComma[1][0] == 'x') {
        Token valueToken = Token(tokenOperand);
        valueToken.string = splitByComma[0].substr(1);

        u64 parsedValue = parseValue(operand);
        if (parsedValue > 0xFFFF) {
            return NO_ADDRESSING_MODE;
        }

        return XINDEXED_INDIRECT;
    }

    // check if operand is indirect y indexed
    if (splitByComma.size() == 2 && splitByComma[0].size() > 2 && splitByComma[1].size() == 1
        && splitByComma[0][0] == '(' && splitByComma[0][splitByComma[0].size() - 1] == ')' 
        && isHexadecimalNumber(splitByComma[0].substr(1,splitByComma[0].size() - 2))
        && splitByComma[1][0] == 'y') {
        Token valueToken = Token(tokenOperand);
        valueToken.string = splitByComma[0].substr(1,splitByComma[0].size() - 2);

        u64 parsedValue = parseValue(operand);
        if (parsedValue > 0xFFFF) {
            return NO_ADDRESSING_MODE;
        }

        return INDIRECT_YINDEXED;
    }

    // invalid operand
    return NO_ADDRESSING_MODE;
}




/**
 * Throws a compiler error when trying to parse a token
 * TODO: store more useful information in this. Also add support for various types of errors, some that don't require having
 * a token.
 * 
 * @param error The type of error to throw
 * @param currentToken The token that caused the error
 * @param msg The message to display with the error
 */
void AlienCPUAssembler::error(AssemblerError error, const Token& currentToken, std::stringstream msg) {
    std::string name;
    switch(error) {
        case INVALID_TOKEN_ERROR:
            name = BOLD_RED + "[invalid_token]" + RESET;
            break;
        case FILE_ERROR:
            name = BOLD_RED + "[file_error]" + RESET;
            break;
        case MULTIPLE_DEFINITION_ERROR:
            name = BOLD_RED + "[multiple_definition]" + RESET;
            break;
        case UNRECOGNIZED_TOKEN_ERROR:
            name = BOLD_RED + "[unrecognized_token]" + RESET;
            break;
        case MISSING_TOKEN_ERROR:
            name = BOLD_YELLOW + "[missing_token]" + RESET;
        case INTERNAL_ERROR:
            name = BOLD_RED + "[internal_error]" + RESET;
            break;
        default:
            name = BOLD_RED + "[error]" + RESET;
    }

    std::stringstream msgStream;

    // check if we have a valid token to display information about
    if (&currentToken == NULL_TOKEN) {
        msgStream << name << " " << msg.str() << std::endl;
    } else {
        msgStream << name << " " << msg.str() << " at line " << currentToken.lineNumber 
                << " \"" << currentToken.string << "\"" << std::endl;
    }

    throw std::runtime_error(msgStream.str());
}


/**
 * Warns about potential bugs in the code
 * 
 * @param warn The type of warning to display
 * @param msg The message to display with the warning
 */
void AlienCPUAssembler::warn(AssemblerWarn warn, std::stringstream msg) {
	std::string name;
	switch(warn) {
		case WARN:
		default:
			name = BOLD_YELLOW + "[warn]" + RESET;
	}

	std::cout << name << " " << msg.str() << std::endl;
}


/**
 * Logs information about the assembling process
 * 
 * @param log The type of log to display
 * @param msg The message to display with the log
 */
void AlienCPUAssembler::log(AssemblerLog log, std::stringstream msg) {
    if (!debugOn) {
        return;
    }

    std::string name;
    switch(log) {
        case LOG_TOKENIZING:
            name = BOLD_BLUE + "[tokenizing]" + RESET;
            break;
        case LOG_PARSING:
            name = BOLD_GREEN + "[parsing]" + RESET;
            break;
        case LOG_ASSEMBLING:
            name = BOLD_MAGENTA + "[assembling]" + RESET;
            break;
        default:
            name = BOLD_CYAN + "[log]" + RESET;
    }

    std::cout << name << " " << msg.str() << std::endl;
}



/**
 * Prints all memory segments in the memory map in ascending order.
 */
void AlienCPUAssembler::printMemoryMap() {
	log(LOG, std::stringstream() << BOLD << BOLD_WHITE << "Printing Memory Map");
	
	// print out each memory segmentsegment
	const u16 NUMBER_OF_BYTES_PER_LINE = 16;
    for (auto it = memoryMap.begin(); it != memoryMap.end(); it++) {
        // get current segment
        MemorySegment segment = *(*it).second;
        std::cout << "[" << segment.prettyStringifyStartAddress() << "," 
                << segment.prettyStringifyEndAddress() << "]\t";
        
        // print out each byte in segment
        for (int i = 0; i < segment.bytes.size(); i++) {
            if (i != 0 && i % NUMBER_OF_BYTES_PER_LINE == 0) {
                std::cout << std::endl << "\t\t\t";
            }
            std::cout << stringifyHex(segment.bytes[i]) << " ";
        }
        std::cout << std::endl;
    }

    log(LOG, std::stringstream() << BOLD << BOLD_WHITE << "Done Printing Memory Map");
}

/**
 * Prints all parsed tokens and their associated memory address.
 */
void AlienCPUAssembler::printParsedTokens() {
	for (ParsedToken& parsedToken : parsedTokens) {
        if (parsedToken.type == TOKEN_INSTRUCTION) {
            std::cout << "[" << parsedToken.prettyStringifyMemoryAddress() << "]\t" << parsedToken.token.string 
                    << " " << addressingModeNames.at(parsedToken.addressingMode) << std::endl;
        } else if (parsedToken.type == TOKEN_INSTRUCTION_OPERAND || parsedToken.type == TOKEN_GLOBAL_LABEL || parsedToken.type == TOKEN_LOCAL_LABEL) {
            std::cout << "[" << parsedToken.prettyStringifyMemoryAddress() << "]\t" << parsedToken.token.string << std::endl;
        } else {
            std::cout << "\t\t" << ITALICS << GRAY << parsedToken.token.string << RESET << std::endl;
        }
    }
}




/**
 * Extracts the string oeprand from the token
 * 
 * @param token The token to extract the string operand from
 * @return The string operand
 */
std::string AlienCPUAssembler::getStringToken(std::string token) {
    if (!isStringToken(token)) {
        error(INTERNAL_ERROR, *NULL_TOKEN, std::stringstream() << "Invalid string token: " << token);
    }

    return token.substr(1, token.size() - 2);
}


/**
 * Simulates writing a byte to file by first tracking the byte in the assembler
 * 
 * @param value The value to write to the file
 */
void AlienCPUAssembler::writeByte(Byte value) {
    // Don't write if not assembling
    if (status != ASSEMBLING) {
        currentProgramCounter++;
        return;
    }

    // write the byte
    // check if we are writing to a new memory segment
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
        (*memorySegment).bytes.push_back(value);
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
    currentProgramCounter++;
}

/**
 * Writes two bytes in specified endian order to file
 * 
 * @param value The value to write to the file
 * @param lowEndian If true, the low byte is written first, otherwise the high byte is written first
 */
void AlienCPUAssembler::writeTwoBytes(u16 value, bool lowEndian) {
    writeBytes(value, 2, lowEndian);
}

/**
 * Writes four bytes in specified endian order to file
 * 
 * @param value The value to write to the file
 * @param lowEndian If true, the low byte is written first, otherwise the high byte is written first
 */
void AlienCPUAssembler::writeWord(Word value, bool lowEndian) {
    writeBytes(value, 4, lowEndian);
}

/**
 * Writes four bytes in specified endian order to file
 * 
 * @param value The value to write to the file
 * @param lowEndian If true, the low byte is written first, otherwise the high byte is written first
 */
void AlienCPUAssembler::writeTwoWords(u64 value, bool lowEndian) {
    writeBytes(value, 8, lowEndian);
}

/**
 * Writes the specified number of bytes in specified endian order to file
 * 
 * @param value The value to write to the file
 * @param bytes The number of bytes to write
 * @param lowEndian If true, the low byte is written first, otherwise the high byte is written first
 */
void AlienCPUAssembler::writeBytes(u64 value, Byte bytes, bool lowEndian) {
    if (bytes > 8) {
        error(INTERNAL_ERROR, *NULL_TOKEN, std::stringstream() << "Cannot write more than 8 bytes");
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