#include "AlienCPUAssembler.h"

/**
 * Parse the current token as a value and throw an error if it is not a valid value.
 * 
 * Does not modify the internal state.
 * 
 * @param min The minimum value the parsed value can be.
 * @param max The maximum value the parsed value can be.
 * @return The parsed value.
 */
u64 AlienCPUAssembler::EXPECT_PARSEDVALUE(u64 min, u64 max) {
    u64 parsedValue = parseValue(tokens[currentTokenI].string);
    if (parsedValue < min || parsedValue > max) {
        error(INVALID_TOKEN_ERROR, tokens[currentTokenI], std::stringstream() << "Invalid Value: " << parsedValue << 
        " must be between " << min << " and " << max);
    }
    return parsedValue;
};

/**
 * Parse the current token as a value and throw an error if it is not a valid value.
 * 
 * Does not modify the internal state.
 * 
 * @param min The minimum value the parsed value can be.
 * @param max The maximum value the parsed value can be.
 * @return The parsed value.
 */
u64 AlienCPUAssembler::EXPECT_PARSEDVALUE(std::string val, u64 min, u64 max) {
    u64 parsedValue = parseValue(val);
    if (parsedValue < min || parsedValue > max) {
        error(INVALID_TOKEN_ERROR, tokens[currentTokenI], std::stringstream() << "Invalid Value: [" << val << "] " 
        << parsedValue << " must be between " << min << " and " << max);
    }
    return parsedValue;
};

/**
 * Throw an error if there is no operand available for the current token.
 * 
 * The current token is considered to be the one requesting an operand. This means
 * the token after this current token is the operand.
 * This does not modify the internal state.
 */
void AlienCPUAssembler::EXPECT_OPERAND() {
    if (currentTokenI == tokens.size() - 1 || tokens[currentTokenI + 1].lineNumber != tokens[currentTokenI].lineNumber) {
        error(MISSING_TOKEN_ERROR, tokens[currentTokenI], std::stringstream() << "Missing Operand");
    }
};

/**
 * Throw an error if there is an operand available for the current token.
 * 
 * The current token is considered to be the one which should have no operand. This means
 * the token after this current token is the operand.
 * This does not modify the internal state.
 */
void AlienCPUAssembler::EXPECT_NO_OPERAND() {
    if (currentTokenI != tokens.size() - 1 && tokens[currentTokenI + 1].lineNumber == tokens[currentTokenI].lineNumber) {
        error(MISSING_TOKEN_ERROR, tokens[currentTokenI], std::stringstream() << "Unrecognized Operand");
    }
};

/**
 * Check if there is an operand available for the current token.
 * 
 * The current token is considered to be the one which potentially has an operand. This means
 * the token after this current token is the operand if it exists.
 * This does not modify the internal state.
 * 
 * @param requireSameLine If true, the operand must be on the same line as the current token.
 * @return True if there is an operand available, false otherwise.
 */
bool AlienCPUAssembler::HAS_OPERAND(bool requireSameLine) {
    return currentTokenI != tokens.size() - 1 && (!requireSameLine || tokens[currentTokenI + 1].lineNumber == tokens[currentTokenI].lineNumber);
};



/**
 * Starts assembling on the data segment.
 * 
 * USAGE: .data [name]
 * 
 * If a name operand is supplied, the assembler will start assembling on the that specific segment.
 */
void AlienCPUAssembler::DIR_DATA() {
    segments[segmentType][segmentName] = currentProgramCounter;

    segmentType = DATA_SEGMENT;
    segmentName = ""; // initially set to default segment

    // check if supplied a name operand
    if (HAS_OPERAND()) {
        currentTokenI++;
        segmentName = tokens[currentTokenI].string;
        parsedTokens.push_back(ParsedToken(tokens[currentTokenI], TOKEN_DIRECTIVE_OPERAND));
    }

    // load the saved program counter, default to 0 if not set. 
    // std::map will automatically create the key-value pair if not present.
    currentProgramCounter = segments[segmentType][segmentName];
}

/**
 * Starts assembling on the text segment.
 * 
 * USAGE: .text [name]
 * 
 * If a name operand is supplied, the assembler will start assembling on the that specific segment.
 */
