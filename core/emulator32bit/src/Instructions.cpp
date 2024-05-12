#include <emulator32bit/Emulator32bit.h>

void Emulator32bit::_hlt(word instr, EmulatorException& exception) {
	exception.type = EmulatorException::Type::HALT;
}