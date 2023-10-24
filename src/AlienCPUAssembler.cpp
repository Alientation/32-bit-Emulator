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
        "globallabel1:\t;this is a comment\n" <<
        "_locallabel:\n" <<
        "globallabel2:\n" <<
        "_locallabel:";

    assembler.betterAssemble(sourceCode.str());
}


/**
 * Constructs a new AlienCPUAssembler for the given AlienCPU.
 */
AlienCPUAssembler::AlienCPUAssembler(AlienCPU& cpu, bool debugOn) : cpu(cpu), debugOn(debugOn) {}


/**
 * Assembles the given assembly source code into machine code and loads it onto the cpu.
 * 
 * TODO: technically should probably load into a .bin file and then manually write it into the cpu
 * Documentation on the assembly language can be found SOMEWHERE
 * 
 * @param source The assembly source code to assemble into machine code.
 */
void AlienCPUAssembler::betterAssemble(std::string source) {
    log(LOG, std::stringstream() << BOLD << BOLD_WHITE << "Assembling..." << RESET);

    // reset assembler state to be ready for a new assembly
    outputFile = "";
    sourceCode = source;
    tokens.clear();
    parsedTokens.clear();

    dataProgramCounter = 0;
    textProgramCounter = 0;
    
    tokenize();

    // for (Token& token : tokens) {
    //     std::cout << token.string << " [" << token.lineNumber << "]" << std::endl;
    // }

    parseTokens();

    for (ParsedToken parsedToken : parsedTokens) {
        if (parsedToken.type == INSTRUCTION) {
            std::cout << "[" << parsedToken.memoryAddress << "]\t" << parsedToken.token.string 
                    << " " << addressingModeNames.at(parsedToken.addressingMode) << std::endl;
        } else if (parsedToken.type == INSTRUCTION_OPERAND || parsedToken.type == GLOBAL_LABEL || parsedToken.type == LOCAL_LABEL) {
            std::cout << "[" << parsedToken.memoryAddress << "]\t" << parsedToken.token.string << std::endl;
        } else {
            std::cout << "\t" << parsedToken.token.string << std::endl;
        }
    }
    

    log(LOG, std::stringstream() << BOLD << BOLD_GREEN << "Successfully Assembled!" << RESET);
}



/**
 * Parses the tokens for assembler directives and to map labels
 */