void AlienCPUAssembler::DIR_TEXT() {
    segments[segmentType][segmentName] = currentProgramCounter;

    segmentType = TEXT_SEGMENT;
    segmentName = ""; // initially set to default segment

    // check if supplied a name operand
    if (HAS_OPERAND()) {
        currentTokenI++;
        segmentName = tokens[currentTokenI].string;
        parsedTokens.push_back(ParsedToken(tokens[currentTokenI], TOKEN_DIRECTIVE_OPERAND));
    }

    // load the saved program counter, default to 0 if not set. 
    // std::map will automatically create the key-value pair if not present.
    currentProgramCounter = segments[segmentType][segmentName];
}


/**
 * Sets the output file for the assembler.
 * 
 * USAGE: .outfile "filename"
 * 
 * The filename must be a string literal.
 */
void AlienCPUAssembler::DIR_OUTFILE() {
    EXPECT_OPERAND();
    currentTokenI++;

    // check if outfile has already been defined TODO: determine if we should allow multiple outfiles
    if (!outputFile.empty()) {
        error(MULTIPLE_DEFINITION_ERROR, tokens[currentTokenI], std::stringstream() 
                << ".outfile directive cannot be defined multiple times in the same file");
    }

    // set the output file, must be a string argument
    outputFile = getStringToken(tokens[currentTokenI].string);
    if (!isValidFilename(outputFile)) {
        error(INVALID_TOKEN_ERROR, tokens[currentTokenI], std::stringstream() 
                << "Invalid filename for .outfile directive: " << outputFile);
    }
    parsedTokens.push_back(ParsedToken(tokens[currentTokenI], TOKEN_DIRECTIVE_OPERAND));
}


/**
 * Sets the current program counter to the specified value.
 * 
 * USAGE: .org value
 * 
 * The value must be a valid value capable of being evaluated in the parse phase.
 */
void AlienCPUAssembler::DIR_ORG() {
    EXPECT_OPERAND();
    currentTokenI++;

    // parse org value, must be a value capable of being evaluated in the parse phase
    // ie, any labels referenced must be already defined and any expressions must be evaluated
    currentProgramCounter = EXPECT_PARSEDVALUE(0, 0xFFFFFFFF);
    parsedTokens.push_back(ParsedToken(tokens[currentTokenI], TOKEN_DIRECTIVE_OPERAND));
}



/**
 * Helper for .db and related directives.
 * Defines a series of bytes at the current program counter.
 * 
 * @param token The token containing the operand.
 * @param bytes The number of bytes to define.
 * @param lowEndian If true, the bytes will be written in little endian order.
 */
void AlienCPUAssembler::defineBytes(std::string token, Byte bytes, bool lowEndian) {
    // split operand by commas
    std::vector<std::string> splitByComma = split(token, ',');

    // check if there is enough memory space to write to
    if (currentProgramCounter + splitByComma.size() * bytes < currentProgramCounter) {
        error(INTERNAL_ERROR, tokens[currentTokenI], std::stringstream() 
                << "Failed DEFINEBYTES: Current address " << stringifyHex(currentProgramCounter) 
                << " plus splitByComma.size() " << stringifyHex(splitByComma.size()) << " times bytes " 
                << stringifyHex(bytes) << " overflows");
    }

    // parse each value, must be a value capable of being evaluated in the parse phase
    // ie, any labels referenced must be already defined and any expressions must be evaluated
    for (std::string& value : splitByComma) {
        u64 parsedValue = EXPECT_PARSEDVALUE(trim(value), 0, (1 << (bytes * 8)) - 1);
        writeBytes(parsedValue, bytes, lowEndian);
    }
}


/**
 * Defines a series of 1 byte values at the current program counter, stored in low endian format.
 * 
 * USAGE: .db value[, value...]
 * 
 * Each value must be a valid value capable of being evaluated in the parse phase.
 */
void AlienCPUAssembler::DIR_DB_LO() {
    EXPECT_OPERAND();
    currentTokenI++;
    defineBytes(tokens[currentTokenI].string, 1, true);
    parsedTokens.push_back(ParsedToken(tokens[currentTokenI], TOKEN_DIRECTIVE_OPERAND));
}

/**
 * Defines a series of 2 byte values at the current program counter, stored in low endian format.
 * 
 * USAGE: .d2b value[, value...]
 * 
 * Each value must be a valid value capable of being evaluated in the parse phase.
 */
