
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
        // 0 instructions means to run until something stops the program. The first instruction
        // never stops at a breakpoint, that is how a run goes on from one.
        bool first = true;
        while (instructions == 0 || result.instructions_ran < instructions)
        {
            if (UNLIKELY(!m_breakpoints.empty()) && !first && m_breakpoints.contains(m_pc))
            {
                result.status = RunResult::Status::BREAKPOINT;
                result.message = std::format("Breakpoint at {:#010x}", m_pc);
                break;
            }
            first = false;

            const word instr = fetch_instruction();
            if (UNLIKELY(m_history_size != 0))
            {
                if (m_history.size() == m_history_size)
                {
                    m_history.pop_front();
                }
                m_history.push_back({.pc = m_pc, .instruction = instr});
            }

            if (UNLIKELY(m_trace != nullptr))
            {
                execute_traced(instr);
            }
            else
            {
                execute(instr);
            }
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

void Emulator32bit::add_breakpoint(const word pc)
{
    m_breakpoints.insert(pc);
}

bool Emulator32bit::remove_breakpoint(const word pc)
{
    return m_breakpoints.erase(pc) != 0;
}

void Emulator32bit::clear_breakpoints()
{
    m_breakpoints.clear();
}

const std::set<word> &Emulator32bit::breakpoints() const
{
    return m_breakpoints;
}

void Emulator32bit::set_trace(std::ostream *out)
{
    m_trace = out;
}

void Emulator32bit::set_history_size(const size_t count)
{
    m_history_size = count;
    while (m_history.size() > count)
    {
        m_history.pop_front();
    }
}

size_t Emulator32bit::history_size() const
{
    return m_history_size;
}

std::vector<Emulator32bit::ExecutedInstruction> Emulator32bit::history() const
{
    return {m_history.begin(), m_history.end()};
}

void Emulator32bit::set_symbols(const SymbolMap *symbols)
{
    m_symbols = symbols;
}

namespace
{

/// "NzCv": an upper case letter is a flag that is set.
std::string flag_letters(const word pstate)
{
    std::string letters;
    letters += test_bit(pstate, kNFlagBit) ? 'N' : 'n';
    letters += test_bit(pstate, kZFlagBit) ? 'Z' : 'z';
    letters += test_bit(pstate, kCFlagBit) ? 'C' : 'c';
    letters += test_bit(pstate, kVFlagBit) ? 'V' : 'v';
    return letters;
}

} // namespace

// `0x00000010 <main+0x4>: add x0, x1, 4 ; x0=0x1->0x5 NZCV=nzcv->nzCv`, with the reason in place of
// the changes when the instruction does not complete. A taken branch shows `pc=target`.
void Emulator32bit::execute_traced(const word instr)
{
    const word pc = m_pc;
    word regs_before[kNumReg];
    for (U8 r = 0; r < kNumReg; r++)
    {
        regs_before[r] = m_x[r];
    }
    const word pstate_before = m_pstate;

    std::string line = std::format("{:#010x}", pc);
    if (m_symbols != nullptr)
    {
        const std::string name = m_symbols->describe(pc);
        if (!name.empty())
        {
            line += " <" + name + ">";
        }
    }
    line += ": " + disassemble_instr(instr);

    try
    {
        execute(instr);
    }
    catch (const Exception &e)
    {
        *m_trace << line << " ; " << (e.get_type() == InterruptType::HALT_INSTR ? "halt" : e.what())
                 << "\n";
        throw;
    }
    catch (const std::exception &e)
    {
        *m_trace << line << " ; " << e.what() << "\n";
        throw;
    }

    std::string changes;
    for (U8 r = 0; r < kNumReg; r++)
    {
        if (m_x[r] != regs_before[r])
        {
            changes += std::format(" {}={:#x}->{:#x}",
                                   r == register_to_U8(Register::SP) ? std::string("sp")
                                                                     : "x" + std::to_string(r),
                                   regs_before[r], m_x[r]);
        }
    }
    if (m_pstate != pstate_before)
    {
        changes += " NZCV=" + flag_letters(pstate_before) + "->" + flag_letters(m_pstate);
    }
    if (m_pc != pc)
    {
        changes += std::format(" pc={:#x}", m_pc + 4);
    }

    *m_trace << line << (changes.empty() ? "" : " ;" + changes) << "\n";
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