void AlienCPUAssembler::parseTokens() {
    log(PARSING, std::stringstream() << BOLD << "Parsing Tokens" << RESET);

    // memory segment currently writing to
    SegmentType segment = TEXT;

    // check if there is an operand available
    auto hasOperand = [this](int tokenIndex, bool onSameLine = true) {
        return tokenIndex != tokens.size() - 1 && (!onSameLine || tokens[tokenIndex + 1].lineNumber == tokens[tokenIndex].lineNumber);
    };
    
    for (int tokenI = 0; tokenI < tokens.size(); tokenI++) {
        Token& token = tokens[tokenI];

        // check if the token is a directive
        if (token.string[0] == '.') {
            parsedTokens.push_back(ParsedToken(token, DIRECTIVE));

            if (token.string == ".data") {
                segment = DATA;
            } else if (token.string == ".text") {
                segment = TEXT;
            } else if (token.string == ".outfile") {
                // needs an operand value
                if (!hasOperand(tokenI)) {
                    error(MISSING_TOKEN, token, std::stringstream() << ".outfile directive must be supplied a value");
                }

                // check if outfile has already been defined TODO: determine if we should allow multiple outfiles
                if (!outputFile.empty()) {
                    error(ERROR, token, std::stringstream() << ".outfile directive cannot be defined multiple times in the same file");
                }

                // set the outfile
                outputFile = tokens[++tokenI].string;
                parsedTokens.push_back(ParsedToken(tokens[tokenI], DIRECTIVE_OPERAND));
            } else if (token.string == ".org") {
                // needs an operand value
                if (!hasOperand(tokenI)) {
                    error(MISSING_TOKEN, token, std::stringstream() << ".org directive must be supplied a value");
                }

                // parse org value, must be a value capable of being evaluated in the parse phase
                // ie, any labels referenced must be already defined and any expressions must be evaluated
                Word value = parseValue(tokens[++tokenI]);
                parsedTokens.push_back(ParsedToken(tokens[tokenI], DIRECTIVE_OPERAND));
                
                if (segment == TEXT) {
                    textProgramCounter = value;
                } else if (segment == DATA) {
                    dataProgramCounter = value;
                } else {
                    error(ERROR, token, std::stringstream() << "Invalid segment");
                }
            } else if (token.string == ".db") {

            } else if (token.string == ".d2b") {

            } else if (token.string == ".dw") {

            } else if (token.string == ".d2w") {

            } else if (token.string == ".db*") {

            } else if (token.string == ".d2b*") {

            } else if (token.string == ".dw*") {

            } else if (token.string == ".d2w*") {

            } else if (token.string == ".advance") {

            }

            continue;
        }

        // check if token is label
        if (token.string[token.string.size() - 1] == ':') {




            continue;
        }

        // check if token is instruction
        if (instructionMap.find(token.string) != instructionMap.end()) {
            // no more tokens to parse that are on the same line
            // so either this has no operands or it is missing operands
            if (!hasOperand(tokenI)) {
                AddressingMode addressingMode = NO_ADDRESSING_MODE;
                if (validInstruction(token.string, IMPLIED)) {
                    addressingMode = IMPLIED;
                }

                if (validInstruction(token.string, ACCUMULATOR)) {
                    if (addressingMode != NO_ADDRESSING_MODE) {
                        error(INVALID_TOKEN, token, std::stringstream() << "Multiple Possible Addressing Modes");
                    }
                    addressingMode = ACCUMULATOR;
                }

                // check if instruction is valid if there are no operands
                if (addressingMode == NO_ADDRESSING_MODE) {
                    // missing operands
                    error(MISSING_TOKEN, token, std::stringstream() << "Missing Instruction Operand");
                }

                // check if in proper segment
                if (segment != TEXT) {
                    error(ERROR, token, std::stringstream() << "Instruction must be in the text segment");
                }
                
                // valid instruction
                log(PARSING, std::stringstream() << GREEN << "Instruction\t" << RESET << "[" << token.string << "]");
                parsedTokens.push_back(ParsedToken(token, INSTRUCTION, textProgramCounter, addressingMode));
                textProgramCounter++; // no extra bytes for operands
                continue;
            }

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
            parsedTokens.push_back(ParsedToken(token, INSTRUCTION, textProgramCounter, addressingMode));
            textProgramCounter++; // 1 byte for the instruction

            parsedTokens.push_back(ParsedToken(operandToken, INSTRUCTION_OPERAND, textProgramCounter));
            textProgramCounter+= addressingModeOperandBytes.at(addressingMode); // bytes for the operand
            continue;
        }

        // unrecognized token
        error(INVALID_TOKEN, token, std::stringstream() << "Unrecognized Token While Parsing");
    }

    log(PARSING, std::stringstream() << BOLD_GREEN << "Parsed Tokens" << RESET);
}