void AlienCPUAssembler::DIR_D2B_LO() {
    EXPECT_OPERAND();
    currentTokenI++;
    defineBytes(tokens[currentTokenI].string, 2, true);
    parsedTokens.push_back(ParsedToken(tokens[currentTokenI], TOKEN_DIRECTIVE_OPERAND));
}

/**
 * Defines a series of 4 byte values at the current program counter, stored in low endian format.
 * 
 * USAGE: .dw value[, value...]
 * 
 * Each value must be a valid value capable of being evaluated in the parse phase.
 */
void AlienCPUAssembler::DIR_DW_LO() {
    EXPECT_OPERAND();
    currentTokenI++;
    defineBytes(tokens[currentTokenI].string, 4, true);
    parsedTokens.push_back(ParsedToken(tokens[currentTokenI], TOKEN_DIRECTIVE_OPERAND));
}

/**
 * Defines a series of 8 byte values at the current program counter, stored in low endian format.
 * 
 * USAGE: .d2w value[, value...]
 * 
 * Each value must be a valid value capable of being evaluated in the parse phase.
 */
void AlienCPUAssembler::DIR_D2W_LO() {
    EXPECT_OPERAND();
    currentTokenI++;
    defineBytes(tokens[currentTokenI].string, 8, true);
    parsedTokens.push_back(ParsedToken(tokens[currentTokenI], TOKEN_DIRECTIVE_OPERAND));
}

/**
 * Defines a series of 1 byte values at the current program counter, stored in high endian format.
 * 
 * USAGE: .dbhi value[, value...]
 * 
 * Each value must be a valid value capable of being evaluated in the parse phase.
 */
void AlienCPUAssembler::DIR_DB_HI() {
    EXPECT_OPERAND();
    currentTokenI++;
    defineBytes(tokens[currentTokenI].string, 1, false);
    parsedTokens.push_back(ParsedToken(tokens[currentTokenI], TOKEN_DIRECTIVE_OPERAND));
}

/**
 * Defines a series of 2 byte values at the current program counter, stored in high endian format.
 * 
 * USAGE: .d2bhi value[, value...]
 * 
 * Each value must be a valid value capable of being evaluated in the parse phase.
 */
void AlienCPUAssembler::DIR_D2B_HI() {
    EXPECT_OPERAND();
    currentTokenI++;
    defineBytes(tokens[currentTokenI].string, 2, false);
    parsedTokens.push_back(ParsedToken(tokens[currentTokenI], TOKEN_DIRECTIVE_OPERAND));
}

/**
 * Defines a series of 4 byte values at the current program counter, stored in high endian format.
 * 
 * USAGE: .dwhi value[, value...]
 * 
 * Each value must be a valid value capable of being evaluated in the parse phase.
 */
void AlienCPUAssembler::DIR_DW_HI() {
    EXPECT_OPERAND();
    currentTokenI++;
    defineBytes(tokens[currentTokenI].string, 4, false);
    parsedTokens.push_back(ParsedToken(tokens[currentTokenI], TOKEN_DIRECTIVE_OPERAND));
}

/**
 * Defines a series of 8 byte values at the current program counter, stored in high endian format.
 * 
 * USAGE: .d2whi value[, value...]
 * 
 * Each value must be a valid value capable of being evaluated in the parse phase.
 */
void AlienCPUAssembler::DIR_D2W_HI() {
    EXPECT_OPERAND();
    currentTokenI++;
    defineBytes(tokens[currentTokenI].string, 8, false);
    parsedTokens.push_back(ParsedToken(tokens[currentTokenI], TOKEN_DIRECTIVE_OPERAND));
}


/**
 * Defines a series of strings at the current program counter.
 * 
 * USAGE: .ascii "string"[, "string"...]
 * 
 * Each string must be a valid string literal.
 */
