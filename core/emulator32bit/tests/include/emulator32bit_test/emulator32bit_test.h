#pragma once

#include <emulator32bit/emulator32bit.h>
#include <emulator32bit/emulator32bit_util.h>
#include <gtest/gtest.h>
#include <new>

class EmulatorFixture : public ::testing::Test
{
  protected:
    Emulator32bit cpu{1, 0, {}, 0, 1};

    void reset(word ram_npages, word ram_start_page, const byte rom_data[], word rom_npages,
               word rom_start_page)
    {
        cpu.~Emulator32bit();
        new (&cpu) Emulator32bit{ram_npages, ram_start_page, rom_data, rom_npages, rom_start_page};
    }

    /// Writes `instr` at `addr`, points the PC at it and executes exactly one instruction.
    void step(word addr, word instr)
    {
        cpu.system_bus->write_word(addr, instr);
        cpu.set_pc(addr);
        cpu.run(1);
    }
};