/**
 * Converts the given operand to an addressing mode.
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
            // error(ERROR, tokenInstruction, std::stringstream() << "Invalid instruction for addressing mode RELATIVE or IMMEDIATE: " << tokenInstruction.string);
            return NO_ADDRESSING_MODE;
        }
    }

    // check if operand is zeropage or absolute
    if (operand[0] == '%' || operand[0] == '0' || operand[0] == '$' || isNumber(operand)) {
        u64 parsedValue = parseValue(tokenOperand);
        if (parsedValue > 0xFFFFFFFF) {
            // error(ERROR, tokenOperand, std::stringstream() << "Invalid zeropage or absolute value: " << operand);
            return NO_ADDRESSING_MODE;
        }

        if (parsedValue <= 0xFFFF && validInstruction(tokenInstruction.string, ZEROPAGE)) {
            return ZEROPAGE;
        } else if (validInstruction(tokenInstruction.string, ABSOLUTE)) {
            return ABSOLUTE;
        } else {
            // error(ERROR, tokenInstruction, std::stringstream() << "Invalid instruction for addressing mode ZEROPAGE or ABSOLUTE: " << tokenInstruction.string);
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
            // error(ERROR, tokenOperand, std::stringstream() << "Invalid zeropage,x or zeropage,y or absolute,x or absolute,y value: " << operand);
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
            // error(ERROR, tokenInstruction, std::stringstream() << "Invalid instruction for addressing mode ZEROPAGE_X, ABSOLUTE_X, ZEROPAGE_Y, or ABSOLUTE_Y: " << tokenInstruction.string);
            return NO_ADDRESSING_MODE;
        }
    }

    // check if operand is indirect
    if (operand[0] == '(' && operand[operand.size() - 1] == ')' && isHexadecimalNumber(operand.substr(1,operand.size() - 2))) {
        Token valueToken = Token(tokenOperand);
        valueToken.string = operand.substr(1,operand.size() - 2);

        u64 parsedValue = parseValue(valueToken);
        if (parsedValue > 0xFFFFFFFF) {
            // error(ERROR, tokenOperand, std::stringstream() << "Invalid indirect value: " << operand);
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
            // error(ERROR, tokenOperand, std::stringstream() << "Invalid x indexed indirect value: " << operand);
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
            // error(ERROR, tokenOperand, std::stringstream() << "Invalid indirect y indexed value: " << operand);
            return NO_ADDRESSING_MODE;
        }

        return INDIRECT_YINDEXED;
    }

    // invalid operand
    // error(ERROR, tokenInstruction, std::stringstream() << "Invalid operand to convert: " << operand);
    return NO_ADDRESSING_MODE;
}




void AlienCPUAssembler::evaluateExpression(Token token) {

}


/**
 * 
 */
u64 AlienCPUAssembler::parseValue(const Token token) {
    if (token.string.empty()) {
        error(ERROR, token, std::stringstream() << "Invalid empty value to parse: " << token.string);
    }

    // remove the value symbol if it is present
    std::string value = token.string[0] == '#' ? token.string.substr(1) : token.string;
    
    // check if value is empty
    if (value.empty()) {
        error(ERROR, token, std::stringstream() << "Invalid value to parse: " << value);
    }


    // check if it references a non numeric value
    if (!isHexadecimalNumber(value.substr(1)) && !isNumber(value)) {










        error(ERROR, token, std::stringstream() << "Unsupported Non-numeric Value: " << value);
    }


    // hexadecimal
    if (value[0] == '$') {
        // check it contains only 0-9 or A-F characters
        std::string numericValue = value.substr(1);
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
            error(ERROR, token, std::stringstream() << "Invalid Hexadecimal Digit '" << *it << "': " << numericValue);
        }

        // proper hexadecimal value
        return number;
    } else if (value[0] == '0') {
        // check it contains only 0-7 characters
        std::string numericValue = value.substr(1);
        u64 number = 0;
        std::string::const_iterator it = numericValue.begin();
        while (it != numericValue.end() && (*it) >= '0' && (*it) <= '7') {
            number *= 8;
            number += (*it) - '0';
            ++it;
        }
        
        // contains other characters
        if (it != numericValue.end()) {
            error(ERROR, token, std::stringstream() << "Invalid Octal Digit '" << *it << "': " << numericValue);
        }

        // proper octal value
        return number;
    } else if (value[0] == '%') {
        // check it contains only 0-1 characters
        std::string numericValue = value.substr(1);
        u64 number = 0;
        std::string::const_iterator it = numericValue.begin();
        while (it != numericValue.end() && (*it) >= '0' && (*it) <= '1') {
            number *= 2;
            number += (*it) - '0';
            ++it;
        }
        
        // contains other characters
        if (it != numericValue.end()) {
            error(ERROR, token, std::stringstream() << "Invalid Binary Digit '" << *it << "': " << numericValue);
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
            error(ERROR, token, std::stringstream() << "Invalid Decimal Digit '" << *it << "': " << value);
        }

        // proper decimal value
        return number;
    }

    error(ERROR, token, std::stringstream() << "Unsupported Numeric Value: " << value);
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
    log(TOKENIZING, std::stringstream() << BOLD << "Tokenizing Source Code" << RESET);
    
    bool isSingleLineComment = false;
    bool isMultiLineComment = false;
    int currentTokenStart = -1;
    int lineNumber = 0;
    std::string currentToken = "";

    // default true for first column on each line. Every subsequent column should have a preceeding
    // tab character
    bool readyForNextToken = true;
    for (int charLocation = 0; charLocation < sourceCode.size(); charLocation++) {
        char character = sourceCode[charLocation];
        if (character == '\n') {
            lineNumber++;
        }

        // skip whitespace until we find a token to tokenize
        // this will trim any leading whitespace
        if (readyForNextToken && std::isspace(character)) {
            continue;
        }

        // add the current token to tokens
        currentToken += character;

        // check to end current built token
        if (!isMultiLineComment && (character == '\n' || character == '\t' || charLocation == sourceCode.size() - 1)) {
            // end current token
            if (!isSingleLineComment) {
                // trim any trailing whitespace
                tokens.push_back(Token(trim(currentToken), currentTokenStart, character == '\n' ? lineNumber - 1 : lineNumber));
                log(TOKENIZING, std::stringstream() << CYAN << "Token\t" << RESET << "[" << currentToken << "]");
            } else {
                log(TOKENIZING, std::stringstream() << GREEN << "Comment\t" << RESET << "[" << currentToken << "]");
            }

            currentToken.clear();
            currentTokenStart = -1;

            // prepare for next token
            readyForNextToken = true;
            
            // end single line comment
            if (character == '\n') {
                isSingleLineComment = false;
            }
            continue;
        }

        // first character of token
        if (readyForNextToken) {
            currentTokenStart = charLocation;
            readyForNextToken = false;

            // check if token is a comment
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


    if (isMultiLineComment) {
        error(MISSING_TOKEN, Token(currentToken, currentTokenStart, lineNumber), 
                std::stringstream() << "Multiline comment is not closed by \'*;\'");
    }

    if (currentToken.size() != 0) {
        error(ERROR, Token(currentToken, currentTokenStart, OLDcurrentLine),
                std::stringstream() << "Current token has not been processed");
    }

    log(TOKENIZING, std::stringstream() << BOLD_GREEN << "Tokenized\t" << RESET << tostring(tokens));
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
            name = BOLD_RED + "[INVALID_TOKEN]" + RESET;
            break;
        case MISSING_TOKEN:
            name = BOLD_YELLOW + "[MISSING_TOKEN]" + RESET;
        default:
            name = BOLD_RED + "[ERROR]" + RESET;
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
            name = BOLD_BLUE + "[TOKENIZING]" + RESET;
            break;
        case PARSING:
            name = BOLD_GREEN + "[PARSING]" + RESET;
            break;
        case ASSEMBLING:
            name = BOLD_MAGENTA + "[ASSEMBLING]" + RESET;
            break;
        default:
            name = BOLD_CYAN + "[LOG]" + RESET;
    }

    std::cout << name << " " << msg.str() << std::endl;
}




























