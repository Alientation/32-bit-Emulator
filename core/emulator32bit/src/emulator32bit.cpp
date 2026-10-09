
#include "emulator32bit/emulator32bit.h"
#include "emulator32bit/virtual_memory.h"
#include "util/logger.h"
#include "util/types.h"

#include <cctype>
#include <format>
#include <utility>

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
    Emulator32bit(std::make_unique<RAM>(ram_npages, ram_start_page),
                  std::make_unique<ROM>(rom_data, rom_npages, rom_start_page),
                  std::make_unique<MockDisk>())
{
}

Emulator32bit::Emulator32bit() :
    Emulator32bit(RAM_NPAGES, RAM_START_PAGE, ROM_DATA, ROM_NPAGES, ROM_START_PAGE)
{
}

Emulator32bit::Emulator32bit(std::unique_ptr<RAM> ram, std::unique_ptr<ROM> rom,
                             std::unique_ptr<Disk> disk) :
    system_bus(std::make_unique<SystemBus>(std::move(ram), std::move(rom), std::move(disk))),
    // The pages that are paged in are the pages of the RAM, and are swapped out to the disk. Made
    // after the bus owns them, so that they are freed with it if making the MMU throws.
    mmu(std::make_unique<VirtualMemory>(system_bus->disk.get(), system_bus->ram->get_lo_page(),
                                        system_bus->ram->get_mem_pages(), system_bus.get())),
    memory(*system_bus, *mmu)
{
    fill_out_instructions();
    reset();
}

Emulator32bit::~Emulator32bit() = default;

Emulator32bit::Exception::Exception(Emulator32bit::InterruptType type, const std::string &msg,
                                    const word iss) :
    type(type),
    message(msg),
    iss(iss)
{
}

word Emulator32bit::Exception::get_iss() const noexcept
{
    return iss;
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
    return memory.fetch_instruction(m_pc);
}

