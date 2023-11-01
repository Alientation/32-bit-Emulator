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




void Assembler::DIR_DATA() {

}

void Assembler::DIR_TEXT() {

}

void Assembler::DIR_OUTFILE() {

}

void Assembler::DIR_END() {

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

void Assembler::DIR_GLOBAL() {

}

void Assembler::DIR_EXTERN() {

}

void Assembler::DIR_DEFINE() {

}

void Assembler::DIR_SET() {

}

void Assembler::DIR_CHECKPC() {

}

void Assembler::DIR_ALIGN() {

}

void Assembler::DIR_INCBIN() {

}

void Assembler::DIR_INCLUDE() {

}

void Assembler::DIR_REQUIRE() {

}

void Assembler::DIR_SCOPE() {

}

void Assembler::DIR_SCEND() {

}

void Assembler::DIR_MACRO() {

}

void Assembler::DIR_MACEND() {

}

void Assembler::DIR_INVOKE() {

}