/**
 * Assembles the given assembly source code into machine code and loads it onto the cpu.
 * TODO: technically should probably load into a .bin file and then manually write it into the cpu
 * 
 * Documentation on the assembly language can be found SOMEWHERE
 * 
 * @param source The assembly source code to assemble into machine code.
 */
void AlienCPUAssembler::assemble(std::string source) {
    std::cout << "ASSEMBLING..." << std::endl;

    // reset output file;
    outputFile = "";

    // reset assembler state to be ready for a new assembly
    // the source code to assemble
    sourceCode = source;

    // memory address of the next program instruction to be assembled
    OLDlocationPointer = 0x00000000;

    // split the source code into lines
    OLDlines = split(source, '\n');
    OLDcurrentLineTokens.clear();
    OLDpreviousGlobalLabel = "";


    // labels that reference code locations
    OLDglobalCodeLabels.clear(); // can be referenced from anywhere in the program
    OLDlocalCodeLabels.clear(); // can only be referenced locally in a subroutine (between two global labels)

    // processed value labels (labels that reference values)
    OLDglobalValueLabels.clear();
    OLDlocalValueLabels.clear();

    // value labels expressions that have not be processed yet
    OLDglobalUnprocessedValueLabels.clear();
    OLDlocalUnprocessedValueLabels.clear();


    // iterate over each line
    for (OLDcurrentLine = 0; OLDcurrentLine < OLDlines.size(); OLDcurrentLine++) {
        std::cout << std::endl << " PARSING LINE " << OLDcurrentLine << ": " << OLDlines[OLDcurrentLine] << std::endl;
        OLDparseLine(OLDlines[OLDcurrentLine]);
        std::cout << "   >> PARSED LINE " << OLDcurrentLine << std::endl;

    }

    // process unprocessed value labels sequentially from global to local in order of appearance
    for (LabelExpressionPair labelExpressionPair : OLDglobalUnprocessedValueLabels) {
        //evaluateExpression(labelExpressionPair);
    }

    for (LabelExpressionPair labelExpressionPair : OLDlocalUnprocessedValueLabels) {
        //evaluateExpression(labelExpressionPair);
    }


    /*
        USE TOKENIZED LINES TO ASSEMBLE INTO BINARY FILE
    */

    // iterate over each line for the second pass and fill in values for labels
    // this will write to the memory
    OLDlocationPointer = 0x00000000;
    for (OLDcurrentLine = 0; OLDcurrentLine < OLDlines.size(); OLDcurrentLine++) {
        OLDassembleLine(OLDlines[OLDcurrentLine]);
    }

    std::cout << std::endl << "SUCCESSFULLY ASSEMBLED! " << std::endl;
}



