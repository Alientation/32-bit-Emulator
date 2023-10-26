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
    u64 parsedValue = parseValue(tokens[currentTokenI]);
    if (parsedValue < min || parsedValue > max) {
        error(INVALID_TOKEN_ERROR, tokens[currentTokenI], std::stringstream() << "Invalid Value: " << parsedValue << 
        " must be between " << min << " and " << max);
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
        error(MISSING_TOKEN_ERROR, tokens[currentTokenI], std::stringstream() << "Too Many Operands");
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
        error(INVALID_TOKEN_ERROR, tokens[currentTokenI], std::stringstream() << "Invalid filename for .outfile directive: " << outputFile);
    }
    parsedTokens.push_back(ParsedToken(tokens[currentTokenI], TOKEN_DIRECTIVE_OPERAND));
}


void AlienCPUAssembler::DIR_ORG() {
    EXPECT_OPERAND();
    currentTokenI++;

    // parse org value, must be a value capable of being evaluated in the parse phase
    // ie, any labels referenced must be already defined and any expressions must be evaluated
    currentProgramCounter = EXPECT_PARSEDVALUE(0, 0xFFFFFFFF);
    parsedTokens.push_back(ParsedToken(tokens[currentTokenI], TOKEN_DIRECTIVE_OPERAND));
}


void AlienCPUAssembler::DIR_DB_LO() {
    EXPECT_OPERAND();
    currentTokenI++;

    // split operand by commas
    std::vector<std::string> splitByComma = split(tokens[currentTokenI].string, ',');

    // parse each value, must be a value capable of being evaluated in the parse phase
    // ie, any labels referenced must be already defined and any expressions must be evaluated
    for (std::string& value : splitByComma) {
        u64 parsedValue = parseValue(tokens[currentTokenI]);
        if (parsedValue > 0xFF) {
            error(INVALID_TOKEN_ERROR, tokens[currentTokenI], std::stringstream() << "Invalid value for .db directive: " << value);
        }

        currentProgramCounter++;
    }
}

void AlienCPUAssembler::DIR_D2B_LO() {

}

void AlienCPUAssembler::DIR_DW_LO() {

}

void AlienCPUAssembler::DIR_D2W_LO() {

}

void AlienCPUAssembler::DIR_DB_HI() {

}

void AlienCPUAssembler::DIR_D2B_HI() {

}

void AlienCPUAssembler::DIR_DW_HI() {

}

void AlienCPUAssembler::DIR_D2W_HI() {

}


void AlienCPUAssembler::DIR_ADVANCE() {

}

void AlienCPUAssembler::DIR_FILL() {

}

void AlienCPUAssembler::DIR_SPACE() {

}


void AlienCPUAssembler::DIR_DEFINE() {

}


void AlienCPUAssembler::DIR_CHECKPC() {

}

void AlienCPUAssembler::DIR_ALIGN() {

}


void AlienCPUAssembler::DIR_INCBIN() {

}

void AlienCPUAssembler::DIR_INCLUDE() {

}

void AlienCPUAssembler::DIR_REQUIRE() {

}


void AlienCPUAssembler::DIR_SCOPE() {

}

void AlienCPUAssembler::DIR_SCEND() {

}

void AlienCPUAssembler::DIR_MACRO() {

}

void AlienCPUAssembler::DIR_MACEND() {

}

void AlienCPUAssembler::DIR_INVOKE() {

}


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