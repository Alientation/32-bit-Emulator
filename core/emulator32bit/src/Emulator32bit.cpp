#include <emulator32bit/Emulator32bit.h>

const word Emulator32bit::RAM_MEM_SIZE = 1024;
const word Emulator32bit::RAM_MEM_START = 0;
const byte Emulator32bit::ROM_DATA[] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
const word Emulator32bit::ROM_MEM_SIZE = 1024;
const word Emulator32bit::ROM_MEM_START = 1024;

Emulator32bit::Emulator32bit() : system_bus(new RAM(RAM_MEM_SIZE, RAM_MEM_START), new ROM(ROM_DATA, ROM_MEM_SIZE, ROM_MEM_START)) {
	// Constructor
	// fill out instruction functions
	#define _INSTR(op) _instructions[_op_##op] = std::bind(&Emulator32bit::_##op, this, std::placeholders::_1, std::placeholders::_2);
	_INSTR(hlt)

	_INSTR(add)
	_INSTR(sub)
	_INSTR(rsb)
	_INSTR(adc)
	_INSTR(sbc)
	_INSTR(rsc)
	_INSTR(mul)
	_INSTR(umull)
	_INSTR(smull)

	_INSTR(vabs_f32)
	_INSTR(vneg_f32)
	_INSTR(vsqrt_f32)
	_INSTR(vadd_f32)
	_INSTR(vsub_f32)
	_INSTR(vdiv_f32)
	_INSTR(vmul_f32)
	_INSTR(vcmp_f32)
	_INSTR(vsel_f32)
	_INSTR(vcint_f32)
	_INSTR(vcflo_f32)
	_INSTR(vmov_f32)

	_INSTR(and)
	_INSTR(orr)
	_INSTR(eor)
	_INSTR(bic)
	_INSTR(lsl)
	_INSTR(lsr)
	_INSTR(asr)
	_INSTR(ror)

	_INSTR(cmp)
	_INSTR(cmn)
	_INSTR(tst)
	_INSTR(teq)
	
	_INSTR(mov)
	_INSTR(mvn)
	
	_INSTR(ldr)
	_INSTR(ldrb)
	_INSTR(ldrh)
	_INSTR(str)
	_INSTR(strb)
	_INSTR(strh)
	_INSTR(swp)
	_INSTR(swpb)
	_INSTR(swph)
	
	_INSTR(b)
	_INSTR(bl)
	_INSTR(bx)
	_INSTR(blx)
	_INSTR(swi)
	
	// _INSTR(nop_)
	// _INSTR(nop_)
	// _INSTR(nop_)
	// _INSTR(nop_)
	// _INSTR(nop_)
	// _INSTR(nop_)
	// _INSTR(nop_)
	// _INSTR(nop_)
	// _INSTR(nop_)
	// _INSTR(nop_)
	// _INSTR(nop_)
	// _INSTR(nop_)
	// _INSTR(nop_)

	_INSTR(nop)
	#undef _INSTR
}

Emulator32bit::~Emulator32bit() {
	
}

void Emulator32bit::run(unsigned int instructions, EmulatorException &exception) {
	// Run the emulator for a given number of instructions
	while (instructions > 0 && exception.isOK()) {
		word instr = system_bus.readWord(_pc, exception.sys_bus_exception, exception.mem_read_exception);
		exception.instr = instr;
		execute(instr, exception);
		_pc += 4;
	}

	if (!exception.isOK()) {
		handle_exception(exception);
	}
}

void Emulator32bit::reset() {
	system_bus.reset();
	for (int i = 0; i < sizeof(_x) / sizeof(_x[0]); i++) {
		_x[i] = 0;
	}
	_pstate = 0;
	_pc = 0;

}

void Emulator32bit::execute(word instr, EmulatorException &exception) {
	byte opcode = (instr) & ~((1 << 26) - 1);

	if (!_instructions[opcode]) {
		exception.type = EmulatorException::Type::BAD_INSTR;
		return;
	}

	_instructions[opcode](instr, exception);
}

word Emulator32bit::read_reg(byte reg, EmulatorException &exception) {
	if (reg == XZR) {
		return 0;
	} else if (reg < (sizeof(_x) / sizeof(word))) {
		return _x[reg];
	}

	exception.type = EmulatorException::Type::BAD_REG;
	return 0;
}

void Emulator32bit::write_reg(byte reg, word val, EmulatorException &exception) {
	if (reg == XZR) {
		return;
	} else if (reg < (sizeof(_x) / sizeof(word))) {
		_x[reg] = val;
		return;
	}

	exception.type = EmulatorException::Type::BAD_REG;
}

void Emulator32bit::handle_exception(EmulatorException &exception) {
	// todo later
}

void Emulator32bit::set_NZCV(bool N, bool Z, bool C, bool V) {
	_pstate = set_bit(_pstate, N_FLAG, N);
	_pstate = set_bit(_pstate, Z_FLAG, Z);
	_pstate = set_bit(_pstate, C_FLAG, C);
	_pstate = set_bit(_pstate, V_FLAG, V);
}


bool Emulator32bit::EmulatorException::isOK() {
	return type == EmulatorException::Type::AOK && sys_bus_exception.type == SystemBus::SystemBusException::AOK
		&& mem_read_exception.type == Memory::MemoryReadException::Type::AOK
		&& mem_write_exception.type == Memory::MemoryWriteException::Type::AOK;
}