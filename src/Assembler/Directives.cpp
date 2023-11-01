#include "Assembler.h"


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