void AlienCPUAssembler::DIR_ASCII() {
    EXPECT_OPERAND();
    currentTokenI++;

    // split operand by commas
    std::vector<std::string> splitByComma = split(tokens[currentTokenI].string, ',');
    std::vector<std::string> strings;
    Word bytesNeeded = 0;
    for (std::string str : splitByComma) {
        strings.push_back(getStringToken(trim(str)));
        bytesNeeded += strings.back().size();
    }

    // check if there is enough memory space to write to
    if (currentProgramCounter + bytesNeeded < currentProgramCounter) {
        error(INTERNAL_ERROR, tokens[currentTokenI], std::stringstream() 
                << "Failed DEFINESTRINGS: Current address " << stringifyHex(currentProgramCounter) 
                << " plus bytesNeeded " << stringifyHex(bytesNeeded) << " overflows");
    }

    // write each string
    for (std::string str : strings) {
        for (char c : str) {
            writeBytes(c, 1, true);
        }
    }

    parsedTokens.push_back(ParsedToken(tokens[currentTokenI], TOKEN_DIRECTIVE_OPERAND));
}

/**
 * Defines a series of strings at the current program counter with each string followed by a zero byte.
 * 
 * USAGE: .asciiz "string"[, "string"...]
 * 
 * Each string must be a valid string literal.
 */
void AlienCPUAssembler::DIR_ASCIIZ() {
    EXPECT_OPERAND();
    currentTokenI++;

    // split operand by commas
    std::vector<std::string> splitByComma = split(tokens[currentTokenI].string, ',');
    std::vector<std::string> strings;
    Word bytesNeeded = 0;
    for (std::string str : splitByComma) {
        strings.push_back(getStringToken(trim(str)));
        bytesNeeded += strings.back().size() + 1;
    }

    // check if there is enough memory space to write to
    if (currentProgramCounter + bytesNeeded < currentProgramCounter) {
        error(INTERNAL_ERROR, tokens[currentTokenI], std::stringstream() 
                << "Failed DEFINESTRINGS: Current address " << stringifyHex(currentProgramCounter) 
                << " plus bytesNeeded " << stringifyHex(bytesNeeded) << " overflows");
    }

    // write each string
    for (std::string str : strings) {
        for (char c : str) {
            writeBytes(c, 1, true);
        }
        writeBytes(0, 1, true);
    }

    parsedTokens.push_back(ParsedToken(tokens[currentTokenI], TOKEN_DIRECTIVE_OPERAND));
}


/**
 * Advances the current program counter to the specified address.
 * 
 * USAGE: .advance address,[filler]
 * 
 * The address must be a valid 4 byte value capable of being evaluated in the parse phase.
 * The address must be greater than the current program counter.
 * The filler must be a valid 1 byte value capable of being evaluated in the parse phase.
 */
void AlienCPUAssembler::DIR_ADVANCE() {
    EXPECT_OPERAND();
    currentTokenI++;

    // split operand by commas
    std::vector<std::string> splitByComma = split(tokens[currentTokenI].string, ',');
    
    // no valid address operand
    if (splitByComma.size() == 0) {
        error(INVALID_TOKEN_ERROR, tokens[currentTokenI], std::stringstream()
                << "Missing operand for .advance directive: " << tokens[currentTokenI].string);
    }

    Word targetAddress = (Word) EXPECT_PARSEDVALUE(trim(splitByComma[0]), 0, 0xFFFFFFFF);

    // cannot advance address backwards
    if (targetAddress < currentProgramCounter) {
        error(INVALID_TOKEN_ERROR, tokens[currentTokenI], std::stringstream() 
                << "Invalid address for .advance directive: " << stringifyHex(targetAddress) 
                << " must be greater than " << stringifyHex(currentProgramCounter));
    }

    Byte filler = 0;

    // check if there is filler argument
    if (splitByComma.size() > 1) {
        filler = (Byte) EXPECT_PARSEDVALUE(trim(splitByComma[1]), 0, 0xFF);

        // too many operands for .advance directive
        if (splitByComma.size() > 2) {
            error(UNRECOGNIZED_TOKEN_ERROR, tokens[currentTokenI], std::stringstream() 
                    << "Unrecognized operand for .advance directive: " << tokens[currentTokenI].string);
        }
    }

    // fill the space with the filler value
    for (Word i = currentProgramCounter; i < targetAddress; i++) {
        writeBytes(filler, 1, true);
    }

    parsedTokens.push_back(ParsedToken(tokens[currentTokenI], TOKEN_DIRECTIVE_OPERAND));
}