/**
 * Converts a string value into a number
 * 
 * To signify a base value, the number must be preceded by a special character representing the base
 * Binary: '%'
 * Octal: '0'
 * Decimal: (default, no need to preceed with a special character)
 * Hexadecimal: '$'
 * 
 * @param value string to convert to number
 * @return numeric representation of the string
 */
u64 AlienCPUAssembler::OLDparseValue(const std::string& value) {
    if (value.empty()) {
        throw std::runtime_error("Invalid value to parse: " + value);
    }

    std::string numericValue = value.substr(1);

    // hexadecimal
    if (value[0] == '$') {
        std::string::const_iterator it = numericValue.begin();
        // check it contains only 0-9 or A-F characters
        u64 number = 0;
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
            std::stringstream ss;
            ss << "Invalid hexadecimal digit \'" << *it << "\': " << numericValue;
            throw std::runtime_error(ss.str());
        }

        // proper hexadecimal value
        return number;
    }

    
    // must be an expression of some sort
    if (!isNumber(numericValue)) {
        throw std::runtime_error("Invalid value to parse (Unsupported Non-numeric Value): " + numericValue);
    }

    // must be a number of some base
    u64 number = 0;
    std::string::const_iterator it = numericValue.begin();
    switch(value[0]) {
        case '%':
            // base 2
            while (it != numericValue.end() && (*it) >= '0' && (*it) <= '1') {
                number *= 2;
                number += (*it) - '0';
                ++it;
            }
            
            // contains other characters
            if (it != numericValue.end()) {
                std::stringstream ss;
                ss << "Invalid binary digit \'" << *it << "\': " << numericValue;
                throw std::runtime_error(ss.str());
            }

            break;
        case '0':
            // base 8
            while (it != numericValue.end() && (*it) >= '0' && (*it) <= '7') {
                number *= 8;
                number += (*it) - '0';
                ++it;
            }
            
            // contains other characters
            if (it != numericValue.end()) {
                std::stringstream ss;
                ss << "Invalid octal digit \'" << *it << "\': " << numericValue;
                throw std::runtime_error(ss.str());
            }

            break;
        default:
            number = std::stoi(value);
            break;
    }

    return number;
}


/**
 * Assembles the given line.
 * 
 * @param line The line to assemble.
 */
