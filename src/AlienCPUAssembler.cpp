#include "AlienCPUAssembler.h"


int main() {
    AlienCPU cpu;
    AlienCPUAssembler assembler(cpu, true);

    std::stringstream sourceCode;
    sourceCode << 
        "\tLDA\t\t \t#$FFFF\n" << 
        ";*THIS IS A COMMENT\n" << 
        ";SO IS THIS\n" << 
        ".org\tB0101\n" <<
        "globallabel1:\t;this is a comment\n" <<
        "_locallabel:\n" <<
        "globallabel2:\n" <<
        "_locallabel:\n";

    assembler.betterAssemble(sourceCode.str());
}


/**
 * Constructs a new AlienCPUAssembler for the given AlienCPU.
 */
AlienCPUAssembler::AlienCPUAssembler(AlienCPU& cpu, bool debugOn) : cpu(cpu), debugOn(debugOn) {}


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
    locationPointer = 0x00000000;

    // split the source code into lines
    lines = split(source, '\n');
    currentLineTokens.clear();
    previousGlobalLabel = "";


    // labels that reference code locations
    globalCodeLabels.clear(); // can be referenced from anywhere in the program
    localCodeLabels.clear(); // can only be referenced locally in a subroutine (between two global labels)

    // processed value labels (labels that reference values)
    globalValueLabels.clear();
    localValueLabels.clear();

    // value labels expressions that have not be processed yet
    globalUnprocessedValueLabels.clear();
    localUnprocessedValueLabels.clear();


    // iterate over each line
    for (currentLine = 0; currentLine < lines.size(); currentLine++) {
        std::cout << std::endl << " PARSING LINE " << currentLine << ": " << lines[currentLine] << std::endl;
        parseLine(lines[currentLine]);
        std::cout << "   >> PARSED LINE " << currentLine << std::endl;

    }

    // process unprocessed value labels sequentially from global to local in order of appearance
    for (LabelExpressionPair labelExpressionPair : globalUnprocessedValueLabels) {
        evaluateExpression(labelExpressionPair);
    }

    for (LabelExpressionPair labelExpressionPair : localUnprocessedValueLabels) {
        evaluateExpression(labelExpressionPair);
    }


    /*
        USE TOKENIZED LINES TO ASSEMBLE INTO BINARY FILE
    */

    // iterate over each line for the second pass and fill in values for labels
    // this will write to the memory
    locationPointer = 0x00000000;
    for (currentLine = 0; currentLine < lines.size(); currentLine++) {
        assembleLine(lines[currentLine]);
    }

    std::cout << std::endl << "SUCCESSFULLY ASSEMBLED! " << std::endl;
}





void AlienCPUAssembler::betterAssemble(std::string source) {
    log(LOG, std::stringstream() << BOLD << BOLD_WHITE << "Assembling..." << RESET);

    // reset assembler state to be ready for a new assembly
    outputFile = "";
    sourceCode = source;
    tokens.clear();
    
    tokenize();
    

    log(LOG, std::stringstream() << BOLD << BOLD_GREEN << "Successfully Assembled!" << RESET);
}




