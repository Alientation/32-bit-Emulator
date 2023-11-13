#include "Assembler.h"


/**
 * Parse the current token as a value and throw an error if it is not a valid value.
 * 
 * Does not modify the internal state.
 * 
 * @param min The minimum value the parsed value can be.
 * @param max The maximum value the parsed value can be.
 * @return The parsed value.
 * 
 * @throws INVALID_TOKEN_ERROR If the token is not a valid value.
 */
u64 Assembler::EXPECT_PARSEDVALUE(u64 min, u64 max) {
    u64 parsedValue = parseValue(currentObjectFile->tokens[currentTokenI].string);
    if (parsedValue < min || parsedValue > max) {
        error(INVALID_TOKEN_ERROR, std::stringstream() << "Invalid Value: " << parsedValue 
				<< " must be between " << min << " and " << max << " " 
				<< currentObjectFile->tokens[currentTokenI].errorstring());
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
 * 
 * @throws INVALID_TOKEN_ERROR If the token is not a valid value.
 */
u64 Assembler::EXPECT_PARSEDVALUE(std::string val, u64 min, u64 max) {
    u64 parsedValue = parseValue(val);
    if (parsedValue < min || parsedValue > max) {
        error(INVALID_TOKEN_ERROR, std::stringstream() << "Invalid Value: [" << val << "] " 
        		<< parsedValue << " must be between " << min << " and " << max << " " 
				<< currentObjectFile->tokens[currentTokenI].errorstring());
    }
    return parsedValue;
};

/**
 * Throw an error if there is no operand available for the current token.
 * 
 * The current token is considered to be the one requesting an operand. This means
 * the token after this current token is the operand.
 * This does not modify the internal state.
 * 
 * @throws MISSING_TOKEN_ERROR If there is no operand available.
 */
void Assembler::EXPECT_OPERAND() {
    if (currentTokenI == currentObjectFile->tokens.size() - 1 || 
		currentObjectFile->tokens[currentTokenI + 1].lineNumber != currentObjectFile->tokens[currentTokenI].lineNumber) {
        error(MISSING_TOKEN_ERROR, std::stringstream() << "Missing Operand " 
				<< currentObjectFile->tokens[currentTokenI].errorstring());
    }
};

/**
 * Throw an error if there is an operand available for the current token.
 * 
 * The current token is considered to be the one which should have no operand. This means
 * the token after this current token is the operand.
 * This does not modify the internal state.
 * 
 * @throws UNRECOGNIZED_TOKEN_ERROR If there is an operand available.
 */
void Assembler::EXPECT_NO_OPERAND() {
    if (currentTokenI != currentObjectFile->tokens.size() - 1 && 
		currentObjectFile->tokens[currentTokenI + 1].lineNumber == currentObjectFile->tokens[currentTokenI].lineNumber) {
        error(UNRECOGNIZED_TOKEN_ERROR, std::stringstream() << "Unrecognized Operand "
				<< currentObjectFile->tokens[currentTokenI].errorstring());
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
bool Assembler::HAS_OPERAND(bool requireSameLine) {
    return currentTokenI != currentObjectFile->tokens.size() - 1 && (!requireSameLine || 
		currentObjectFile->tokens[currentTokenI + 1].lineNumber == currentObjectFile->tokens[currentTokenI].lineNumber);
};



/**
 * Switches to the DATA section of the program memory.
 * 
 * USAGE: .data [name]
 * 
 * If a name operand is not supplied, the program will switch to the default DATA segment.
 */
void Assembler::DIR_DATA() {
	// check if we have an operand
	std::string name = "";
	if (HAS_OPERAND()) {
		currentTokenI++;
		name = currentObjectFile->tokens[currentTokenI].string;
	}

	// create the new segment if it does not already exist
	if (currentObjectFile->segmentMap[SEGMENT_DATA].find(name) == currentObjectFile->segmentMap[SEGMENT_DATA].end()) {
		currentObjectFile->segmentMap[SEGMENT_DATA][name] = new Segment(SEGMENT_DATA, name);
	}
	
	// set the current segment to the requested segment
	currentSegment = currentObjectFile->segmentMap[SEGMENT_DATA][name];
}

/**
 * Switches to the TEXT section of the program memory.
 * 
 * USAGE: .text [name]
 * 
 * If a name operand is not supplied, the program will switch to the default TEXT segment.
 */
void Assembler::DIR_TEXT() {
	// check if we have an operand
	std::string name = "";
	if (HAS_OPERAND()) {
		currentTokenI++;
		name = currentObjectFile->tokens[currentTokenI].string;
	}

	// create the new segment if it does not already exist
	if (currentObjectFile->segmentMap[SEGMENT_TEXT].find(name) == currentObjectFile->segmentMap[SEGMENT_TEXT].end()) {
		currentObjectFile->segmentMap[SEGMENT_TEXT][name] = new Segment(SEGMENT_TEXT, name);
	}
	
	// set the current segment to the requested segment
	currentSegment = currentObjectFile->segmentMap[SEGMENT_TEXT][name];
}

/**
 * Immediately stops the assembler from parsing any more tokens.
 * 
 * USAGE: .end
 */
void Assembler::DIR_END() {
	currentTokenI = currentObjectFile->tokens.size() - 1;
}

/**
 * Sets the current relative program counter to the specified value (relative to the start of the current section).
 * This means that this whole section of program memory could be moved around by the linker or loader.
 * It would simply have a gap in the section caused by the .org directive.
 * 
 * USAGE: .org value
 * 
 * The value must be either a numeric constant defined in the current file or a label
 * defined in the current file. The value must be a 32-bit value.
 */
void Assembler::DIR_ORG_RELATIVE() {
	EXPECT_OPERAND();
	currentTokenI++;

	Word value = EXPECT_PARSEDVALUE(0, 0xFFFFFFFF);
	currentSegment->programCounter = value;

	isRelativeMemory = true;
}

/**
 * Sets the current absolute program counter to the specified value.
 * This means that this section of program memory cannot be moved around.
 * 
 * USAGE: .org value
 * 
 * The value operand must be a 32-bit number. It cannot be a referenced label.
 */
void Assembler::DIR_ORG_ABSOLUTE() {
	EXPECT_OPERAND();
	currentTokenI++;

	Word value = parseValue(currentObjectFile->tokens[currentTokenI].string, false);
	currentSegment->programCounter = value;

	isRelativeMemory = false;
}


/**
 * Helper for .db and related directives.
 * Defines a series of bytes at the current program counter.
 * 
 * @param token The token containing the operand.
 * @param bytes The number of bytes to define.
 * @param lowEndian If true, the bytes will be written in little endian order.
 * 
 * @throws INTERNAL_ERROR If there is not enough memory space to write to.
 */
void Assembler::defineBytes(std::string token, Byte bytes, bool lowEndian) {
	// split operand by commas
    std::vector<std::string> splitByComma = split(token, ',');
	Word* currentProgramCounter = &(currentSegment->programCounter);

    // check if there is enough memory space to write to
    if (currentProgramCounter + splitByComma.size() * bytes < currentProgramCounter) {
        error(INTERNAL_ERROR, std::stringstream() 
                << "Failed DEFINEBYTES: Current address " << stringifyHex(*currentProgramCounter) 
                << " plus splitByComma.size() " << stringifyHex(splitByComma.size()) << " times bytes " 
                << stringifyHex(bytes) << " overflows");
    }

	u64 maxValue = (u64)1 << (bytes * 8);
	if (maxValue-1 == 0) { // fix for overflow
		maxValue = 0xFFFFFFFFFFFFFFFFULL;
	} else {
		maxValue--;
	}

    for (std::string& value : splitByComma) {
        u64 parsedValue = EXPECT_PARSEDVALUE(trim(value), 0, maxValue);
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
void Assembler::DIR_DB_LO() {
    EXPECT_OPERAND();
    currentTokenI++;
    defineBytes(currentObjectFile->tokens[currentTokenI].string, 1, true);
}

/**
 * Defines a series of 2 byte values at the current program counter, stored in low endian format.
 * 
 * USAGE: .d2b value[, value...]
 * 
 * Each value must be a valid value capable of being evaluated in the parse phase.
 */
void Assembler::DIR_D2B_LO() {
    EXPECT_OPERAND();
    currentTokenI++;
    defineBytes(currentObjectFile->tokens[currentTokenI].string, 2, true);
}

/**
 * Defines a series of 4 byte values at the current program counter, stored in low endian format.
 * 
 * USAGE: .dw value[, value...]
 * 
 * Each value must be a valid value capable of being evaluated in the parse phase.
 */
void Assembler::DIR_DW_LO() {
    EXPECT_OPERAND();
    currentTokenI++;
    defineBytes(currentObjectFile->tokens[currentTokenI].string, 4, true);
}

/**
 * Defines a series of 8 byte values at the current program counter, stored in low endian format.
 * 
 * USAGE: .d2w value[, value...]
 * 
 * Each value must be a valid value capable of being evaluated in the parse phase.
 */
void Assembler::DIR_D2W_LO() {
    EXPECT_OPERAND();
    currentTokenI++;
    defineBytes(currentObjectFile->tokens[currentTokenI].string, 8, true);
}

/**
 * Defines a series of 1 byte values at the current program counter, stored in high endian format.
 * 
 * USAGE: .dbhi value[, value...]
 * 
 * Each value must be a valid value capable of being evaluated in the parse phase.
 */
void Assembler::DIR_DB_HI() {
    EXPECT_OPERAND();
    currentTokenI++;
    defineBytes(currentObjectFile->tokens[currentTokenI].string, 1, false);
}

/**
 * Defines a series of 2 byte values at the current program counter, stored in high endian format.
 * 
 * USAGE: .d2bhi value[, value...]
 * 
 * Each value must be a valid value capable of being evaluated in the parse phase.
 */
void Assembler::DIR_D2B_HI() {
    EXPECT_OPERAND();
    currentTokenI++;
    defineBytes(currentObjectFile->tokens[currentTokenI].string, 2, false);
}

/**
 * Defines a series of 4 byte values at the current program counter, stored in high endian format.
 * 
 * USAGE: .dwhi value[, value...]
 * 
 * Each value must be a valid value capable of being evaluated in the parse phase.
 */
void Assembler::DIR_DW_HI() {
    EXPECT_OPERAND();
    currentTokenI++;
    defineBytes(currentObjectFile->tokens[currentTokenI].string, 4, false);
}

/**
 * Defines a series of 8 byte values at the current program counter, stored in high endian format.
 * 
 * USAGE: .d2whi value[, value...]
 * 
 * Each value must be a valid value capable of being evaluated in the parse phase.
 */
void Assembler::DIR_D2W_HI() {
    EXPECT_OPERAND();
    currentTokenI++;
    defineBytes(currentObjectFile->tokens[currentTokenI].string, 8, false);
}


/**
 * Defines a series of strings at the current program counter.
 * 
 * USAGE: .ascii "string"[, "string"...]
 * 
 * Each string must be a valid string literal.
 * 
 * @throws INTERNAL_ERROR If there is not enough memory space to write to.
 */
void Assembler::DIR_ASCII() {
    EXPECT_OPERAND();
    currentTokenI++;

    // split operand by commas
    std::vector<std::string> splitByComma = split(currentObjectFile->tokens[currentTokenI].string, ',');
    std::vector<std::string> strings;
    Word bytesNeeded = 0;
    for (std::string str : splitByComma) {
        strings.push_back(getStringToken(trim(str)));
        bytesNeeded += strings.back().size();
    }

    // check if there is enough memory space to write to
    if (currentSegment->programCounter + bytesNeeded < currentSegment->programCounter) {
        error(INTERNAL_ERROR, std::stringstream() 
                << "Failed DEFINESTRINGS: Current address " << stringifyHex(currentSegment->programCounter) 
                << " plus bytesNeeded " << stringifyHex(bytesNeeded) << " overflows "
				<< currentObjectFile->tokens[currentTokenI].errorstring());
    }

    // write each string
    for (std::string str : strings) {
        for (char c : str) {
            writeBytes(c, 1, true);
        }
    }
}

/**
 * Defines a series of strings at the current program counter with each string followed by a zero byte.
 * 
 * USAGE: .asciz "string"[, "string"...]
 * 
 * Each string must be a valid string literal.
 * 
 * @throws INTERNAL_ERROR If there is not enough memory space to write to.
 */
void Assembler::DIR_ASCIZ() {
    EXPECT_OPERAND();
    currentTokenI++;

    // split operand by commas
    std::vector<std::string> splitByComma = split(currentObjectFile->tokens[currentTokenI].string, ',');
    std::vector<std::string> strings;
    Word bytesNeeded = 0;
    for (std::string str : splitByComma) {
        strings.push_back(getStringToken(trim(str)));
        bytesNeeded += strings.back().size() + 1;
    }

    // check if there is enough memory space to write to
    if (currentSegment->programCounter + bytesNeeded < currentSegment->programCounter) {
        error(INTERNAL_ERROR, std::stringstream() 
                << "Failed DEFINESTRINGS: Current address " << stringifyHex(currentSegment->programCounter) 
                << " plus bytesNeeded " << stringifyHex(bytesNeeded) << " overflows "
				<< currentObjectFile->tokens[currentTokenI].errorstring());
    }

    // write each string
    for (std::string str : strings) {
        for (char c : str) {
            writeBytes(c, 1, true);
        }
        writeBytes(0, 1, true);
    }
}

/**
 * Advances the current program counter to the specified address.
 * 
 * USAGE: .advance address,[filler]
 * 
 * The address must be a valid 4 byte value capable of being evaluated in the parse phase.
 * The address must be greater than the current program counter.
 * The filler must be a valid 1 byte value, by default is set to 0.
 * 
 * @throws INVALID_TOKEN_ERROR If the address is not a valid value.
 * @throws UNRECOGNIZED_TOKEN_ERROR If there is an unrecognized operand.
 */
void Assembler::DIR_ADVANCE() {
	EXPECT_OPERAND();
    currentTokenI++;

    // split operand by commas
    std::vector<std::string> splitByComma = split(currentObjectFile->tokens[currentTokenI].string, ',');
    
    // no valid address operand
    if (splitByComma.size() == 0) {
        error(INVALID_TOKEN_ERROR, std::stringstream()
                << "Missing operand for .advance directive: " << currentObjectFile->tokens[currentTokenI].errorstring());
    }

    Word targetAddress = (Word) EXPECT_PARSEDVALUE(trim(splitByComma[0]), 0, 0xFFFFFFFF);

    // cannot advance address backwards
    if (targetAddress < currentSegment->programCounter) {
        error(INVALID_TOKEN_ERROR, std::stringstream() 
                << "Invalid address for .advance directive: " << stringifyHex(targetAddress) 
                << " must be greater than " << stringifyHex(currentSegment->programCounter) << " "
				<< currentObjectFile->tokens[currentTokenI].errorstring());
    }

    Byte filler = 0;

    // check if there is filler argument
    if (splitByComma.size() > 1) {
        filler = (Byte) EXPECT_PARSEDVALUE(trim(splitByComma[1]), 0, 0xFF);

        // too many operands for .advance directive
        if (splitByComma.size() > 2) {
            error(UNRECOGNIZED_TOKEN_ERROR, std::stringstream() 
                    << "Unrecognized operand for .advance directive: " << currentObjectFile->tokens[currentTokenI].errorstring());
        }
    }

    // fill the space with the filler value
    for (Word i = currentSegment->programCounter; i < targetAddress; i++) {
        writeBytes(filler, 1, true);
    }
}

/**
 * Fills a space with bytes. By default, the space is filled with a series of 1 bytes with a value of 0.
 * 
 * USAGE: .fill count[,size[,value]]
 * 
 * The count must be a valid 4 byte value capable of being evaluated in the parse phase.
 * The size must be a valid 1 byte value capable of being evaluated in the parse phase.
 * The value must be within the specified size, by default it is set to 0.
 * 
 * @throws INVALID_TOKEN_ERROR If the count, size, or value is not a valid value.
 * @throws UNRECOGNIZED_TOKEN_ERROR If there is an unrecognized operand.
 * @throws INTERNAL_ERROR If there is not enough memory space to write to.
 */
void Assembler::DIR_FILL() {
	EXPECT_OPERAND();
    currentTokenI++;

    // split operand by commas
    std::vector<std::string> splitByComma = split(currentObjectFile->tokens[currentTokenI].string, ',');

    // no valid fillcount operand
    if (splitByComma.size() == 0) {
        error(MISSING_TOKEN_ERROR, std::stringstream()
                << "Missing operand for .fill directive: " << currentObjectFile->tokens[currentTokenI].errorstring());
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
            error(UNRECOGNIZED_TOKEN_ERROR, std::stringstream() 
                    << "Unrecognized operand for .fill directive: " << currentObjectFile->tokens[currentTokenI].errorstring());
        }
    }

    // check to make sure that there is enough memory to write to
    if (currentSegment->programCounter + fillcount * size < currentSegment->programCounter) {
        error(INTERNAL_ERROR, std::stringstream() 
                << "Failed FILL: Current address " << stringifyHex(currentSegment->programCounter) 
                << " plus fillcount " << stringifyHex(fillcount) << " times size " << stringifyHex(size) << " overflows "
				<< currentObjectFile->tokens[currentTokenI].errorstring());
    }

    // fill the space with the filler value
	for (Word i = 0; i < fillcount; i++) {
		writeBytes(value, size, true);
	}
}

/**
 * Fills a space with bytes. By default, the space is filled with a series of bytes with a value of 0.
 * 
 * USAGE: .space count[,value]
 * 
 * The count must be a valid 4 byte value capable of being evaluated in the parse phase.
 * The value must be a valid 1 byte value, default is set to 0.
 * 
 * @throws MISSING_TOKEN_ERROR If the count operand is missing.
 * @throws INVALID_TOKEN_ERROR If the count or value is not a valid value.
 * @throws UNRECOGNIZED_TOKEN_ERROR If there is an unrecognized operand.
 * @throws INTERNAL_ERROR If there is not enough memory space to write to.
 */
void Assembler::DIR_SPACE() {
	EXPECT_OPERAND();
    currentTokenI++;

    // split operand by commas
    std::vector<std::string> splitByComma = split(currentObjectFile->tokens[currentTokenI].string, ',');

    // no valid count operand
    if (splitByComma.size() == 0) {
        error(MISSING_TOKEN_ERROR, std::stringstream()
                << "Missing operand for .space directive: " << currentObjectFile->tokens[currentTokenI].errorstring());
    }

    Word count = (Word) EXPECT_PARSEDVALUE(trim(splitByComma[0]), 0, 0xFFFFFFFF);
    Byte value = 0;

    // check for value argument if we are in assembling phase
    if (splitByComma.size() > 1) {
        value = (Byte) EXPECT_PARSEDVALUE(trim(splitByComma[1]), 0, 0xFF);

        // too many operands for .space directive
        if (splitByComma.size() > 2) {
            error(UNRECOGNIZED_TOKEN_ERROR, std::stringstream() 
                    << "Unrecognized operand for .space directive: " << currentObjectFile->tokens[currentTokenI].errorstring());
        }
    }

    // check to make sure that there is enough memory to write to
    if (currentSegment->programCounter + count < currentSegment->programCounter) {
        error(INTERNAL_ERROR, std::stringstream() 
                << "Failed SPACE: Current address " << stringifyHex(currentSegment->programCounter) 
                << " plus count " << stringifyHex(count) << " overflows " << 
				currentObjectFile->tokens[currentTokenI].errorstring());
    }

    // fill the space with the filler value
    for (Word i = 0; i < count; i++) {
        writeBytes(value, 1, true);
    }
}

/**
 * Marks a label or macro as global.
 * 
 * .global label (marked as a global label)
 * .global .macro name (marked as a global macro)
 * .global .macro name arg1, arg2, ...
 * 
 * USAGE: .global label or .global .macro name [arg1, arg2, ...]
 */
void Assembler::DIR_GLOBAL() {
	EXPECT_OPERAND();
	int firstGlobalToken = currentTokenI;
	currentTokenI++;

	// must be defined in the file scope... this is a requirement for the linker
	if (currentScope != currentObjectFile->filescope) {
		error(INVALID_TOKEN_ERROR, std::stringstream() << ".global must be defined in filescope: " 
				<< currentObjectFile->tokens[currentTokenI].errorstring());
	}

	std::string operand = currentObjectFile->tokens[currentTokenI].string;
	if (operand == ".macro") {
		currentTokenI++;
		std::string macroName = currentObjectFile->tokens[currentTokenI].string;
		std::vector<std::string> parameters;
		if (HAS_OPERAND()) {
			currentTokenI++;
			std::string operand = currentObjectFile->tokens[currentTokenI].string;
			parameters = split(operand, ',');
		}

		// check if the macro was already marked extern
		if (currentObjectFile->markedExternMacros.find(macroName) != currentObjectFile->markedExternMacros.end()) {
			if (currentObjectFile->markedExternMacros[macroName].find(parameters.size()) != currentObjectFile->markedExternMacros[macroName].end()) {
				error(INVALID_TOKEN_ERROR, std::stringstream() << "Macro Already Extern: "
					<< currentObjectFile->tokens[currentTokenI].errorstring());
			}
		}

		// check if the macro was already marked global
		if (currentObjectFile->markedGlobalMacros.find(macroName) != currentObjectFile->markedGlobalMacros.end()) {
			if (currentObjectFile->markedGlobalMacros[macroName].find(parameters.size()) != currentObjectFile->markedGlobalMacros[macroName].end()) {
				error(INVALID_TOKEN_ERROR, std::stringstream() << "Macro Already Global: "
					<< currentObjectFile->tokens[currentTokenI].errorstring());
			}
		}

		// check if the macro was already defined in the filescope
		if (currentObjectFile->filescope->macros.find(macroName) != currentObjectFile->filescope->macros.end()) {
			error(MULTIPLE_DEFINITION_ERROR, std::stringstream() << "Multiple Defined Macros: "
					<< currentObjectFile->tokens[currentTokenI].errorstring());
		}

		// mark as global macro
		currentObjectFile->markedGlobalMacros[macroName].insert(parameters.size());
	} else {
		// we have a label
		if (!isValidLabelName(operand)) {
			error(INVALID_TOKEN_ERROR, std::stringstream() << "Invalid Label Name: " 
					<< currentObjectFile->tokens[currentTokenI].errorstring());
		}

		// check if the label was already marked extern
		if (currentObjectFile->markedExternSymbols.find(operand) != currentObjectFile->markedExternSymbols.end()) {
			error(INVALID_TOKEN_ERROR, std::stringstream() << "Label Already Extern: " 
					<< currentObjectFile->tokens[currentTokenI].errorstring());
		}

		// check if the label was already marked global
		if (currentObjectFile->markedGlobalSymbols.find(operand) == currentObjectFile->markedGlobalSymbols.end()) {
			error(INVALID_TOKEN_ERROR, std::stringstream() << "Label Already Global: " 
					<< currentObjectFile->tokens[currentTokenI].errorstring());
		}

		// warn if the label was already defined in the filescope.
		if (currentObjectFile->filescope->symbols.find(operand) != currentObjectFile->filescope->symbols.end()) {
			warn(WARN, std::stringstream() << "Label Already Defined: " 
					<< currentObjectFile->tokens[currentTokenI].errorstring());
		}

		// mark as global label (symbol)
		currentObjectFile->markedGlobalSymbols.insert(operand);
	}

	// remove global directive from tokens
	currentObjectFile->tokens.erase(currentObjectFile->tokens.begin() + firstGlobalToken, currentObjectFile->tokens.begin() + currentTokenI + 1);
}

/**
 * Marks a label or macro as being defined in one of the included files.
 * 
 * .extern label (tells the assembler that the label is defined in another file)
 * .extern .macro name (tells the assembler that the macro is defined in another file)
 * .extern .macro name arg1, arg2, ...
 * 
 * USAGE: .extern label or .extern .macro name [arg1, arg2, ...]
 */
void Assembler::DIR_EXTERN() {
	EXPECT_OPERAND();
	int firstExternToken = currentTokenI;
	currentTokenI++;

	// must be defined in the file scope... this is a requirement for the linker
	if (currentScope != currentObjectFile->filescope) {
		error(INVALID_TOKEN_ERROR, std::stringstream() << ".extern must be defined in filescope: " 
				<< currentObjectFile->tokens[currentTokenI].errorstring());
	}

	std::string operand = currentObjectFile->tokens[currentTokenI].string;
	if (operand == ".macro") {
		currentTokenI++;
		std::string macroName = currentObjectFile->tokens[currentTokenI].string;
		std::vector<std::string> parameters;
		if (HAS_OPERAND()) {
			currentTokenI++;
			std::string operand = currentObjectFile->tokens[currentTokenI].string;
			parameters = split(operand, ',');
		}

		// check if the macro was already marked extern
		if (currentObjectFile->markedExternMacros.find(macroName) != currentObjectFile->markedExternMacros.end()) {
			if (currentObjectFile->markedExternMacros[macroName].find(parameters.size()) != currentObjectFile->markedExternMacros[macroName].end()) {
				error(INVALID_TOKEN_ERROR, std::stringstream() << "Macro Already Extern: "
					<< currentObjectFile->tokens[currentTokenI].errorstring());
			}
		}

		// check if the macro was already marked global
		if (currentObjectFile->markedGlobalMacros.find(macroName) != currentObjectFile->markedGlobalMacros.end()) {
			if (currentObjectFile->markedGlobalMacros[macroName].find(parameters.size()) != currentObjectFile->markedGlobalMacros[macroName].end()) {
				error(INVALID_TOKEN_ERROR, std::stringstream() << "Macro Already Global: "
					<< currentObjectFile->tokens[currentTokenI].errorstring());
			}
		}

		// check if the macro was already defined in the filescope
		if (currentObjectFile->filescope->macros.find(macroName) != currentObjectFile->filescope->macros.end()) {
			error(MULTIPLE_DEFINITION_ERROR, std::stringstream() << "Multiple Defined Macros: "
					<< currentObjectFile->tokens[currentTokenI].errorstring());
		}

		// mark as extern macro
		currentObjectFile->markedExternMacros[macroName].insert(parameters.size());
	} else {
		// we have a label
		if (!isValidLabelName(operand)) {
			error(INVALID_TOKEN_ERROR, std::stringstream() << "Invalid Label Name: " 
					<< currentObjectFile->tokens[currentTokenI].errorstring());
		}

		// check if the label was already marked extern
		if (currentObjectFile->markedExternSymbols.find(operand) == currentObjectFile->markedExternSymbols.end()) {
			error(INVALID_TOKEN_ERROR, std::stringstream() << "Label Already Extern: " 
					<< currentObjectFile->tokens[currentTokenI].errorstring());
		}

		// check if the label was already marked global
		if (currentObjectFile->markedGlobalSymbols.find(operand) != currentObjectFile->markedGlobalSymbols.end()) {
			error(INVALID_TOKEN_ERROR, std::stringstream() << "Label Already Global: " 
					<< currentObjectFile->tokens[currentTokenI].errorstring());
		}

		// check if the label was already defined in the filescope.
		if (currentObjectFile->filescope->symbols.find(operand) != currentObjectFile->filescope->symbols.end()) {
			error(MULTIPLE_DEFINITION_ERROR, std::stringstream() << "Multiple Defined Labels: " 
					<< currentObjectFile->tokens[currentTokenI].errorstring());
		}

		// mark as global label (symbol)
		currentObjectFile->markedExternSymbols.insert(operand);
	}

	// remove extern directive from tokens
	currentObjectFile->tokens.erase(currentObjectFile->tokens.begin() + firstExternToken, currentObjectFile->tokens.begin() + currentTokenI + 1);
}

/**
 * Defines a label with a specific value.
 * 
 * USAGE: .define name,value
 * 
 * The name must be a valid label name.
 * The value must be a valid value capable of being evaluated in the parse phase.
 * 
 * @throws MISSING_TOKEN_ERROR If the name or value operand is missing.
 * @throws INVALID_TOKEN_ERROR If the name is not a valid label name or the value is not a valid value.
 * @throws UNRECOGNIZED_TOKEN_ERROR If there is an unrecognized operand.
 */
void Assembler::DIR_EQU() {
	EXPECT_OPERAND();
    currentTokenI++;
	std::string operand = currentObjectFile->tokens[currentTokenI].string;

    // split operand by commas
    std::vector<std::string> splitByComma = split(operand, ',');

    // no valid name operand
    if (splitByComma.size() == 0) {
        error(MISSING_TOKEN_ERROR, std::stringstream()
                << "Missing label name operand for .define directive: " 
				<< currentObjectFile->tokens[currentTokenI].errorstring());
    }

    std::string name = trim(splitByComma[0]);

    if (name.size() == 0) {
        error(INVALID_TOKEN_ERROR, std::stringstream() 
                << "Invalid label name for .define directive: "
				<< currentObjectFile->tokens[currentTokenI].errorstring());
    }

    if (splitByComma.size() < 2) {
        error(MISSING_TOKEN_ERROR, std::stringstream()
                << "Missing value operand for .define directive: " 
				<< currentObjectFile->tokens[currentTokenI].errorstring());
    }

    // define label with value
    Word value = EXPECT_PARSEDVALUE(trim(splitByComma[1]), 0, 0xFFFFFFFF);
    defineLabel(name, value);

    // too many operands for .define directive
    if (splitByComma.size() > 2) {
        error(UNRECOGNIZED_TOKEN_ERROR, std::stringstream() 
                << "Unrecognized operand for .define directive: " 
				<< currentObjectFile->tokens[currentTokenI].errorstring());
    }
}

/**
 * Checks the current program counter to make sure it does not overstep a memory boundary.
 * 
 * USAGE: .checkpc address
 * 
 * The address must be a value capable of being evaluated in the parse phase.
 * 
 * @throws INTERNAL_ERROR If the current program counter is greater than the checkpc address.
 */
void Assembler::DIR_CHECKPC() {
	EXPECT_OPERAND();
    currentTokenI++;

    Word checkpc = (Word) EXPECT_PARSEDVALUE(0, 0xFFFFFFFF);
    
    if (currentSegment->programCounter > checkpc) {
        error(INTERNAL_ERROR, std::stringstream() 
                << "Failed CHECKPC: Current address " << stringifyHex(currentSegment->programCounter) 
                << " is greater than checkpc " << stringifyHex(checkpc) << " " 
				<< currentObjectFile->tokens[currentTokenI].errorstring());
    }
}

/**
 * Aligns the current program counter to the next multiple of the align value.
 * 
 * USAGE: .align value
 * 
 * The align value must be a value that is capable of being evaluated in the parse phase.
 * If the align causes the program counter to overflow, an error will be thrown.
 * 
 * @throws INTERNAL_ERROR If the current program counter is greater than the checkpc address.
 */
void Assembler::DIR_ALIGN() {
	EXPECT_OPERAND();
    currentTokenI++;

    Word align = (Word) EXPECT_PARSEDVALUE(0, 0xFFFFFFFF);
    Word previousProgramCounter = currentSegment->programCounter;

    // align the current program counter to the next multiple of the align value
    currentSegment->programCounter = (currentSegment->programCounter + align - 1) & ~(align - 1);
    
    // check to make sure we did not go backwards
    if (currentSegment->programCounter < previousProgramCounter) {
        error(INTERNAL_ERROR, std::stringstream() 
                << "Failed ALIGN: Current address " << stringifyHex(currentSegment->programCounter) 
                << " is less than previous address " << stringifyHex(previousProgramCounter) << " " 
				<< currentObjectFile->tokens[currentTokenI].errorstring());
    }
}

void Assembler::DIR_INCLUDE() {
	EXPECT_OPERAND();
    currentTokenI++;

	// must be in filescope
	if (currentScope != currentObjectFile->filescope) {
		error(INVALID_TOKEN_ERROR, std::stringstream() << ".include must be defined in filescope: " 
				<< currentObjectFile->tokens[currentTokenI].errorstring());
	}

	std::string includedFile = getStringToken(currentObjectFile->tokens[currentTokenI].string);
	
	// check if file exists
	bool fileExist = false;
	for (std::string file : files) {
		if (file == includedFile) {
			fileExist = true;
			break;
		}
	}

	if (!fileExist) {
		error(INVALID_TOKEN_ERROR, std::stringstream() << "File does not exist: " 
				<< currentObjectFile->tokens[currentTokenI].errorstring());
	}

	// check if file is already included
	for (std::string file : currentObjectFile->includedFiles) {
		if (file == includedFile) {
			error(INVALID_TOKEN_ERROR, std::stringstream() << "File already included: " 
					<< currentObjectFile->tokens[currentTokenI].errorstring());
		}
	}

	// include the file
	currentObjectFile->includedFiles.push_back(includedFile);

	// remove include directive from tokens
	currentObjectFile->tokens.erase(currentObjectFile->tokens.begin() + currentTokenI - 1, currentObjectFile->tokens.begin() + currentTokenI + 1);
}

/**
 * Starts a new scope.
 * 
 * USAGE: .scope
 * 
 * This directive is used to start a new scope. A scope has a set of local labels which are only visible
 * within that scope. This is useful for defining labels which are only used within a specific
 * section of code.
 */
void Assembler::DIR_SCOPE() {
	startScope();
}

/**
 * Ends the current scope.
 * 
 * USAGE: .scend
 * 
 * This directive is used to end the current scope. This will return the assembler to the parent scope.
 * If there is no parent scope, this will throw an error.
 */
void Assembler::DIR_SCEND() {
	endScope();
}


void Assembler::DIR_MACRO() {
	int firstMacroToken = currentTokenI;

	EXPECT_OPERAND();
	currentTokenI++;

	// must be in filescope
	if (currentScope != currentObjectFile->filescope) {
		error(INVALID_TOKEN_ERROR, std::stringstream() << ".macro must be defined in filescope: " 
				<< currentObjectFile->tokens[currentTokenI].errorstring());
	}

	// check if the macro was already defined
	std::string name = currentObjectFile->tokens[currentTokenI].string;
	std::vector<std::string> parameters;
	if (HAS_OPERAND()) {
		currentTokenI++;
		std::string operand = currentObjectFile->tokens[currentTokenI].string;
		parameters = split(operand, ',');
	}
	
	// check if the macro was already defined
	Macro* macro;
	if (currentScope->macros.find(name) != currentScope->macros.end()) {
		if (currentScope->macros.at(name)->macros.find(parameters.size()) != currentScope->macros.at(name)->macros.end()) {
			error(MULTIPLE_DEFINITION_ERROR, std::stringstream() << "Multiple Defined Macro: " 
					<< currentObjectFile->tokens[currentTokenI].errorstring());
		}
		
		macro = currentScope->macros.at(name);
	} else {
		// create the new macro
		macro = new Macro(name);
		currentScope->macros.insert(std::pair<std::string, Macro*>(name, macro));
	}

	std::vector<std::string> macroDefinition;

	// parse the macro definition
	while (currentTokenI < currentObjectFile->tokens.size()) {
		if (currentObjectFile->tokens[currentTokenI].string == ".macend") {
			// expect no operands
			EXPECT_NO_OPERAND();

			break;
		}

		macroDefinition.push_back(currentObjectFile->tokens[currentTokenI].string);
		currentTokenI++;
	}

	// add macro definition
	macro->macros.insert(std::pair<u64, std::vector<std::string>>(parameters.size(), macroDefinition));

	// remove macro definition from tokens
	currentObjectFile->tokens.erase(currentObjectFile->tokens.begin() + firstMacroToken, currentObjectFile->tokens.begin() + currentTokenI + 1);
}

void Assembler::DIR_MACEND() {
	// should not ever reach here
}

/**
 * 
 */
void Assembler::DIR_INVOKE() {
	// we need to expand out the macro definition TODO:
}

