#include "AlienCPUAssembler.h"

#include <any>

int main() {
    AlienCPU cpu;
    AlienCPUAssembler assembler(cpu, true);

    std::stringstream sourceCode;
    sourceCode << 
        "\tLDA\t\t \t#$FFFF\n" << 
        ";THIS IS A COMMENT\n" << 
        ";SO IS THIS\n" << 
        ".org\t%0101\n" <<
        "\tLDA\t#$FFEF\n" <<
        "globallabel1:\t;this is a comment\n" <<
        "_locallabel:\n" <<
        "globallabel2:\n" <<
        "_locallabel:";

    assembler.assemble(sourceCode.str());
}


/**
 * Constructs a new AlienCPUAssembler for the given AlienCPU.
 */
AlienCPUAssembler::AlienCPUAssembler(AlienCPU& cpu, bool debugOn) : cpu(cpu), debugOn(debugOn) {}

/**
 * Resets the internal state of the assembler.
 */
void AlienCPUAssembler::reset() {
    status = STARTING;
    outputFile.clear();
    sourceCode.clear();
    tokens.clear();
    parsedTokens.clear();
}


/**
 * Assembles the given assembly source code into machine code and loads it onto the cpu.
 * 
 * TODO: technically should probably load into a .bin file and then manually write it into the cpu
 * Documentation on the assembly language can be found SOMEWHERE
 * 
 * @param source The assembly source code to assemble into machine code.
 */
void AlienCPUAssembler::assemble(std::string source) {
    log(LOG, std::stringstream() << BOLD << BOLD_WHITE << "Assembling..." << RESET);

    // reset assembler state to be ready for a new assembly
    reset();
    
    tokenize();

    // print out each token
    int curLine = 0;
    for (Token& token : tokens) {
        if (token.lineNumber > curLine) {
            std::cout << std::endl;
        }
        std::cout << token.string << "\t";
        curLine = token.lineNumber;
    }
    std::cout << std::endl;

    passTokens();

    // print out each parsed token and its associated memory address
    for (ParsedToken& parsedToken : parsedTokens) {
        if (parsedToken.type == INSTRUCTION) {
            std::cout << "[" << parsedToken.memoryAddress << "]\t" << parsedToken.token.string 
                    << " " << addressingModeNames.at(parsedToken.addressingMode) << std::endl;
        } else if (parsedToken.type == INSTRUCTION_OPERAND || parsedToken.type == GLOBAL_LABEL || parsedToken.type == LOCAL_LABEL) {
            std::cout << "[" << parsedToken.memoryAddress << "]\t" << parsedToken.token.string << std::endl;
        } else {
            std::cout << "\t" << parsedToken.token.string << std::endl;
        }
    }

    // assemble the parsed tokens into machine code
    

    log(LOG, std::stringstream() << BOLD << BOLD_GREEN << "Successfully Assembled!" << RESET);
}



/**
 * Iterates through the tokens list and performs the appropriate task for the current assembler status.
 * 
 * If the assembler's status is PARSING, then any label tokens are added to the symbol table.
 * If the assembler's status is ASSEMBLING, then everything is written to binary file
 */
