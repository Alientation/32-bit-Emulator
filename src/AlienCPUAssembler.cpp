#include "AlienCPUAssembler.h"


main() {
    AlienCPU cpu;
    AlienCPUAssembler assembler(cpu);
    assembler.assemble("\tLDA\t\t \t#$FFFF\n;THIS IS A COMMENT\n;SO IS THIS");
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

    // memory address of the next program instruction to be assembled
    locationPointer = 0x00000000;

    // split the source code into lines
    lines = split(source, '\n');

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


    // iterate over each line
    for (currentLine = 0; currentLine < lines.size(); currentLine++) {
        assembleLineFirstPass(lines[currentLine]);
    }

    // if there are any remaining unprocessed code labels, throw an error
    // those labels have been defined to point to non-existant code locations
    if (!globalUnprocessedCodeLabels.empty() || !localUnprocessedCodeLabels.empty()) {
        throw std::runtime_error("Unprocessed code labels: GLOBAL=" + tostring(globalUnprocessedCodeLabels) + " LOCAL=" + tostring(localUnprocessedCodeLabels));
    }

    // process unprocessed value labels sequentially from global to local in order of appearance
    for (LabelExpressionPair labelExpressionPair : globalUnprocessedValueLabels) {
        assembleExpression(labelExpressionPair);
    }

    std::cout << std::endl << "SUCCESSFULLY ASSEMBLED! " << std::endl;
}


/**
 * Assembles the given line.
 * 
 * @param line The line to assemble.
 */
void AlienCPUAssembler::assembleLineFirstPass(std::string& line) {
    std::cout << std::endl << " PARSING LINE " << currentLine << ": " << line << std::endl;

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

    // there shouldn't be no valid tokens when the line is not empty
    if (currentLineTokens.empty()) {
        throw std::runtime_error("Invalid empty tokens: " + tostring(currentLineTokens) + " -> " + line);
    }
    
    // tokens should now be alligned so that the first token is the label if it exists
    assembleLabelFirstPass(currentLineTokens[0]);

    // only label on the current line
    if (currentLineTokens.size() == 1) {
        std::cout << "   >> ONLY LABEL LINE" << std::endl;
        return;
    }
    
    std::cout << "   >> PARSED LINE " << currentLine << std::endl;
}


/**
 * Assembles the given label.
 * 
 * @param label The label to assemble.
 */
void AlienCPUAssembler::assembleLabelFirstPass(std::string& label) {
    if (currentLineTokens.size() == 1 || !label.empty()) {
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

        if (isLocalLabel) {

        } else {

        }

        std::cout << "   >> CODE LABEL LINE" << std::endl;
    } else {
        std::cout << "   >> NO LABEL LINE" << std::endl;
    }
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
 * Splits a string into a vector of strings based on the given delimiter.
 * 
 * @param source The string to split.
 * @param delimiter The delimiter to split the string on.
 * @return A vector of strings split from the given string based on the given delimiter.
 */
static std::vector<std::string> split(std::string source, char delimiter) {
    std::vector<std::string> lines;
    std::string line;
    std::stringstream ss(source);

    while (std::getline(ss, line, delimiter)) {
        lines.push_back(line);
    }

    return lines;
}


/**
 * Converts a vector of strings into a single string.
 * 
 * Formats the string in the following way:
 * [string1, string2, string3, ...]
 * 
 * @param strings The vector of strings to convert.
 * @return A single string containing all of the strings in the given vector.
 */
static std::string tostring(std::vector<std::string>& strings) {
    std::string result = "[";

    for (std::string string : strings) {
        result += string + ", ";
    }

    // remove the last comma and space from string
    result.pop_back();
    result.pop_back();

    result += "]";

    return result;
}


/**
 * Determines if the given processor instruction is valid for the given addressing mode.
 * 
 * @param instruction The processor instruction to check.
 * @param addressingMode The addressing mode to check.
 * @return True if the processor instruction is valid for the given addressing mode, false otherwise.
 */
static bool validProcessorInstructionAddressingMode(std::string& instruction, AddressingMode addressingMode) {
    if (instructionMap.count(instruction) == 0) {
        return false;
    }
    
    return instructionMap.at(instruction).count(addressingMode) > 0;
}


/**
 * Gets the opcode for the given processor instruction and addressing mode.
 * 
 * @param instruction The processor instruction to get the opcode for.
 * @param addressingMode The addressing mode to get the opcode for.
 * 
 * @return The opcode for the given processor instruction and addressing mode.
 */
static u8 getProcessorInstructionOpcode(std::string& instruction, AddressingMode addressingMode) {
    return instructionMap.at(instruction).at(addressingMode);
}