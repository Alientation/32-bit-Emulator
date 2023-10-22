#include "AlienCPUAssembler.h"


main() {
    AlienCPU cpu;
    AlienCPUAssembler assembler(cpu);
    assembler.assemble("\tLDA\t\t \t#$FFFF\n;THIS IS A COMMENT\n;SO IS THIS\n.org\t$FFFG");
}


/**
 * Constructs a new AlienCPUAssembler for the given AlienCPU.
 */
AlienCPUAssembler::AlienCPUAssembler(AlienCPU& cpu) : cpu(cpu) {
    
}


/**
 * Assembles the given assembly source code into machine code and loads it onto the cpu.
 * 
 * Documentation on the assembly language can be found SOMEWHERE
 * 
 * @param source The assembly source code to assemble into machine code.
 */
void AlienCPUAssembler::assemble(std::string source) {
    std::cout << "ASSEMBLING..." << std::endl;

    // reset assembler state to be ready for a new assembly
    // the source code to assemble
    sourceCode = source;

    // memory address of the next program instruction to be assembled
    locationPointer = 0x00000000;

    // split the source code into lines
    lines = split(source, '\n');
    currentLine = 0;
    currentLineTokens.clear();
    previousGlobalLabel = "";


    // labels that reference code locations
    globalCodeLabels.clear(); // can be referenced from anywhere in the program
    localCodeLabels.clear(); // can only be referenced locally in a subroutine (between two global labels)

    // code labels that have not been processed into memory addresses yet
    globalUnprocessedCodeLabels.clear();
    localUnprocessedCodeLabels.clear();

    // processed value labels (labels that reference values)
    globalValueLabels.clear();
    localValueLabels.clear();

    // value labels expressions that have not be processed yet
    globalUnprocessedValueLabels.clear();
    localUnprocessedValueLabels.clear();

    // mapping of global labels to their local labels
    globalLabelToLocalChildrenLabelsMapping.clear();


    // iterate over each line
    for (; currentLine < lines.size(); currentLine++) {
        std::cout << std::endl << " PARSING LINE " << currentLine << ": " << lines[currentLine] << std::endl;
        assembleLineFirstPass(lines[currentLine]);
        std::cout << "   >> PARSED LINE " << currentLine << std::endl;

    }

    // if there are any remaining unprocessed code labels, throw an error
    // those labels have been defined to point to non-existant code locations
    if (!globalUnprocessedCodeLabels.empty() || !localUnprocessedCodeLabels.empty()) {
        throw std::runtime_error("Unprocessed code labels: GLOBAL=" + tostring(globalUnprocessedCodeLabels) + 
                " LOCAL=" + tostring(localUnprocessedCodeLabels));
    }

    // process unprocessed value labels sequentially from global to local in order of appearance
    for (LabelExpressionPair labelExpressionPair : globalUnprocessedValueLabels) {
        assembleExpression(labelExpressionPair);
    }

    for (LabelExpressionPair labelExpressionPair : localUnprocessedValueLabels) {
        assembleExpression(labelExpressionPair);
    }


    // iterate over each line for the second pass and fill in values for labels
    locationPointer = 0x00000000;
    for (currentLine = 0; currentLine < lines.size(); currentLine++) {
        assembleLineSecondPass(lines[currentLine]);
    }

    std::cout << std::endl << "SUCCESSFULLY ASSEMBLED! " << std::endl;
}


/**
 * Assembles the given line.
 * 
 * @param line The line to assemble.
 */