/**
 * Fills a space with bytes. By default, the space is filled with a series of 1 bytes with a value of 0.
 * 
 * USAGE: .fill count[,size[,value]]
 * 
 * The count must be a valid 4 byte value capable of being evaluated in the parse phase.
 * The size must be a valid 1 byte value capable of being evaluated in the parse phase.
 * The value must be a valid value capable of being evaluated in the parse phase.
 */
void AlienCPUAssembler::DIR_FILL() {
    EXPECT_OPERAND();
    currentTokenI++;

    // split operand by commas
    std::vector<std::string> splitByComma = split(tokens[currentTokenI].string, ',');

    // no valid fillcount operand
    if (splitByComma.size() == 0) {
        error(MISSING_TOKEN_ERROR, tokens[currentTokenI], std::stringstream()
                << "Missing operand for .fill directive: " << tokens[currentTokenI].string);
    }

    Word fillcount = (Word) EXPECT_PARSEDVALUE(trim(splitByComma[0]), 0, 0xFFFFFFFF);
    Byte size = 1;
    u64 value = 0;

    // check for size argument
    if (splitByComma.size() > 1) {
        size = (Byte) EXPECT_PARSEDVALUE(trim(splitByComma[1]), 1, 0xFF);

        // check for value argument
        if (splitByComma.size() > 2) {
            value = EXPECT_PARSEDVALUE(trim(splitByComma[2]), 0, (1 << (size * 8)) - 1);
        }

        // too many operands for .fill directive
        if (splitByComma.size() > 3) {
            error(UNRECOGNIZED_TOKEN_ERROR, tokens[currentTokenI], std::stringstream() 
                    << "Unrecognized operand for .fill directive: " << tokens[currentTokenI].string);
        }
    }

    // check to make sure that there is enough memory to write to
    if (currentProgramCounter + fillcount * size < currentProgramCounter) {
        error(INTERNAL_ERROR, tokens[currentTokenI], std::stringstream() 
                << "Failed FILL: Current address " << stringifyHex(currentProgramCounter) 
                << " plus fillcount " << stringifyHex(fillcount) << " times size " << stringifyHex(size) << " overflows");
    }

    // fill the space with the filler value
    for (Word i = 0; i < fillcount; i++) {
        writeBytes(value, size, true);
    }

    parsedTokens.push_back(ParsedToken(tokens[currentTokenI], TOKEN_DIRECTIVE_OPERAND));
}

/**
 * Fills a space with bytes. By default, the space is filled with a series of bytes with a value of 0.
 * 
 * USAGE: .space count[,value]
 * 
 * The count must be a valid 4 byte value capable of being evaluated in the parse phase.
 */
void AlienCPUAssembler::DIR_SPACE() {
    EXPECT_OPERAND();
    currentTokenI++;

    // split operand by commas
    std::vector<std::string> splitByComma = split(tokens[currentTokenI].string, ',');

    // no valid count operand
    if (splitByComma.size() == 0) {
        error(MISSING_TOKEN_ERROR, tokens[currentTokenI], std::stringstream()
                << "Missing operand for .space directive: " << tokens[currentTokenI].string);
    }

    Word count = (Word) EXPECT_PARSEDVALUE(trim(splitByComma[0]), 0, 0xFFFFFFFF);
    Byte value = 0;

    // check for value argument
    if (splitByComma.size() > 1) {
        value = (Byte) EXPECT_PARSEDVALUE(trim(splitByComma[1]), 0, 0xFF);

        // too many operands for .space directive
        if (splitByComma.size() > 2) {
            error(UNRECOGNIZED_TOKEN_ERROR, tokens[currentTokenI], std::stringstream() 
                    << "Unrecognized operand for .space directive: " << tokens[currentTokenI].string);
        }
    }

    // check to make sure that there is enough memory to write to
    if (currentProgramCounter + count < currentProgramCounter) {
        error(INTERNAL_ERROR, tokens[currentTokenI], std::stringstream() 
                << "Failed SPACE: Current address " << stringifyHex(currentProgramCounter) 
                << " plus count " << stringifyHex(count) << " overflows");
    }

    // fill the space with the filler value
    for (Word i = 0; i < count; i++) {
        writeBytes(value, 1, true);
    }

    parsedTokens.push_back(ParsedToken(tokens[currentTokenI], TOKEN_DIRECTIVE_OPERAND));
}