void AlienCPUAssembler::passTokens() {
    log(PARSING, std::stringstream() << BOLD << BOLD_WHITE << "Parsing Tokens" << RESET);

    // memory segment currently writing to
    segmentName = "";
    segmentType = TEXT_SEGMENT;

    //Program counter for the data and text segments of the program
    Word currentProgramCounter = 0;

    // parse a value and throw an error if it is not between min and max
    auto EXPECT_PARSEDVALUE = [this](int tokenIndex, u64 min, u64 max) {
        u64 parsedValue = parseValue(tokens[tokenIndex]);
        if (parsedValue < min || parsedValue > max) {
            error(INVALID_TOKEN, tokens[tokenIndex], std::stringstream() << "Invalid Value: " << parsedValue << 
            " must be between " << min << " and " << max);
        }
        return parsedValue;
    };

    // throw an error if there is no operand available
    auto EXPECT_OPERAND = [this](int tokenIndex) {
        if (tokenIndex == tokens.size() - 1 || tokens[tokenIndex + 1].lineNumber != tokens[tokenIndex].lineNumber) {
            error(MISSING_TOKEN, tokens[tokenIndex], std::stringstream() << "Missing Operand");
        }
    };

    // throw an error if there is an operand available
    auto EXPECT_NO_OPERAND = [this](int tokenIndex) {
        if (tokenIndex != tokens.size() - 1 && tokens[tokenIndex + 1].lineNumber == tokens[tokenIndex].lineNumber) {
            error(MISSING_TOKEN, tokens[tokenIndex], std::stringstream() << "Too Many Operands");
        }
    };

    // check if there is an operand available
    auto HAS_OPERAND = [this](int tokenIndex, bool onSameLine = true) {
        return tokenIndex != tokens.size() - 1 && (!onSameLine || tokens[tokenIndex + 1].lineNumber == tokens[tokenIndex].lineNumber);
    };
    
    // parse each token
    for (int tokenI = 0; tokenI < tokens.size(); tokenI++) {
        Token& token = tokens[tokenI];



        // check if the token is a directive
        if (token.string[0] == '.') {
            if (directiveMap.find(token.string) == directiveMap.end()) {
                error(UNRECOGNIZED_TOKEN, token, std::stringstream() << "Unrecognized Directive");
            }

            DirectiveType type = directiveMap.at(token.string);
            parsedTokens.push_back(ParsedToken(token, DIRECTIVE));
            
            if (type == DATA || type == TEXT) {
                // save previous segment
                segments[segmentType][segmentName] = currentProgramCounter;

                segmentType = type == DATA ? DATA_SEGMENT : TEXT_SEGMENT;
                segmentName = "";

                // check if segment has a name
                if (HAS_OPERAND(tokenI)) {
                    std::string segmentLabel = tokens[++tokenI].string;
                    parsedTokens.push_back(ParsedToken(tokens[tokenI], DIRECTIVE_OPERAND));
                }

                // check and load value if segment has already been defined
                currentProgramCounter = segments[segmentType][segmentName];
            } else if (type == OUTFILE) {
                EXPECT_OPERAND(tokenI++);

                // check if outfile has already been defined TODO: determine if we should allow multiple outfiles
                if (!outputFile.empty()) {
                    error(MULTIPLE_DEFINITION, token, std::stringstream() << ".outfile directive cannot be defined multiple times in the same file");
                }

                // set the outfile, must be a string argument
                outputFile = getStringToken(tokens[tokenI].string);

                if (!isValidFilename(outputFile)) {
                    error(INVALID_TOKEN, tokens[tokenI], std::stringstream() << "Invalid filename for .outfile directive: " << outputFile);
                }

                parsedTokens.push_back(ParsedToken(tokens[tokenI], DIRECTIVE_OPERAND));
            } else if (type == ORG) {
                EXPECT_OPERAND(tokenI++);

                // parse org value, must be a value capable of being evaluated in the parse phase
                // ie, any labels referenced must be already defined and any expressions must be evaluated
                Word value = EXPECT_PARSEDVALUE(tokenI, 0, 0xFFFFFFFF);
                parsedTokens.push_back(ParsedToken(tokens[tokenI], DIRECTIVE_OPERAND));
                
                currentProgramCounter = value;
            } else if (type == DB_LO) {
                // needs an operand
                EXPECT_OPERAND(tokenI++);
                
                // split operand by commas
                std::vector<std::string> splitByComma = split(tokens[tokenI].string, ',');

                // parse each value, must be a value capable of being evaluated in the parse phase
                // ie, any labels referenced must be already defined and any expressions must be evaluated
                for (std::string& value : splitByComma) {
                    u64 parsedValue = parseValue(tokens[tokenI]);
                    if (parsedValue > 0xFF) {
                        error(INVALID_TOKEN, tokens[tokenI], std::stringstream() << "Invalid value for .db directive: " << value);
                    }

                    currentProgramCounter += 1;
                }


            } else if (type == D2B_LO) {

            } else if (type == DW_LO) {

            } else if (type == D2W_LO) {

            } else if (type == DB_HI) {

            } else if (type == D2B_HI) {

            } else if (type == DW_HI) {

            } else if (type == D2W_HI) {

            } else if (type == ADVANCE) {

            } else if (type == FILL) {

            } else if (type == SPACE) {

            } else if (type == DEFINE) {

            } else if (type == CHECKPC) {

            } else if (type == ALIGN) {

            } else if (type == INCBIN) {

            } else if (type == INCLUDE) {

            } else if (type == REQUIRE) {

            } else if (type == SCOPE) {

            } else if (type == SCEND) {

            } else if (type == MACRO) {

            } else if (type == MACEND) {

            } else if (type == INVOKE) {

            } else if (type == ASSERT) {

            } else if (type == ERROR) {

            } else if (type == ERRORIF) {

            } else if (type == IFF) {

            } else if (type == IFDEF) {

            } else if (type == IFNDEF) {

            } else if (type == ELSEIF) {

            } else if (type == ELSEIFDEF) {

            } else if (type == ELSEIFNDEF) {

            } else if (type == ELSE) {

            } else if (type == ENDIF) {

            } else if (type == PRINT) {

            } else if (type == PRINTIF) {

            } else if (type == PRINTNOW) {

            } else {
                error(UNRECOGNIZED_TOKEN, token, std::stringstream() << "Unrecognized Directive");
            }

            EXPECT_NO_OPERAND(tokenI);
            continue;
        }

        // check if token is label
        if (token.string[token.string.size() - 1] == ':') {




            continue;
        }

        // check if token is instruction
        if (instructionMap.find(token.string) != instructionMap.end()) {
            // check if in proper segment
            if (segmentType != TEXT_SEGMENT) {
                error(INVALID_TOKEN, token, std::stringstream() << "Instruction must be in the text segment");
            }

            // no more tokens to parse that are on the same line
            // so either this has no operands or it is missing operands
            if (!HAS_OPERAND(tokenI)) {
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

                // check if instruction is valid if there are no operands
                if (addressingMode == NO_ADDRESSING_MODE) {
                    // missing operands
                    error(MISSING_TOKEN, token, std::stringstream() << "Missing Instruction Operand");
                }
                
                // valid instruction
                log(PARSING, std::stringstream() << GREEN << "Instruction\t" << RESET << "[" << token.string << "]");
                parsedTokens.push_back(ParsedToken(token, INSTRUCTION, currentProgramCounter, addressingMode));
                currentProgramCounter++; // no extra bytes for operands
            } else {
                // parse the operand token to get the addressing mode
                // the operand token is guaranteed to be on the same line as the instruction token
                Token& operandToken = tokens[++tokenI];
                AddressingMode addressingMode = getAddressingMode(token, operandToken);

                // invalid addressing mode
                if (addressingMode == NO_ADDRESSING_MODE) {
                    error(INVALID_TOKEN, operandToken, std::stringstream() << "Invalid Addressing Mode For Instruction " << token.string);
                }

                // valid instruction with operand
                log(PARSING, std::stringstream() << GREEN << "Instruction\t" << RESET << "[" << token.string << "] [" << operandToken.string << "]");
                parsedTokens.push_back(ParsedToken(token, INSTRUCTION, currentProgramCounter, addressingMode));
                currentProgramCounter++; // 1 byte for the instruction

                // operand
                parsedTokens.push_back(ParsedToken(operandToken, INSTRUCTION_OPERAND, currentProgramCounter));
                currentProgramCounter+= addressingModeOperandBytes.at(addressingMode); // bytes for the operand

                EXPECT_NO_OPERAND(tokenI);
            }
            continue;
        }

        // unrecognized token
        error(UNRECOGNIZED_TOKEN, token, std::stringstream() << "Unrecognized Token While Parsing");
    }

    log(PARSING, std::stringstream() << BOLD << BOLD_GREEN << "Parsed Tokens" << RESET);
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
        u64 parsedValue = parseValue(tokenOperand);
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
        u64 parsedValue = parseValue(tokenOperand);
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

        u64 parsedValue = parseValue(valueToken);
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

        u64 parsedValue = parseValue(valueToken);
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

        u64 parsedValue = parseValue(valueToken);
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

        u64 parsedValue = parseValue(valueToken);
        if (parsedValue > 0xFFFF) {
            return NO_ADDRESSING_MODE;
        }

        return INDIRECT_YINDEXED;
    }

    // invalid operand
    return NO_ADDRESSING_MODE;
}