Emulator32bit::RunResult Emulator32bit::run(U64 instructions)
{
    RunResult result{RunResult::Status::LIMIT_REACHED, 0, ""};
    // the debugger may have changed it between runs
    m_watch_hit.clear();
    for (RegisterWatch &watch : m_register_watches) watch.last = read_reg(watch.reg);

    // True (and the result is set) when a watched register changed since it was last looked at.
    // Checked between instructions, which covers a retired instruction and an exception entry.
    const auto register_watch_hit = [&]()
    {
        for (RegisterWatch &watch : m_register_watches)
        {
            const word now = read_reg(watch.reg);
            if (now == watch.last) continue;
            const word before = std::exchange(watch.last, now);
            if (watch.value && now != *watch.value) continue;
            result.status = RunResult::Status::BREAKPOINT;
            result.message = std::format(
                "Register watch {}: {:#x} -> {:#x}, next instruction at {:#010x}",
                watch.reg == static_cast<U8>(Register::SP) ? std::string("sp")
                                                           : "x" + std::to_string(watch.reg),
                before, now, m_pc);
            return true;
        }
        return false;
    };

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
            if (UNLIKELY(!m_register_watches.empty()) && register_watch_hit()) break;

            if (UNLIKELY(!m_breakpoints.empty()) && !first && m_breakpoints.contains(m_pc))
            {
                result.status = RunResult::Status::BREAKPOINT;
                result.message = std::format("Breakpoint at {:#010x}", m_pc);
                break;
            }
            first = false;

            // An interrupt is taken between two instructions, if the program has installed a
            // vector table and unmasked IRQs. The resume address is the instruction that would
            // have executed.
            if (UNLIKELY(system_bus->intc.has_pending()) && m_vbar != 0
                && !test_bit(m_pstate, kIrqMaskBit))
            {
                enter_exception(ExceptionClass::IRQ, 0, 0, m_pc);
                m_pc_written = false;
                continue;
            }

            // With a vector table installed (VBAR != 0) the exceptions of the CPU are raised
            // instead of ending the run, and the next instruction is the first one of the handler.
            word instr;
            try
            {
                instr = fetch_instruction();
            }
            catch (const std::exception &error)
            {
                if (!deliver_exception(error, true)) throw;
                m_pc_written = false;
                continue;
            }

            if (UNLIKELY(m_history_size != 0))
            {
                if (m_history.size() == m_history_size) m_history.pop_front();
                m_history.push_back({.pc = m_pc, .instruction = instr});
            }

            try
            {
                if (UNLIKELY(m_trace != nullptr)) execute_traced(instr);
                else execute(instr);
            }
            catch (const std::exception &error)
            {
                if (!deliver_exception(error, false, instr)) throw;
                m_pc_written = false;
                continue;
            }

            // Whatever wrote the pc (ERET, an exception) wrote the address of the next
            // instruction itself.
            if (LIKELY(!m_pc_written)) m_pc += 4;
            m_pc_written = false;
            m_retired_since_entry = true;
            result.instructions_ran++;
            system_bus->timer.tick();
            system_bus->block.tick();

            if (UNLIKELY(!m_watch_hit.empty()))
            {
                result.status = RunResult::Status::BREAKPOINT;
                result.message = std::move(m_watch_hit);
                m_watch_hit.clear();
                break;
            }
        }

        // The loop looks at the registers before each instruction, so the last one is left.
        if (result.status == RunResult::Status::LIMIT_REACHED
            && UNLIKELY(!m_register_watches.empty()))
        {
            register_watch_hit();
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
            // The hlt ran, if it was the first instruction of a handler that is not a double fault.
            m_retired_since_entry = true;
        }
        else if (e.get_type() == InterruptType::BREAK_INSTR)
        {
            // The brk completed, the debugger goes on with the next instruction.
            m_pc += 4;
            m_retired_since_entry = true;
            result.instructions_ran++;
            result.status = RunResult::Status::BREAKPOINT;
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

void Emulator32bit::add_watchpoint(const word address, const word length, const WatchKind kind)
{
    AEMU_CHECK(length != 0, "A watchpoint needs a length of at least 1");
    remove_watchpoint(address);
    m_watchpoints.push_back({.address = address, .length = length, .kind = kind});
}

bool Emulator32bit::remove_watchpoint(const word address)
{
    return std::erase_if(m_watchpoints,
                         [&](const Watchpoint &watch) { return watch.address == address; })
           != 0;
}

void Emulator32bit::clear_watchpoints()
{
    m_watchpoints.clear();
}

const std::vector<Emulator32bit::Watchpoint> &Emulator32bit::watchpoints() const
{
    return m_watchpoints;
}

void Emulator32bit::add_register_watch(const U8 reg, const std::optional<word> value)
{
    AEMU_CHECK(reg <= static_cast<U8>(Register::SP), "Cannot watch register {}", reg);
    remove_register_watch(reg);
    m_register_watches.push_back({.reg = reg, .value = value, .last = read_reg(reg)});
}

bool Emulator32bit::remove_register_watch(const U8 reg)
{
    return std::erase_if(m_register_watches,
                         [&](const RegisterWatch &watch) { return watch.reg == reg; })
           != 0;
}

void Emulator32bit::clear_register_watches()
{
    m_register_watches.clear();
}

const std::vector<Emulator32bit::RegisterWatch> &Emulator32bit::register_watches() const
{
    return m_register_watches;
}

void Emulator32bit::watch_access(const word address, const word length, const bool write,
                                 const word value)
{
    if (!m_watch_hit.empty())
    {
        return; // the first hit of the instruction is the one that is reported
    }

    const U64 first = address;
    const U64 last = first + length; // one past, 64 bit so it cannot wrap
    for (const Watchpoint &watch : m_watchpoints)
    {
        const bool kind_matches =
            (static_cast<U8>(watch.kind)
             & (write ? static_cast<U8>(WatchKind::WRITE) : static_cast<U8>(WatchKind::READ)))
            != 0;
        if (kind_matches && first < U64(watch.address) + watch.length && U64(watch.address) < last)
        {
            m_watch_hit = std::format("Watchpoint {:#010x}: {} of {:#x} ({} byte{}) at {:#010x} by "
                                      "the instruction at {:#010x}",
                                      watch.address, write ? "write" : "read", value, length,
                                      length == 1 ? "" : "s", address, m_pc);
            return;
        }
    }
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
    const word next_pc = m_pc_written ? m_pc : m_pc + 4;
    if (next_pc != pc + 4)
    {
        changes += std::format(" pc={:#x}", next_pc);
    }

    *m_trace << line << (changes.empty() ? "" : " ;" + changes) << "\n";
}

namespace
{

/// The system registers by name, in the order of their numbers.
struct SysregName
{
    U8 id;
    const char *name;
};

constexpr SysregName kSysregNames[] = {
    {Emulator32bit::kSysregId_pstate, "pstate"}, {Emulator32bit::kSysregId_elr, "elr"},
    {Emulator32bit::kSysregId_spsr, "spsr"},     {Emulator32bit::kSysregId_esr, "esr"},
    {Emulator32bit::kSysregId_far, "far"},       {Emulator32bit::kSysregId_vbar, "vbar"},
    {Emulator32bit::kSysregId_usp, "usp"},       {Emulator32bit::kSysregId_ptbr, "ptbr"},
    {Emulator32bit::kSysregId_sctlr, "sctlr"},
};

const char *exception_class_name(const Emulator32bit::ExceptionClass cls)
{
    using Class = Emulator32bit::ExceptionClass;
    switch (cls)
    {
    case Class::UNDEFINED_INSTRUCTION:
        return "undefined instruction";
    case Class::SUPERVISOR_CALL:
        return "supervisor call";
    case Class::INSTRUCTION_ABORT:
        return "instruction abort";
    case Class::DATA_ABORT:
        return "data abort";
    case Class::BREAKPOINT:
        return "breakpoint";
    case Class::IRQ:
        return "irq";
    }
    return "?";
}

} // namespace

const char *Emulator32bit::sysreg_name(const U8 id)
{
    for (const SysregName &sysreg : kSysregNames)
    {
        if (sysreg.id == id)
        {
            return sysreg.name;
        }
    }
    return nullptr;
}

std::optional<U8> Emulator32bit::sysreg_id(const std::string &name)
{
    std::string lower = name;
    for (char &c : lower)
    {
        c = char(std::tolower(static_cast<unsigned char>(c)));
    }
    for (const SysregName &sysreg : kSysregNames)
    {
        if (lower == sysreg.name)
        {
            return sysreg.id;
        }
    }
    return std::nullopt;
}

word Emulator32bit::read_sysreg(const U8 id) const
{
    switch (id)
    {
    case 0:
        return 0;
    case kSysregId_pstate:
        return m_pstate;
    case kSysregId_elr:
        return m_elr;
    case kSysregId_spsr:
        return m_spsr;
    case kSysregId_esr:
        return m_esr;
    case kSysregId_far:
        return m_far;
    case kSysregId_vbar:
        return m_vbar;
    case kSysregId_usp:
        return m_sp_other;
    case kSysregId_ptbr:
        return m_ptbr;
    case kSysregId_sctlr:
        return m_sctlr;
    default:
        throw Exception(InterruptType::BAD_REG,
                        "System register " + std::to_string(id) + " unimplemented.",
                        kUndefinedIss_sysreg);
    }
}

void Emulator32bit::write_sysreg(const U8 id, const word value)
{
    switch (id)
    {
    case 0:
        break;
    case kSysregId_pstate:
        set_user_mode(test_bit(value, kUserModeBit));
        m_pstate = value & kPstateMask;
        break;
    case kSysregId_elr:
        m_elr = value;
        break;
    case kSysregId_spsr:
        m_spsr = value & kPstateMask;
        break;
    case kSysregId_esr:
        m_esr = value;
        break;
    case kSysregId_far:
        m_far = value;
        break;
    case kSysregId_vbar:
        m_vbar = value & ~word(0xF); // the entries are 16 bytes
        break;
    case kSysregId_usp:
        m_sp_other = value;
        break;
    case kSysregId_ptbr:
        m_ptbr = value & ~word(kPageSize - 1); // the first level table is a page
        mmu->set_page_table_base(m_ptbr);
        break;
    case kSysregId_sctlr:
        m_sctlr = value & kSctlrMmuEnable; // the only bit there is
        mmu->set_walk_enabled(test_bit(m_sctlr, 0));
        break;
    default:
        throw Exception(InterruptType::BAD_REG,
                        "System register " + std::to_string(id) + " unimplemented.",
                        kUndefinedIss_sysreg);
    }
}

void Emulator32bit::set_semihosting(const bool enabled)
{
    m_semihosting = enabled;
}

void Emulator32bit::set_brk_stops(const bool stops)
{
    m_brk_stops = stops;
}

void Emulator32bit::set_user_mode(const bool user)
{
    if (user != user_mode())
    {
        std::swap(m_x[register_to_U8(Register::SP)], m_sp_other);
    }
    m_pstate = set_bit(m_pstate, kUserModeBit, user);
    mmu->set_user_mode(user);
}

void Emulator32bit::require_kernel()
{
    if (UNLIKELY(user_mode()))
    {
        throw Exception(InterruptType::BAD_INSTR, "Privileged instruction in user mode.",
                        kUndefinedIss_privileged);
    }
}

void Emulator32bit::enter_exception(const ExceptionClass cls, const word iss, const word far,
                                    const word elr)
{
    if (!m_retired_since_entry)
    {
        throw Exception(InterruptType::DOUBLE_FAULT,
                        std::format("Double fault: {} raised at {:#010x} before any instruction of "
                                    "the handler ran",
                                    exception_class_name(cls), m_pc));
    }
    m_retired_since_entry = false;

    m_elr = elr;
    m_spsr = m_pstate;
    m_esr = (word(cls) << 26) | (iss & 0x3FFFFFF);
    if (cls == ExceptionClass::INSTRUCTION_ABORT || cls == ExceptionClass::DATA_ABORT)
    {
        m_far = far;
    }

    set_user_mode(false);
    m_pstate = set_bit(m_pstate, kIrqMaskBit, 1);
    m_pc = m_vbar + 16 * word(cls);
    m_pc_written = true;

    if (m_trace != nullptr)
    {
        *m_trace << std::format("-- exception: {}, ESR={:#x}, ELR={:#x}, FAR={:#x} -> {:#x}\n",
                                exception_class_name(cls), m_esr, m_elr, m_far, m_pc);
    }
}

bool Emulator32bit::deliver_exception(const std::exception &error, const bool fetching,
                                      const word instr)
{
    if (m_vbar == 0)
    {
        return false;
    }

    if (const auto *emu = dynamic_cast<const Exception *>(&error))
    {
        if (emu->get_type() != InterruptType::BAD_INSTR
            && emu->get_type() != InterruptType::BAD_REG)
        {
            return false; // halt, a failed assertion, brk with a debugger, a double fault
        }

        if (fetching)
        {
            // Only a pc that is not word aligned fails like this.
            enter_exception(ExceptionClass::INSTRUCTION_ABORT, kAbortIss_alignment, m_pc, m_pc);
        }
        else
        {
            enter_exception(ExceptionClass::UNDEFINED_INSTRUCTION, emu->get_iss(), 0, m_pc);
        }
        return true;
    }

    const ExceptionClass abort_class =
        fetching ? ExceptionClass::INSTRUCTION_ABORT : ExceptionClass::DATA_ABORT;

    if (const auto *fault = dynamic_cast<const VirtualMemory::PageFaultException *>(&error))
    {
        using Reason = VirtualMemory::PageFaultException::Reason;
        word iss =
            fault->get_reason() == Reason::UNMAPPED ? kAbortIss_translation : kAbortIss_permission;
        if (!fetching && fault->get_access() == VirtualMemory::AccessType::WRITE)
        {
            iss |= kAbortIss_write;
        }

        // The page that faulted is the one of the access, or the next one if the access crosses
        // into it, which starts at the page boundary.
        const word page_start = fault->get_vpage() << kNumPageOffsetBits;
        word far = m_pc;
        if (!fetching)
        {
            const word address = data_address_of(instr);
            far = (address >> kNumPageOffsetBits) == fault->get_vpage() ? address : page_start;
        }
        enter_exception(abort_class, iss, far, m_pc);
        return true;
    }

    if (dynamic_cast<const SystemBus::Exception *>(&error) != nullptr)
    {
        enter_exception(abort_class, kAbortIss_bus, fetching ? m_pc : data_address_of(instr), m_pc);
        return true;
    }

    return false;
}

void Emulator32bit::reset()
{
    system_bus->reset();
    static_assert(sizeof(m_x) / sizeof(m_x[0]) == kNumReg);
    for (word &reg : m_x)
    {
        reg = 0;
    }
    // Kernel mode with IRQs masked, and no vector table.
    m_pstate = word(1) << kIrqMaskBit;
    m_pc = 0;

    m_sp_other = 0;
    m_elr = m_spsr = m_esr = m_far = m_vbar = m_ptbr = m_sctlr = 0;
    mmu->set_user_mode(false);
    mmu->set_page_table_base(0);
    mmu->set_walk_enabled(false);
    m_pc_written = false;
    m_retired_since_entry = true;
}