/**
 * 
 */
void AlienCPUAssembler::DIR_GLOBAL() {

}

/**
 * 
 */
void AlienCPUAssembler::DIR_EXTERN() {

}

/**
 * Defines a label with a specific value.
 * 
 * USAGE: .define name,value
 * 
 * The name must be a valid label name.
 * The value must be a valid value capable of being evaluated in the parse phase.
 */
void AlienCPUAssembler::DIR_DEFINE() {
    EXPECT_OPERAND();
    currentTokenI++;

    // split operand by commas
    std::vector<std::string> splitByComma = split(tokens[currentTokenI].string, ',');

    // no valid name operand
    if (splitByComma.size() == 0) {
        error(MISSING_TOKEN_ERROR, tokens[currentTokenI], std::stringstream()
                << "Missing label name operand for .define directive: " << tokens[currentTokenI].string);
    }

    std::string name = trim(splitByComma[0]);

    if (name.size() == 0) {
        error(INVALID_TOKEN_ERROR, tokens[currentTokenI], std::stringstream() 
                << "Invalid label name for .define directive: " << name);
    }

    if (splitByComma.size() < 2) {
        error(MISSING_TOKEN_ERROR, tokens[currentTokenI], std::stringstream()
                << "Missing value operand for .define directive: " << tokens[currentTokenI].string);
    }

    // define label with value
    Word value = EXPECT_PARSEDVALUE(trim(splitByComma[1]), 0, 0xFFFFFFFF);
    defineLabel(name, value);

    // too many operands for .define directive
    if (splitByComma.size() > 2) {
        error(UNRECOGNIZED_TOKEN_ERROR, tokens[currentTokenI], std::stringstream() 
                << "Unrecognized operand for .define directive: " << tokens[currentTokenI].string);
    }

    parsedTokens.push_back(ParsedToken(tokens[currentTokenI], TOKEN_DIRECTIVE_OPERAND));
}

/**
 * Defines a label with a specific value. Will override previously defined label values.
 * 
 * USAGE: .set name,value
 * 
 * The value must be a valid value capable of being evaluated in the parse phase.
 */
void AlienCPUAssembler::DIR_SET() {
    EXPECT_OPERAND();
    currentTokenI++;

    // split operand by commas
    std::vector<std::string> splitByComma = split(tokens[currentTokenI].string, ',');

    // no valid name operand
    if (splitByComma.size() == 0) {
        error(MISSING_TOKEN_ERROR, tokens[currentTokenI], std::stringstream()
                << "Missing label name operand for .set directive: " << tokens[currentTokenI].string);
    }

    std::string name = trim(splitByComma[0]);

    if (name.size() == 0) {
        error(INVALID_TOKEN_ERROR, tokens[currentTokenI], std::stringstream() 
                << "Invalid label name for .set directive: " << name);
    }

    if (splitByComma.size() < 2) {
        error(MISSING_TOKEN_ERROR, tokens[currentTokenI], std::stringstream()
                << "Missing value operand for .set directive: " << tokens[currentTokenI].string);
    }

    // define label with value
    Word value = EXPECT_PARSEDVALUE(trim(splitByComma[1]), 0, 0xFFFFFFFF);
    defineLabel(name, value);

    // too many operands for .set directive
    if (splitByComma.size() > 2) {
        error(UNRECOGNIZED_TOKEN_ERROR, tokens[currentTokenI], std::stringstream() 
                << "Unrecognized operand for .set directive: " << tokens[currentTokenI].string);
    }

    parsedTokens.push_back(ParsedToken(tokens[currentTokenI], TOKEN_DIRECTIVE_OPERAND));
}


/**
 * Checks the current program counter to make sure it does not overstep a memory boundary.
 * 
 * USAGE: .checkpc address
 * 
 * The address must be a value capable of being evaluated in the parse phase.
 */