void AlienCPUAssembler::evaluateExpression(Token token) {

}


/**
 * 
 */
u64 AlienCPUAssembler::parseValue(const Token token) {
    if (token.string.empty()) {
        error(INTERNAL_ERROR, token, std::stringstream() << "Invalid empty value to parse: " << token.string);
    }

    // remove the value symbol if it is present
    std::string value = token.string[0] == '#' ? token.string.substr(1) : token.string;
    
    // check if value is empty
    if (value.empty()) {
        error(INVALID_TOKEN, token, std::stringstream() << "Invalid value to parse: " << value);
    }


    // check if it references a non numeric value
    if (!isHexadecimalNumber(value.substr(1)) && !isNumber(value)) {










        error(INVALID_TOKEN, token, std::stringstream() << "Unsupported Non-numeric Value: " << value);
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
            error(INVALID_TOKEN, token, std::stringstream() << "Invalid Hexadecimal Digit '" << *it << "': " << numericValue);
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
            error(INVALID_TOKEN, token, std::stringstream() << "Invalid Octal Digit '" << *it << "': " << numericValue);
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
            error(INVALID_TOKEN, token, std::stringstream() << "Invalid Binary Digit '" << *it << "': " << numericValue);
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
            error(INVALID_TOKEN, token, std::stringstream() << "Invalid Decimal Digit '" << *it << "': " << value);
        }

        // proper decimal value
        return number;
    }

    error(UNRECOGNIZED_TOKEN, token, std::stringstream() << "Unsupported Numeric Value: " << value);
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
 */
