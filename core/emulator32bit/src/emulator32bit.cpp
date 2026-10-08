
#include "emulator32bit/emulator32bit.h"
#include "emulator32bit/virtual_memory.h"
#include "util/logger.h"
#include "util/types.h"

#include <format>

namespace
{

// AEMU_OPCODES is the only list of opcodes, so this is where a mistake in it is caught.
constexpr word kListedOpcodes[] = {
#define AEMU_OPCODE_VALUE(name, opcode) opcode,
    AEMU_OPCODES(AEMU_OPCODE_VALUE)
#undef AEMU_OPCODE_VALUE
};

constexpr bool opcodes_are_valid()
{
    for (size_t i = 0; i < std::size(kListedOpcodes); i++)
    {
        if (kListedOpcodes[i] >= kMaxInstructions)
        {
            return false; // does not fit in the 6 bit opcode field
        }
        for (size_t j = 0; j < i; j++)
        {
            if (kListedOpcodes[i] == kListedOpcodes[j])
            {
                return false; // two instructions with one opcode
            }
        }
    }
    return true;
}

static_assert(opcodes_are_valid(), "AEMU_OPCODES has an opcode that is used twice or is above 63");

} // namespace

Emulator32bit::Emulator32bit(word ram_npages, word ram_start_page, const byte rom_data[],
                             word rom_npages, word rom_start_page) :
    system_bus(new SystemBus(new RAM(ram_npages, ram_start_page),
                             new ROM(rom_data, rom_npages, rom_start_page)))
{
    fill_out_instructions();
    reset();
}

Emulator32bit::Emulator32bit() :
    Emulator32bit(RAM_NPAGES, RAM_START_PAGE, ROM_DATA, ROM_NPAGES, ROM_START_PAGE)
{
}

Emulator32bit::Emulator32bit(RAM *ram, ROM *rom, Disk *disk) :
    system_bus(new SystemBus(ram, rom, disk,
                             new VirtualMemory(disk, ram->get_lo_page(), ram->get_mem_pages())))
{
    fill_out_instructions();
    reset();
}

Emulator32bit::~Emulator32bit() = default;

Emulator32bit::Exception::Exception(Emulator32bit::InterruptType type, const std::string &msg) :
    type(type),
    message(msg)
{
}

const char *Emulator32bit::Exception::what() const noexcept
{
    return message.c_str();
}

Emulator32bit::InterruptType Emulator32bit::Exception::get_type() const noexcept
{
    return type;
}

void Emulator32bit::fill_out_instructions()
{
    for (int i = 0; i < kMaxInstructions; i++)
    {
        m_instruction_handler[i] = &Emulator32bit::_bad_opcode;
    }

#define AEMU_SET_HANDLER(name, opcode) m_instruction_handler[_op_##name] = &Emulator32bit::_##name;
    AEMU_OPCODES(AEMU_SET_HANDLER)
#undef AEMU_SET_HANDLER
}

void Emulator32bit::print()
{
    *m_out << "32 bit emulator\nRegisters:\n";
    *m_out << std::format(" pc: {}\n sp: {}\nxzr: {}\n", to_color_hex_str(m_pc),
                          to_color_hex_str(read_reg(Register::SP)), to_color_hex_str(word(0)));
    for (U8 i = 0; i <= register_to_U8(Register::X29); i++)
    {
        *m_out << std::format("x{:02}: {}\n", i, to_color_hex_str(read_reg(i)));
    }

    *m_out << std::format("\nN={:d} Z={:d} C={:d} V={:d}\n", test_bit(m_pstate, kNFlagBit),
                          test_bit(m_pstate, kZFlagBit), test_bit(m_pstate, kCFlagBit),
                          test_bit(m_pstate, kVFlagBit));
}

word Emulator32bit::fetch_instruction()
{
    if (UNLIKELY(m_pc & 0b11))
    {
        throw Exception(InterruptType::BAD_INSTR,
                        "Misaligned program counter " + std::to_string(m_pc));
    }
    return system_bus->fetch_instruction(m_pc);
}

Emulator32bit::RunResult Emulator32bit::run(U64 instructions)
{
    RunResult result{RunResult::Status::LIMIT_REACHED, 0, ""};

    // What stops the program is reported through the result, and logged at the level of how
    // unusual it is. Anything that goes wrong in the machine is a fault of the program, whatever
    // part of the machine it is in.
    const auto fault = [&](const char *what, const std::string &message)
    {
        AEMU_WARN("Emulator32bit::run() - {}: {}", what, message);
        result.status = RunResult::Status::FAULT;
        result.message = message;
    };

    try
    {
        // 0 instructions means to run until something stops the program.
        while (instructions == 0 || result.instructions_ran < instructions)
        {
            const word instr = fetch_instruction();
            execute(instr);
            m_pc += 4;
            result.instructions_ran++;
        }
    }
    catch (const Exception &e)
    {
        if (e.get_type() == InterruptType::HALT_INSTR)
        {
            AEMU_DEBUG("Emulator32bit::run() - Halted after {} instructions.",
                       result.instructions_ran);
            result.status = RunResult::Status::HALTED;
            result.message = e.what();
        }
        else
        {
            fault("Emulator exception", e.what());
        }
    }
    catch (const SystemBus::Exception &e)
    {
        fault("System bus exception", e.what());
    }
    catch (const VirtualMemory::VirtualMemoryException &e)
    {
        // E.g. an access to an unmapped virtual page, or to memory that the page does not allow.
        fault("Virtual memory exception", e.what());
    }
    catch (const FreeBlockList::FreeBlockListException &e)
    {
        // There are no more pages on the disk, or no more process numbers.
        fault("Out of resources", e.what());
    }
    catch (const Disk::DiskReadException &e)
    {
        fault("Disk read error", e.what());
    }
    catch (const Disk::DiskWriteException &e)
    {
        fault("Disk write error", e.what());
    }
    catch (const aemu::log::FatalError &e)
    {
        // Only thrown when the fatal action is Throw, and the error is logged already.
        result.status = RunResult::Status::FAULT;
        result.message = e.what();
    }

    return result;
}

void Emulator32bit::reset()
{
    system_bus->reset();
    static_assert(sizeof(m_x) / sizeof(m_x[0]) == kNumReg);
    for (word &reg : m_x)
    {
        reg = 0;
    }
    m_pstate = 0;
    m_pc = 0;
}