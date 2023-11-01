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
 * This means that this section of program memory could be moved around by the linker or loader.
 * 
 * USAGE: .org value
 * 
 * The value must be either a numeric constant defined in the current file or a label
 * defined in the current file. The value must be a 32-bit value.
 */
void Assembler::DIR_ORG_RELATIVE() {

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

}

void Assembler::defineBytes(std::string token, Byte bytes, bool lowEndian) {

}

void Assembler::DIR_DB_LO() {

}

void Assembler::DIR_D2B_LO() {

}

void Assembler::DIR_DW_LO() {

}

void Assembler::DIR_D2W_LO() {

}

void Assembler::DIR_DB_HI() {

}

void Assembler::DIR_D2B_HI() {

}

void Assembler::DIR_DW_HI() {

}

void Assembler::DIR_D2W_HI() {

}

void Assembler::DIR_ASCII() {

}

void Assembler::DIR_ASCIZ() {

}

void Assembler::DIR_ADVANCE() {

}

void Assembler::DIR_FILL() {

}

void Assembler::DIR_SPACE() {

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
	currentTokenI++;

	std::string operand = currentObjectFile->tokens[currentTokenI].string;
	if (operand == ".macro") {

	} else {
		// we have a label
		if (!isValidLabelName(operand)) {
			error(INVALID_TOKEN_ERROR, std::stringstream() << "Invalid Label Name: " 
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
	currentTokenI++;

	std::string operand = currentObjectFile->tokens[currentTokenI].string;
	if (operand == ".macro") {

	} else {
		// we have a label
		if (!isValidLabelName(operand)) {
			error(INVALID_TOKEN_ERROR, std::stringstream() << "Invalid Label Name: " 
					<< currentObjectFile->tokens[currentTokenI].errorstring());
		}

		// check if the label was already marked global
		if (currentObjectFile->markedExternSymbols.find(operand) == currentObjectFile->markedExternSymbols.end()) {
			error(INVALID_TOKEN_ERROR, std::stringstream() << "Label Already Extern: " 
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

}

void Assembler::DIR_MACEND() {

}

/**
 * 
 */
void Assembler::DIR_INVOKE() {
	// we need to expand out the macro definition
}