void AlienCPUAssembler::tokenize() {
    log(TOKENIZING, std::stringstream() << BOLD << BOLD_WHITE << "Tokenizing Source Code" << RESET);
    
    // current token being constructed
    std::string currentToken = "";

    // is the current token a comment and what type
    bool isSingleLineComment = false;
    bool isMultiLineComment = false;

    // the index of the first character from the original source code of the current token being constructed
    int currentTokenStart = -1;

    // the line the current token is on in the original source code
    int lineNumber = 0;

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
                log(TOKENIZING, std::stringstream() << CYAN << "Token\t" << RESET << "[" << currentToken << "]");
            } else {
                log(TOKENIZING, std::stringstream() << GREEN << "Comment\t" << RESET << "[" << currentToken << "]");
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
            log(TOKENIZING, std::stringstream() << GREEN << "Comments\t" << RESET << "[" << tostring(list) << "]");

            // end current token
            currentToken.clear();
            currentTokenStart = -1;

            readyForNextToken = true;
        }
    }

    // check if multi line comment was never closed
    if (isMultiLineComment) {
        error(MISSING_TOKEN, Token(currentToken, currentTokenStart, lineNumber), 
                std::stringstream() << "Multiline comment is not closed by \'*;\'");
    }

    // check if current token has not been processed
    if (currentToken.size() != 0) {
        error(INTERNAL_ERROR, Token(currentToken, currentTokenStart, lineNumber),
                std::stringstream() << "Current token has not been processed");
    }

    log(TOKENIZING, std::stringstream() << BOLD << BOLD_GREEN << "Tokenized\t" << RESET << tostring(tokens));
}




/**
 * Throws a compiler error when trying to parse a token
 * 
 * @param error The type of error to throw
 * @param currentToken The token that caused the error
 * @param msg The message to display with the error
 */
void AlienCPUAssembler::error(AssemblerError error, Token currentToken, std::stringstream msg) {
    std::string name;
    switch(error) {
        case INVALID_TOKEN:
            name = BOLD_RED + "[invalid_token]" + RESET;
            break;
        case MULTIPLE_DEFINITION:
            name = BOLD_RED + "[multiple_definition]" + RESET;
            break;
        case UNRECOGNIZED_TOKEN:
            name = BOLD_RED + "[unrecognized_token]" + RESET;
            break;
        case MISSING_TOKEN:
            name = BOLD_YELLOW + "[missing_token]" + RESET;
        case INTERNAL_ERROR:
            name = BOLD_RED + "[internal_error]" + RESET;
            break;
        default:
            name = BOLD_RED + "[error]" + RESET;
    }

    std::stringstream msgStream;
    msgStream << name << " " << msg.str() << " at line " << currentToken.lineNumber 
            << " \"" << currentToken.string << "\"" << std::endl;

    throw std::runtime_error(msgStream.str());
}


/**
 * Warns about potential bugs in the code
 * 
 * @param warn The type of warning to display
 * @param msg The message to display with the warning
 */
void AlienCPUAssembler::warn(AssemblerWarn warn, std::stringstream msg) {

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
        case TOKENIZING:
            name = BOLD_BLUE + "[tokenizing]" + RESET;
            break;
        case PARSING:
            name = BOLD_GREEN + "[parsing]" + RESET;
            break;
        case ASSEMBLING:
            name = BOLD_MAGENTA + "[assembling]" + RESET;
            break;
        default:
            name = BOLD_CYAN + "[log]" + RESET;
    }

    std::cout << name << " " << msg.str() << std::endl;
}


/**
 * Determines whether the provided token is a string operand
 * 
 * @param token The token to check
 * @return true if the token is a string operand, false otherwise
 */
bool AlienCPUAssembler::isStringToken(std::string token) {
    return token.size() >= 2 && token[0] == '\"' && token[token.size() - 1] == '\"';
}


/**
 * Extracts the string oeprand from the token
 * 
 * @param token The token to extract the string operand from
 * @return The string operand
 */
std::string AlienCPUAssembler::getStringToken(std::string token) {
    if (!isStringToken(token)) {
        error(INTERNAL_ERROR, Token(token, -1, -1), std::stringstream() << "Invalid string token: " << token);
    }

    return token.substr(1, token.size() - 2);
}


/**
 * Check to make sure the filename only contains letters, numbers, spaces, parenthesis, underscores, 
 * dashes, commas, periods, or stars.
 * 
 * @param filename The filename to check for validity
 * @return true if the filename is valid, false otherwise
 */
bool AlienCPUAssembler::isValidFilename(std::string filename) {
    for (char c : filename) {
        if (!std::isalnum(c) && c != ' ' && c != '(' && c != ')' && c != '_' && c != '-' && c != ',' 
            && c != '.' && c != '*') {
            return false;
        }
    }

    return true;
}