void AlienCPUAssembler::DIR_CHECKPC() {
    EXPECT_OPERAND();
    currentTokenI++;

    Word checkpc = (Word) EXPECT_PARSEDVALUE(0, 0xFFFFFFFF);
    
    if (currentProgramCounter > checkpc) {
        error(INTERNAL_ERROR, tokens[currentTokenI], std::stringstream() 
                << "Failed CHECKPC: Current address " << stringifyHex(currentProgramCounter) 
                << " is greater than checkpc " << stringifyHex(checkpc));
    }

    parsedTokens.push_back(ParsedToken(tokens[currentTokenI], TOKEN_DIRECTIVE_OPERAND));
}

/**
 * Aligns the current program counter to the next multiple of the align value.
 * 
 * USAGE: .align value
 * 
 * The align value must be a value that is capable of being evaluated in the parse phase.
 * If the align causes the program counter to overflow, an error will be thrown.
 */
void AlienCPUAssembler::DIR_ALIGN() {
    EXPECT_OPERAND();
    currentTokenI++;

    Word align = (Word) EXPECT_PARSEDVALUE(0, 0xFFFFFFFF);
    Word previousProgramCounter = currentProgramCounter;

    // align the current program counter to the next multiple of the align value
    currentProgramCounter = (currentProgramCounter + align - 1) & ~(align - 1);
    
    // check to make sure we did not go backwards
    if (currentProgramCounter < previousProgramCounter) {
        error(INTERNAL_ERROR, tokens[currentTokenI], std::stringstream() 
                << "Failed ALIGN: Current address " << stringifyHex(currentProgramCounter) 
                << " is less than previous address " << stringifyHex(previousProgramCounter));
    }

    parsedTokens.push_back(ParsedToken(tokens[currentTokenI], TOKEN_DIRECTIVE_OPERAND));
}


void AlienCPUAssembler::DIR_INCBIN() {

}

void AlienCPUAssembler::DIR_INCLUDE() {

}

void AlienCPUAssembler::DIR_REQUIRE() {

}



void AlienCPUAssembler::DIR_REPEAT() {

}

void AlienCPUAssembler::DIR_REND() {

}


/**
 * Starts a new scope.
 * 
 * USAGE: .scope
 * 
 * This directive is used to start a new scope. A scope has a set of local labels which are only visible
 * within that scope. This is useful for defining labels which are only used within a specific
 * section of code. TODO: for parsing values, allow parent scope labels to be referenced from the local scope using
 * special symbols like '.' or '..' or something.
 */
void AlienCPUAssembler::DIR_SCOPE() {
    EXPECT_NO_OPERAND();

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
 * Ends the current scope.
 * 
 * USAGE: .scend
 * 
 * This directive is used to end the current scope. This will return the assembler to the parent scope.
 * If there is no parent scope, this will throw an error.
 */
void AlienCPUAssembler::DIR_SCEND() {
    EXPECT_NO_OPERAND();

    // check if the current scope has no parent scope. This means we are at the global scope which has
    // no parent scope. This implies that there is more SCEND directives than SCOPE directives at this point.
    if (currentScope->parent == nullptr) {
        error(INVALID_TOKEN_ERROR, tokens[currentTokenI], std::stringstream() << "Cannot end scope, no parent scope.");
    }

    currentScope = currentScope->parent;
}

void AlienCPUAssembler::DIR_MACRO() {

}

void AlienCPUAssembler::DIR_MACEND() {

}

void AlienCPUAssembler::DIR_INVOKE() {

}



// STUFF I DON'T REALLY NEED TO FINISH RIGHT NOW BUT ARE NICE TO HAVE
void AlienCPUAssembler::DIR_ASSERT() {

}

void AlienCPUAssembler::DIR_ERROR() {

}

void AlienCPUAssembler::DIR_ERRORIF() {

}

void AlienCPUAssembler::DIR_IF() {

}

void AlienCPUAssembler::DIR_IFDEF() {

}

void AlienCPUAssembler::DIR_IFNDEF() {

}

void AlienCPUAssembler::DIR_ELSEIF() {

}

void AlienCPUAssembler::DIR_ELSEIFDEF() {

}

void AlienCPUAssembler::DIR_ELSEIFNDEF() {

}

void AlienCPUAssembler::DIR_ELSE() {

}

void AlienCPUAssembler::DIR_ENDIF() {

}

void AlienCPUAssembler::DIR_PRINT() {

}

void AlienCPUAssembler::DIR_PRINTIF() {

}

void AlienCPUAssembler::DIR_PRINTNOW() {

}