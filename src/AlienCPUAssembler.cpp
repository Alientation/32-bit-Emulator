#include "AlienCPUAssembler.h"


main() {
    AlienCPU AlienCPU;
    assemble(AlienCPU, "\tLDA\t\t \t#$FFFF\n;THIS IS A COMMENT\n;SO IS THIS");
}


/**
 * Assembles the given assembly source code into machine code.
 * 
 * Documentation on the assembly language can be found SOMEWHERE
 * 
 * @param AlienCPU The AlienCPU to assemble the source code for.
 * @param source The assembly source code to assemble into machine code.
 */
static void assemble(AlienCPU& AlienCPU, std::string source) {
    std::cout << "ASSEMBLING..." << std::endl;

    // memory address of the next program instruction to be assembled
    u64 locationPointer = 0x00000000;

    // split the source code into lines
    std::vector<std::string> lines = split(source, '\n');

    // iterate over each line
    int lineNumber = 0;
    for (std::string line : lines) {
        std::cout << " PARSING LINE " << (lineNumber++) << ": " << line << std::endl;

        // check if line is empty or a comment
        if (line.empty() || line[0] == ';') {
            continue;
        }

        // split the line into tokens delimited by at minimum a tab
        std::vector<std::string> tokens;
        for (std::string token : split(line, '\t')) {
            // remove whitespace from token
            token.erase(std::remove_if(token.begin(), token.end(), isspace), token.end());

            // only add token if it is not empty or it is the first token
            // this will help to allign tokens so that the first token is always the label
            if (!token.empty() || tokens.empty()) {
                tokens.push_back(token);
            }
        }
        
        std::cout << "   TOKENS: " << std::endl;
        for (std::string token : tokens) {
            std::cout << "\t|" << token << " " << std::endl;
        }

        // there shouldn't be no valid tokens when the line is not empty
        if (tokens.empty()) {
            throw std::runtime_error("Invalid tokens: " + tostring(tokens) + " -> " + line);
        }
        
        // tokens should now be alligned so that the first token is the label
        std::string label = tokens[0];

        if (tokens.size() == 1) {

        }
        


        std::cout << std::endl;
    }


    std::cout << "SUCCESSFULLY ASSEMBLED! " << std::endl;
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