void AlienCPUAssembler::assembleLineFirstPass(std::string& line) {
    // check if line is empty or a comment
    if (line.empty() || line[0] == ';') {
        std::cout << "   >> NO CODE LINE" << std::endl;
        return;
    }

    currentLineTokens.clear();

    // split the line into tokens delimited by at minimum a tab
    for (std::string token : split(line, '\t')) {
        // remove whitespace from token
        token.erase(std::remove_if(token.begin(), token.end(), isspace), token.end());

        // only add token if it is not empty and not a comment or it is the first token
        // this will help to allign tokens so that the first token is always the label
        if (!token.empty() || currentLineTokens.empty() && token[0] != ';') {
            currentLineTokens.push_back(token);
        }
    }
    
    std::cout << "   >> TOKENS: " << std::endl;
    for (int tokenI = 0; tokenI < currentLineTokens.size(); tokenI++) {
        std::cout << "\t" <<tokenI << ": " << currentLineTokens[tokenI] << " " << std::endl;
    }

    // there should be code tokens when the line is not empty
    if (currentLineTokens.empty()) {
        throw std::runtime_error("Invalid empty tokens: " + tostring(currentLineTokens) + " -> " + line);
    }


    // process assembler directives, could potentially be local labels, but we don't know for sure
    if (currentLineTokens[0][0] == '.' && parseAssemblerDirective()) {
        std::cout << "   >> PARSED ASSEMBLER DIRECTIVE: " << currentLineTokens[0] << std::endl;
        return;
    }
    
    // tokens should now be alligned so that the first token is the label if it exists
    assembleLabelFirstPass(currentLineTokens[0]);

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
    // ORG
    if (currentLineTokens[0] == ".org") {
        std::cout << "   >> ORG Statement" << std::endl;
        
        if (currentLineTokens.size() == 1) {
            throw std::runtime_error("ORG Directive must be supplied a value: " + lines[currentLine]);
        }

        u64 parsedValue = parseValue(currentLineTokens[1]);
        if (parsedValue > 0xFFFFFFFF) {
            throw std::runtime_error("Invalid ORG value: " + currentLineTokens[1] + 
                    "\nProgram address origin must be within 0xFFFFFFFF");
        }

        std::cout << "   >> MOVING LOCATION POINTER TO " << stringifyHex((Word) parsedValue) << std::endl;
        locationPointer = (Word) parsedValue;

        if (currentLineTokens.size() > 2) {
            throw std::runtime_error("Unrecognized token: " + currentLineTokens[2] +
                    "\nExpected new line. ");
        }
        return true;
    }

    // DEFINING DATA 








    return false;
}



/**
 * Assembles the given label.
 * 
 * @param label The label to assemble.
 */
void AlienCPUAssembler::assembleLabelFirstPass(std::string& label) {
    if (currentLineTokens.size() != 1 && label.empty()) {
        std::cout << "   >> NO LABEL LINE" << std::endl;
        return;
    }

    if (label.empty()) {
        throw std::runtime_error("Invalid label: " + label + " -> " + lines[currentLine]);
    }

    // check if label is a local label
    bool isLocalLabel = label[0] == '.';

    // check if label is a value label
    // if so, store it for later processing
    if (currentLineTokens.size() > 1 && currentLineTokens[2] == "equ") {
        if (currentLineTokens.size() == 3) {
            if (isLocalLabel) {
                localUnprocessedValueLabels.push_back(LabelExpressionPair(label, currentLineTokens[3]));
            } else {
                globalUnprocessedValueLabels.push_back(LabelExpressionPair(label, currentLineTokens[3]));
            }
        } else {
            throw std::runtime_error("Invalid value label: " + label + " -> " + lines[currentLine]);
        }
        std::cout << "   >> VALUE LABEL LINE" << std::endl;
        return;
    }

    // label is a code label
    if (isLocalLabel) {

    } else {
        previousGlobalLabel = label;
    }

    std::cout << "   >> CODE LABEL LINE" << std::endl;
}


/**
 * Assembles the given pseduo instruction.
 * 
 * @param pseduoInstruction The pseduo instruction to assemble.
 */
void AlienCPUAssembler::assemblePseduoInstruction(std::string& pseduoInstruction) {

}


/**
 * Assembles the given instruction.
 * 
 * @param instruction The instruction to assemble.
 */
void AlienCPUAssembler::assembleInstructionFirstPass(std::string& instruction) {

}


/**
 * Converts the given operand to an addressing mode.
 * 
 * @param operand The operand to convert.
 * @return The addressing mode of the given operand.
 */
AddressingMode AlienCPUAssembler::convertOperandToAddressingMode(std::string& operand) {
    return IMMEDIATE;
}


/**
 * Assembles the given line into machine code.
 * 
 * @param line The line to assemble.
 */
void AlienCPUAssembler::assembleLineSecondPass(std::string& line) {

}


/**
 * Assembles the given instruction into machine code.
 * 
 * @param instruction The instruction to assemble.
 */
void AlienCPUAssembler::assembleInstructionSecondPass(std::string& instruction) {

}



void AlienCPUAssembler::assembleExpression(LabelExpressionPair& labelExpressionPair) {
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


bool AlienCPUAssembler::isNumber(const std::string& string) {
    std::string::const_iterator it = string.begin();
    while (it != string.end() && std::isdigit(*it)) {
        ++it;
    }
    return !string.empty() && it == string.end();
}