void AlienCPUAssembler::OLDparseLine(std::string& line) {
    // check if line is empty or a comment
    if (line.empty() || line[0] == ';') {
        std::cout << "   >> NO CODE LINE" << std::endl;
        return;
    }

    // split the line into tokens delimited by at minimum a tab
    OLDcurrentLineTokens.clear();
    for (std::string token : split(line, '\t')) {
        // remove all whitespace from token
        token.erase(std::remove_if(token.begin(), token.end(), isspace), token.end());

        // only add token if it is not empty and not a comment or it is the first token
        // this will help to allign tokens so that the first token is always the label
        if ((!token.empty() || OLDcurrentLineTokens.empty()) && token[0] != ';') {
            OLDcurrentLineTokens.push_back(token);
        }
    }
    
    std::cout << "   >> TOKENS: " << std::endl;
    for (int tokenI = 0; tokenI < OLDcurrentLineTokens.size(); tokenI++) {
        std::cout << "\t" <<tokenI << ": " << OLDcurrentLineTokens[tokenI] << " " << std::endl;
    }

    // there should be code tokens when the line is not empty
    if (OLDcurrentLineTokens.empty()) {
        throw std::runtime_error("Invalid Empty Tokens: " + tostring(OLDcurrentLineTokens) + " -> " + line);
    }


    // process assembler directives, could potentially be local labels, but we don't know for sure
    if (OLDcurrentLineTokens[0][0] == '#' && OLDparseAssemblerDirective()) {
        std::cout << "   >> PARSED ASSEMBLER DIRECTIVE: " << OLDcurrentLineTokens[0] << std::endl;
        return;
    }
    
    // tokens should now be alligned so that the first token is the label if it exists
    if (OLDparseLabel(OLDcurrentLineTokens[0])) {
        return;
    }

    // only label on the current line
    if (OLDcurrentLineTokens.size() == 1) {
        std::cout << "   >> ONLY LABEL LINE" << std::endl;
        return;
    }


    // parse instruction


    
    
}


/**
 * Checks whether the current tokenized line is an assembler directive. Parses the directive if
 * it is a valid assembler directive and returns true. Otherwise returns false.
 * 
 * @return whether the current tokenized line is an assembler directive
 */
