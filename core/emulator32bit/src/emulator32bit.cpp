
#include "emulator32bit/emulator32bit.h"
#include "emulator32bit/virtual_memory.h"
#include "emulator32bit/kernel/better_virtual_memory.h"
#include "emulator32bit/timer.h"

#include "util/types.h"

#include <stdio.h>

const word Emulator32bit::RAM_NPAGES = 16;
const word Emulator32bit::RAM_START_PAGE = 0;
const byte Emulator32bit::ROM_DATA[16 << PAGE_PSIZE] = {};
const word Emulator32bit::ROM_NPAGES = 16;
const word Emulator32bit::ROM_START_PAGE = 16;

Emulator32bit::Emulator32bit(word ram_npages, word ram_start_page, const byte rom_data[],
		word rom_npages, word rom_start_page) :
	ram(new RAM(ram_npages, ram_start_page)),
	rom(new ROM(rom_data, rom_npages, rom_start_page)),
	disk(new MockDisk()),
	mmu(new VirtualMemory(disk)),
	system_bus(*ram, *rom, *disk, *mmu)
{
	fill_out_instructions();
	reset();
}

Emulator32bit::Emulator32bit() :
	Emulator32bit(RAM_NPAGES, RAM_START_PAGE, ROM_DATA, ROM_NPAGES, ROM_START_PAGE)
{

}

Emulator32bit::Emulator32bit(RAM *ram, ROM *rom, Disk *disk) :
	ram(ram),
	rom(rom),
	disk(disk),
	mmu(new VirtualMemory(disk)),
	system_bus(*ram, *rom, *disk, *mmu)
{
	fill_out_instructions();
	reset();
}

Emulator32bit::~Emulator32bit()
{
	disk->save();
	delete mmu;
	delete ram;
	delete rom;
	delete disk;
}

Emulator32bit::Exception::Exception(Emulator32bit::InterruptType type, const std::string& msg) :
	type(type), message(msg)
{

}

const char* Emulator32bit::Exception::what() const noexcept
{
	return message.c_str();
}

void Emulator32bit::fill_out_instructions()
{
	for (int i = 0; i < _num_instructions; i++) {
		_instructions[i] = Emulator32bit::_hlt;
	}

	/* fill out instruction functions and construct disassembler instruction mapping */
	#define _INSTR(op) _instructions[_op_##op] = Emulator32bit::_##op;

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

	_INSTR(vabs)
	_INSTR(vneg)
	_INSTR(vsqrt)
	_INSTR(vadd)
	_INSTR(vsub)
	_INSTR(vdiv)
	_INSTR(vmul)
	_INSTR(vcmp)
	_INSTR(vsel)
	_INSTR(vcint)
	_INSTR(vcflo)
	_INSTR(vmov)

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

	_INSTR(adrp)

	_INSTR(hlt)

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

void Emulator32bit::print()
{
	printf("32 bit emulator\nRegisters:\n");
	printf(" pc: %s\n sp: %s\nxzr: %s\n", to_color_hex_str(_pc).c_str(), to_color_hex_str(read_reg(SP)).c_str(), to_color_hex_str((word)0).c_str());
	for (int i = 0; i < SP; i++) {
		printf("x%.2d: %s\n", i, to_color_hex_str(read_reg(i)).c_str());
	}

	printf("\nMemory Dump: TODO");
}

void Emulator32bit::run(unsigned long long instructions)
{
	word instr = _op_hlt;
	unsigned long long num_instructions_ran = 0;
	try
	{
		if (instructions == 0)
		{
			while (true)
			{
				instr = system_bus.read_word_aligned_ram(_pc);
				execute(instr);
				_pc += 4;
				num_instructions_ran++;
			}
		}
		else
		{
			unsigned long long start_instructions = instructions;
			while (instructions > 0)
			{
				instr = system_bus.read_word_aligned_ram(_pc);
				execute(instr);
				_pc += 4;
				instructions--;
			}
			num_instructions_ran = start_instructions - instructions;
		}
	}
	catch(const Exception& e)
	{
		std::cerr << "Caught Emulator Exception: " << e.what() << std::endl;
	}
	catch(const SystemBus::Exception& e)
	{
		std::cerr << "Caught System Bus Exception: " << e.what() << std::endl;
	}

	printf("Ran %llu instructions\n", num_instructions_ran);
}

void Emulator32bit::reset()
{
	system_bus.reset();
	for (unsigned long long i = 0; i < sizeof(_x) / sizeof(_x[0]); i++)
    {
		_x[i] = (1ULL << (8 * sizeof(word))) - 1;
	}
	_x[XZR] = 0;
	_pstate = 0;
	_pc = 0;

}