void AlienCPUAssembler::parseTokens() {
    for (Token token : tokens) {



    }
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


        // check to end current built token
        if (!isMultiLineComment && (character == '\n' || character == '\t' || charLocation == sourceCode.size() - 1)) {
            // end current token
            if (!isSingleLineComment) {
                // trim any trailing whitespace
                tokens.push_back(Token(trim(currentToken), currentTokenStart, lineNumber));
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

        // add the current token to tokens
        currentToken += character;

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
        error(MISSING_TOKEN, Token(currentToken, currentTokenStart, currentLine), 
                std::stringstream() << "Multiline comment is not closed by \'*;\'\n");
    }

    if (currentToken.size() != 0) {
        error(ERROR, Token(currentToken, currentTokenStart, currentLine),
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
    msgStream << name << " \"" << msg.str() << "\"\nat " << currentToken.token  
            << " in line " << currentToken.lineNumber << std::endl;

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
 * Assembles the given line.
 * 
 * @param line The line to assemble.
 */
void AlienCPUAssembler::parseLine(std::string& line) {
    // check if line is empty or a comment
    if (line.empty() || line[0] == ';') {
        std::cout << "   >> NO CODE LINE" << std::endl;
        return;
    }

    // split the line into tokens delimited by at minimum a tab
    currentLineTokens.clear();
    for (std::string token : split(line, '\t')) {
        // remove all whitespace from token
        token.erase(std::remove_if(token.begin(), token.end(), isspace), token.end());

        // only add token if it is not empty and not a comment or it is the first token
        // this will help to allign tokens so that the first token is always the label
        if ((!token.empty() || currentLineTokens.empty()) && token[0] != ';') {
            currentLineTokens.push_back(token);
        }
    }
    
    std::cout << "   >> TOKENS: " << std::endl;
    for (int tokenI = 0; tokenI < currentLineTokens.size(); tokenI++) {
        std::cout << "\t" <<tokenI << ": " << currentLineTokens[tokenI] << " " << std::endl;
    }

    // there should be code tokens when the line is not empty
    if (currentLineTokens.empty()) {
        throw std::runtime_error("Invalid Empty Tokens: " + tostring(currentLineTokens) + " -> " + line);
    }


    // process assembler directives, could potentially be local labels, but we don't know for sure
    if (currentLineTokens[0][0] == '#' && parseAssemblerDirective()) {
        std::cout << "   >> PARSED ASSEMBLER DIRECTIVE: " << currentLineTokens[0] << std::endl;
        return;
    }
    
    // tokens should now be alligned so that the first token is the label if it exists
    if (parseLabel(currentLineTokens[0])) {
        return;
    }

    // only label on the current line
    if (currentLineTokens.size() == 1) {
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
bool AlienCPUAssembler::parseAssemblerDirective() {
    // OUTFILE filename
    // Set the output file name
    if (currentLineTokens[0] == ".outfile") {
        if (currentLineTokens.size() == 1) {
            throw std::runtime_error("OUTFILE Directive must be supplied a value: " + lines[currentLine]);
        }

        if (outputFile != "") {
            throw std::runtime_error("OUTFILE Directive multiple definition: " + currentLineTokens[1] + " (" + outputFile + ")");
        }
        outputFile = currentLineTokens[1];

        if (currentLineTokens.size() > 2) {
            throw std::runtime_error("Expected new line: " + currentLineTokens[2]);
        }
        return true;
    }


    // ORG value
    // Set the current program location in memory to the specified value
    if (currentLineTokens[0] == ".org") {
        std::cout << "   >> ORG Statement" << std::endl;
        
        if (currentLineTokens.size() == 1) {
            throw std::runtime_error("ORG Directive must be supplied a value: " + lines[currentLine]);
        }

        u64 parsedValue = parseValue(currentLineTokens[1]);
        if (parsedValue > 0xFFFFFFFF) {
            throw std::runtime_error("Invalid ORG Directive value: " + currentLineTokens[1] + 
                    "\nProgram address origin must be within 0xFFFFFFFF");
        }

        std::cout << "   >> MOVING LOCATION POINTER TO " << stringifyHex((Word) parsedValue) << std::endl;
        locationPointer = (Word) parsedValue;

        if (currentLineTokens.size() > 2) {
            throw std::runtime_error("Expected new line: " + currentLineTokens[2]);
        }
        return true;
    }


    // DB [value,value,...,value]
    // Define a sequence of bytes at the current program location in memory
    if (currentLineTokens[0] == ".db" || currentLineTokens[0] == ".byte") {

    }
    
    // D2B [value,value,...,value]
    // Stores in little endian format
    if (currentLineTokens[0] == ".d2b" || currentLineTokens[0] == ".2byte") {

    }

    // D2B-BE [value,value,...,value]
    // Stores in big endian format

    // DW [value,value,...,value]
    // stores in little endian format
    if (currentLineTokens[0] == ".dw" || currentLineTokens[0] == ".word") {

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
    if (currentLineTokens[0] == ".advance") {
        if (currentLineTokens.size() == 1) {
            throw std::runtime_error("ADVANCE Directive must be supplied a value: " + lines[currentLine]);
        }

        std::vector<std::string> splitByComma = split(currentLineTokens[1], ' ');

        u64 parsedValue = parseValue(splitByComma[0]);
        if (parsedValue > 0xFFFFFFFF) {
            throw std::runtime_error("Invalid ADVANCE Directive value: " + splitByComma[0] + 
                    "\nProgram address origin must be within 0xFFFFFFFF");
        }

        Word targetLocation = (Word) parsedValue;
        if (targetLocation <= locationPointer) {
            throw std::runtime_error("Invalid ADVANCE Directive value: " + splitByComma[0] + 
                    "\nProgram address origin must be greater than the current location pointer");
        }

        u64 filler = 0;
        u8 bytesWidth = 1;
        if (splitByComma.size() > 1) {
            filler = parseValue(splitByComma[1]);

            // count how many bytes to represent the filler value
            u64 temp = filler;
            u8 countBytes = 0;
            while (temp > 0) {
                countBytes++;
                temp >>= 8;
            }

            if (splitByComma.size() > 2) {
                u64 parsedBytesWidth = parseValue(splitByComma[2]);
                
                if (parsedBytesWidth > 7) {
                    throw std::runtime_error("Invalid ADVANCE Directive bytes width: " + splitByComma[2] + 
                            "\nBytes width must be within 0-7");
                }
                bytesWidth = (u8) parsedBytesWidth;
            } else {
                bytesWidth = countBytes;
            }

            // if the target region cannot be split without extra space with the filler value
            if ((targetLocation - locationPointer) % bytesWidth != 0) {
                throw std::runtime_error("Invalid ADVANCE Directive filler value: " + splitByComma[1] + 
                        "\nFiller value must be be able to fit completely within the target filled region\n" +
                        "Filler value: " + std::to_string(filler) + " (" + stringifyHex(filler) + ")\n" +
                        "Filler value size: " + std::to_string(countBytes) + " bytes\n" +
                        "Target filled region size: " + std::to_string(targetLocation - locationPointer) + " bytes");
            }

            if (splitByComma.size() > 3) {
                throw std::runtime_error("Expected new line: " + splitByComma[2]);
            }
        }

        // advance location pointer, don't write any bytes yet since this is the first pass
        locationPointer = targetLocation;

        if (currentLineTokens.size() > 2) {
            throw std::runtime_error("Expected new line: " + currentLineTokens[2]);
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
bool AlienCPUAssembler::parseLabel(std::string& label) {
    if (currentLineTokens.size() != 1 && label.empty()) {
        std::cout << "   >> NO LABEL LINE" << std::endl;
        return false;
    }

    if (label.empty()) {
        throw std::runtime_error("Invalid Empty Label: " + label + " -> " + lines[currentLine]);
    }

    // check if label is a local label, designated by '_' as the first character
    bool isLocalLabel = label[0] == '_';

    // label is a value label
    if (currentLineTokens.size() > 1 && currentLineTokens[2] == "equ") {
        // The value label definition line is composed of only three tokens, 
        // label name, the directive (EQU), and the value to be stored in the label
        if (currentLineTokens.size() == 2) {
            throw std::runtime_error("Value Label must be supplied a value: " + label + " -> " + lines[currentLine]);
        }

        // add to correct unproccessed label lists
        if (isLocalLabel) {
            localUnprocessedValueLabels.push_back(LabelExpressionPair(label, currentLineTokens[3]));
        } else {
            globalUnprocessedValueLabels.push_back(LabelExpressionPair(label, currentLineTokens[3]));
        }

        if (currentLineTokens.size() > 3) {
            throw std::runtime_error("Expected new line: " + currentLineTokens[3]);
        }
        
        std::cout << "   >> VALUE LABEL LINE" << std::endl;
        return true;
    }

    // label is a code label
    if (isLocalLabel) {
        // check if local label is defined under a global label scope
        if (previousGlobalLabel.empty()) {
            throw std::runtime_error("Invalid Local Code Label: " + label + 
                    " cannot be defined before a global label is defined");
        }

        // check if local label name is valid at the current global label scope
        bool isLocalLabelNameUnique = localCodeLabels.find(label) != localCodeLabels.end();
        if (isLocalLabelNameUnique && 
                localCodeLabels.at(label).find(previousGlobalLabel) != localCodeLabels.at(label).end()) {
            throw std::runtime_error("Duplicate Local Label In Same Scope: " + label + " (" + previousGlobalLabel + ")");
        }

        // create local label scope mapping if not already present
        if (isLocalLabelNameUnique) {
            localCodeLabels[label] = std::map<std::string, Word>();
        }

        // map local label to correct global label scope
        localCodeLabels[label][previousGlobalLabel] = locationPointer;
    } else {
        // check if global label has already been defined
        if (globalCodeLabels.find(label) != globalCodeLabels.end()) {
            throw std::runtime_error("Duplicate Global Label: " + label);
        }

        // add global label to map
        globalCodeLabels[label] = locationPointer;
        previousGlobalLabel = label;
    }

    std::cout << "   >> CODE LABEL LINE" << std::endl;
    return false;
}


/**
 * Assembles the given instruction.
 * 
 * @param instruction The instruction to assemble.
 */
void AlienCPUAssembler::parseInstruction(std::string& instruction) {

}


/**
 * Converts the given operand to an addressing mode.
 * 
 * @param operand The operand to convert.
 * @return The addressing mode of the given operand.
 */
AddressingMode AlienCPUAssembler::convertOperandToAddressingMode(std::string& operand) {
    return IMMEDIATE; // TODO:
}


/**
 * Assembles the given line into machine code.
 * 
 * @param line The line to assemble.
 */
void AlienCPUAssembler::assembleLine(std::string& line) {

}


/**
 * Assembles the given instruction into machine code.
 * 
 * @param instruction The instruction to assemble.
 */
void AlienCPUAssembler::assembleInstruction(std::string& instruction) {

}



void AlienCPUAssembler::evaluateExpression(LabelExpressionPair& labelExpressionPair) {
    std::cout << "   >> ASSEMBLING EXPRESSION: " << labelExpressionPair.expression << " (" << labelExpressionPair.label << ")" << std::endl;

    Word value;


    std::cout << "   >> ASSEMBLED EXPRESSION: " << labelExpressionPair.expression << " (" << labelExpressionPair.label << ")" << " = " << value << std::endl;
}


/**
 * Converts a string value into a number
 * 
 * To signify a base value, the number must be preceded by a special character representing the base
 * Binary: 'B' or '%'
 * Octal: '@' or 'Q' or 'C' or 'O
 * Decimal: 'D' (default, no need to preceed with a special character)
 * Hexadecimal: 'H' or '$'
 * 
 * @param value string to convert to number
 * @return numeric representation of the string
 */
u64 AlienCPUAssembler::parseValue(const std::string& value) {
    if (value.empty()) {
        throw std::runtime_error("Invalid value to parse: " + value);
    }

    std::string numericValue = value.substr(1);

    // hexadecimal
    if (value[0] == 'H' || value[0] == '$') {
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
        case 'B':
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
        case '@':
        case 'Q':
        case 'C':
        case 'O':
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
        case 'D':
        default:
            number = std::stoi(value);
            break;
    }

    return number;
}