bool AlienCPUAssembler::OLDparseAssemblerDirective() {
    // OUTFILE filename
    // Set the output file name
    if (OLDcurrentLineTokens[0] == ".outfile") {
        if (OLDcurrentLineTokens.size() == 1) {
            throw std::runtime_error("OUTFILE Directive must be supplied a value: " + OLDlines[OLDcurrentLine]);
        }

        if (outputFile != "") {
            throw std::runtime_error("OUTFILE Directive multiple definition: " + OLDcurrentLineTokens[1] + " (" + outputFile + ")");
        }
        outputFile = OLDcurrentLineTokens[1];

        if (OLDcurrentLineTokens.size() > 2) {
            throw std::runtime_error("Expected new line: " + OLDcurrentLineTokens[2]);
        }
        return true;
    }


    // ORG value
    // Set the current program location in memory to the specified value
    if (OLDcurrentLineTokens[0] == ".org") {
        std::cout << "   >> ORG Statement" << std::endl;
        
        if (OLDcurrentLineTokens.size() == 1) {
            throw std::runtime_error("ORG Directive must be supplied a value: " + OLDlines[OLDcurrentLine]);
        }

        u64 parsedValue = OLDparseValue(OLDcurrentLineTokens[1]);
        if (parsedValue > 0xFFFFFFFF) {
            throw std::runtime_error("Invalid ORG Directive value: " + OLDcurrentLineTokens[1] + 
                    "\nProgram address origin must be within 0xFFFFFFFF");
        }

        std::cout << "   >> MOVING LOCATION POINTER TO " << stringifyHex((Word) parsedValue) << std::endl;
        OLDlocationPointer = (Word) parsedValue;

        if (OLDcurrentLineTokens.size() > 2) {
            throw std::runtime_error("Expected new line: " + OLDcurrentLineTokens[2]);
        }
        return true;
    }


    // DB [value,value,...,value]
    // Define a sequence of bytes at the current program location in memory
    if (OLDcurrentLineTokens[0] == ".db" || OLDcurrentLineTokens[0] == ".byte") {

    }
    
    // D2B [value,value,...,value]
    // Stores in little endian format
    if (OLDcurrentLineTokens[0] == ".d2b" || OLDcurrentLineTokens[0] == ".2byte") {

    }

    // D2B-BE [value,value,...,value]
    // Stores in big endian format

    // DW [value,value,...,value]
    // stores in little endian format
    if (OLDcurrentLineTokens[0] == ".dw" || OLDcurrentLineTokens[0] == ".word") {

    }
    
    // DW-BE [value,value,...,value]
    // stores in big endian format

    // D2W [value,value,...,value]
    // stores in little endian format

    // D2W-BE [value,value,...,value]
    // stores in big endian format



    // ADVANCE target [filler bytesWidth]
    // Advance the current program location in memory to a specified location and fills in bytes 
    // with a filler value. Default filler value is 0
    if (OLDcurrentLineTokens[0] == ".advance") {
        if (OLDcurrentLineTokens.size() == 1) {
            throw std::runtime_error("ADVANCE Directive must be supplied a value: " + OLDlines[OLDcurrentLine]);
        }

        std::vector<std::string> splitByComma = split(OLDcurrentLineTokens[1], ' ');

        u64 parsedValue = OLDparseValue(splitByComma[0]);
        if (parsedValue > 0xFFFFFFFF) {
            throw std::runtime_error("Invalid ADVANCE Directive value: " + splitByComma[0] + 
                    "\nProgram address origin must be within 0xFFFFFFFF");
        }

        Word targetLocation = (Word) parsedValue;
        if (targetLocation <= OLDlocationPointer) {
            throw std::runtime_error("Invalid ADVANCE Directive value: " + splitByComma[0] + 
                    "\nProgram address origin must be greater than the current location pointer");
        }

        u64 filler = 0;
        u8 bytesWidth = 1;
        if (splitByComma.size() > 1) {
            filler = OLDparseValue(splitByComma[1]);

            // count how many bytes to represent the filler value
            u64 temp = filler;
            u8 countBytes = 0;
            while (temp > 0) {
                countBytes++;
                temp >>= 8;
            }

            if (splitByComma.size() > 2) {
                u64 parsedBytesWidth = OLDparseValue(splitByComma[2]);
                
                if (parsedBytesWidth > 7) {
                    throw std::runtime_error("Invalid ADVANCE Directive bytes width: " + splitByComma[2] + 
                            "\nBytes width must be within 0-7");
                }
                bytesWidth = (u8) parsedBytesWidth;
            } else {
                bytesWidth = countBytes;
            }

            // if the target region cannot be split without extra space with the filler value
            if ((targetLocation - OLDlocationPointer) % bytesWidth != 0) {
                throw std::runtime_error("Invalid ADVANCE Directive filler value: " + splitByComma[1] + 
                        "\nFiller value must be be able to fit completely within the target filled region\n" +
                        "Filler value: " + std::to_string(filler) + " (" + stringifyHex(filler) + ")\n" +
                        "Filler value size: " + std::to_string(countBytes) + " bytes\n" +
                        "Target filled region size: " + std::to_string(targetLocation - OLDlocationPointer) + " bytes");
            }

            if (splitByComma.size() > 3) {
                throw std::runtime_error("Expected new line: " + splitByComma[2]);
            }
        }

        // advance location pointer, don't write any bytes yet since this is the first pass
        OLDlocationPointer = targetLocation;

        if (OLDcurrentLineTokens.size() > 2) {
            throw std::runtime_error("Expected new line: " + OLDcurrentLineTokens[2]);
        }

        return true;
    }


    // FILL end [filler]
    // Fill a specified region of memory with a filler value. Default filler value is 0


    // SPACE label value
    // Define a label to the current location and advances the current location by value
    

    // DEFINE label value
    // Define a label with a value, can only reference other previously defined labels 
    // and of correct scope
    // TODO: remove the EQU directive and move it here. 
    // All directives should be represented with .<directive name> in the first column


    // CHECKPC value
    // Check if the current program location in memory is less than or equal to a specified value


    // ALIGN value
    // Align the current program location in memory to a specified value


    // DATA
    // Define a data segment. If not defined with a label then the data segment is the default data segment


    // TEXT [label]


    // INCBIN [offest length]
    // Include a binary file into the assembly source code. The offset and length are optional


    // INCLUDE filename
    // Include another assembly source code file into the current assembly source code


    // REQUIRE filename
    // Include another assembly source code file into the current assembly source code.
    // Only includes the file once in the current assembly source code


    // SCOPE
    // Define a new scope for local labels


    // SCEND
    // End the current scope for local labels


    // MACRO macrolabel [argumentlabel,argumentlabel,...,argumentlabel]
    // Define a macro scope with a label and argument labels
    // will be inlined into the invoke call


    // MACEND
    // End the current macro scope


    // INVOKE macrolabel [argument,argument,...,argument]
    // Invoke a macro with a label and arguments


    // ASSERT value1 value2
    // Assert that value1 is equal to value2


    // ERROR message
    // User created error message


    // ERRORIF value message
    // User created error message if value is true


    // IFDEF label
    // If the label is defined then the following code is assembled


    // IFNDEF label
    // If the label is not defined then the following code is assembled


    // ELSE
    // If the previous IFDEF or IFNDEF directive was not assembled then the following code is assembled


    // ENDIF
    // End the previous IFDEF or IFNDEF directive


    // PRINT message
    // Print a message to the console on the last pass


    // PRINTIF value message
    // Print a message to the console on the last pass if value is true


    // PRINTNOW message
    // Print a message to the console immediately


    return false;
}



/**
 * Assembles the given label.
 * 
 * @param label The label to assemble.
 * @return true if the whole line was parsed already
 */
bool AlienCPUAssembler::OLDparseLabel(std::string& label) {
    if (OLDcurrentLineTokens.size() != 1 && label.empty()) {
        std::cout << "   >> NO LABEL LINE" << std::endl;
        return false;
    }

    if (label.empty()) {
        throw std::runtime_error("Invalid Empty Label: " + label + " -> " + OLDlines[OLDcurrentLine]);
    }

    // check if label is a local label, designated by '_' as the first character
    bool isLocalLabel = label[0] == '_';

    // label is a value label
    if (OLDcurrentLineTokens.size() > 1 && OLDcurrentLineTokens[2] == "equ") {
        // The value label definition line is composed of only three tokens, 
        // label name, the directive (EQU), and the value to be stored in the label
        if (OLDcurrentLineTokens.size() == 2) {
            throw std::runtime_error("Value Label must be supplied a value: " + label + " -> " + OLDlines[OLDcurrentLine]);
        }

        // add to correct unproccessed label lists
        if (isLocalLabel) {
            OLDlocalUnprocessedValueLabels.push_back(LabelExpressionPair(label, OLDcurrentLineTokens[3]));
        } else {
            OLDglobalUnprocessedValueLabels.push_back(LabelExpressionPair(label, OLDcurrentLineTokens[3]));
        }

        if (OLDcurrentLineTokens.size() > 3) {
            throw std::runtime_error("Expected new line: " + OLDcurrentLineTokens[3]);
        }
        
        std::cout << "   >> VALUE LABEL LINE" << std::endl;
        return true;
    }

    // label is a code label
    if (isLocalLabel) {
        // check if local label is defined under a global label scope
        if (OLDpreviousGlobalLabel.empty()) {
            throw std::runtime_error("Invalid Local Code Label: " + label + 
                    " cannot be defined before a global label is defined");
        }

        // check if local label name is valid at the current global label scope
        bool isLocalLabelNameUnique = OLDlocalCodeLabels.find(label) != OLDlocalCodeLabels.end();
        if (isLocalLabelNameUnique && 
                OLDlocalCodeLabels.at(label).find(OLDpreviousGlobalLabel) != OLDlocalCodeLabels.at(label).end()) {
            throw std::runtime_error("Duplicate Local Label In Same Scope: " + label + " (" + OLDpreviousGlobalLabel + ")");
        }

        // create local label scope mapping if not already present
        if (isLocalLabelNameUnique) {
            OLDlocalCodeLabels[label] = std::map<std::string, Word>();
        }

        // map local label to correct global label scope
        OLDlocalCodeLabels[label][OLDpreviousGlobalLabel] = OLDlocationPointer;
    } else {
        // check if global label has already been defined
        if (OLDglobalCodeLabels.find(label) != OLDglobalCodeLabels.end()) {
            throw std::runtime_error("Duplicate Global Label: " + label);
        }

        // add global label to map
        OLDglobalCodeLabels[label] = OLDlocationPointer;
        OLDpreviousGlobalLabel = label;
    }

    std::cout << "   >> CODE LABEL LINE" << std::endl;
    return false;
}


/**
 * Assembles the given instruction.
 * 
 * @param instruction The instruction to assemble.
 */
void AlienCPUAssembler::OLDparseInstruction(std::string& instruction) {

}


/**
 * Assembles the given line into machine code.
 * 
 * @param line The line to assemble.
 */
void AlienCPUAssembler::OLDassembleLine(std::string& line) {

}


/**
 * Assembles the given instruction into machine code.
 * 
 * @param instruction The instruction to assemble.
 */
void AlienCPUAssembler::OLDassembleInstruction(std::string& instruction) {

}