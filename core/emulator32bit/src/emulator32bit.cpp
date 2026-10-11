
#include "emulator32bit/emulator32bit.h"
#include "emulator32bit/disassembler.h"
#include "emulator32bit/fpu.h"

#include "emulator32bit/virtual_memory.h"
#include "util/logger.h"
#include "util/types.h"

#include <cctype>
#include <format>
#include <iostream>
#include <utility>

using namespace encoding;

namespace
{

constexpr word kListedOpcodes[] = {
#define AEMU_OPCODE_VALUE(name, opcode) opcode,
    AEMU_OPCODES(AEMU_OPCODE_VALUE)
#undef AEMU_OPCODE_VALUE
};

// AEMU_OPCODES is the only list of opcodes, so this checks it at compile time.
constexpr bool opcodes_are_valid()
{
    for (size_t i = 0; i < std::size(kListedOpcodes); i++)
    {
        // The opcode does not fit in the 6 bit opcode field.
        if (kListedOpcodes[i] >= kMaxInstructions) return false;

        for (size_t j = 0; j < i; j++)
        {
            // Two instructions with the same opcode.
            if (kListedOpcodes[i] == kListedOpcodes[j]) return false;
        }
    }
    return true;
}

static_assert(opcodes_are_valid(), "AEMU_OPCODES has an opcode that is used twice or is above 63");

} // namespace

std::ostream *Emulator32bit::default_out()
{
    return &std::cout;
}

std::ostream *Emulator32bit::default_err()
{
    return &std::cerr;
}

Emulator32bit::Emulator32bit(const word ram_npages, const word ram_start_page,
                             const byte rom_data[], const word rom_npages,
                             const word rom_start_page) :
    Emulator32bit(std::make_unique<RAM>(ram_npages, ram_start_page),
                  std::make_unique<ROM>(rom_data, rom_npages, rom_start_page),
                  std::make_unique<MockDisk>())
{
}

Emulator32bit::Emulator32bit(std::unique_ptr<RAM> ram, std::unique_ptr<ROM> rom,
                             std::unique_ptr<Disk> disk) :
    system_bus(std::make_unique<SystemBus>(std::move(ram), std::move(rom), std::move(disk))),
    // Paged in pages live in the RAM and are swapped out to the disk. The MMU is made after the bus
    // owns the RAM, ROM and disk, so they are freed with it if making the MMU throws.
    mmu(std::make_unique<VirtualMemory>(*system_bus->disk, system_bus->ram->get_lo_page(),
                                        system_bus->ram->get_mem_pages(), system_bus.get())),
    memory(*system_bus, *mmu)
{
    reset();
}

Emulator32bit::~Emulator32bit() = default;

Emulator32bit::Trap::Trap(const Emulator32bit::TrapType type, const std::string &msg,
                          const word iss) :
    type(type),
    message(msg),
    iss(iss)
{
}

word Emulator32bit::Trap::get_iss() const noexcept
{
    return iss;
}

const char *Emulator32bit::Trap::what() const noexcept
{
    return message.c_str();
}

Emulator32bit::TrapType Emulator32bit::Trap::get_type() const noexcept
{
    return type;
}

[[gnu::always_inline]] inline void Emulator32bit::execute(const word instr)
{
    switch (bitfield_unsigned<26, 6>(instr))
    {
#define AEMU_EXECUTE_CASE(name, opcode)                                                            \
    case _op_##name:                                                                               \
        _##name(instr);                                                                            \
        break;
        AEMU_OPCODES(AEMU_EXECUTE_CASE)
#undef AEMU_EXECUTE_CASE
    default:
        _bad_opcode(instr);
        break;
    }
}

inline void Emulator32bit::record_history(const word instr)
{
    // Once the ring buffer is full, the oldest entry is overwritten.
    m_history.pc[m_history.head] = m_pc;
    m_history.instruction[m_history.head] = instr;
    if (++m_history.head == m_history.size) m_history.head = 0;
    if (m_history.count < m_history.size) m_history.count++;
}

void Emulator32bit::print()
{
    *m_out << "32 bit emulator\nRegisters:\n";
    *m_out << std::format(" pc: {}\n", to_color_hex_str(m_pc));
    for (U8 reg = 0; reg < kNumReg; reg++)
    {
        *m_out << std::format("{:>3}: {}\n", register_name(reg), to_color_hex_str(read_reg(reg)));
    }

    const NZCVFlags flags = get_NZCV();
    *m_out << std::format("\nN={:d} Z={:d} C={:d} V={:d}\n", flags.n, flags.z, flags.c, flags.v);
}

word Emulator32bit::fetch_instruction()
{
    if (UNLIKELY(m_pc % kInstructionSize != 0))
    {
        throw Trap(TrapType::MISALIGNED, "Misaligned program counter " + std::to_string(m_pc));
    }
    return memory.fetch_instruction(m_pc);
}

namespace
{

/// What stops the program is reported through the result, and logged at the level of how unusual
/// it is. Anything that goes wrong in the machine is a fault of the program, whatever part of the
/// machine it is in.
///
/// @param result The result of the run, which becomes a fault.
/// @param what The kind of fault.
/// @param message What happened.
void fault(Emulator32bit::RunResult &result, const char *what, const std::string &message)
{
    AEMU_WARN("{}: {}", what, message);
    result.status = Emulator32bit::RunResult::Status::FAULT;
    result.message = message;
}

/// The physical address of a byte of an access that succeeded, for a watchpoint on physical
/// addresses. A byte that cannot be translated is one that nothing watches.
///
/// @param physical_address Receives the physical address.
/// @param mmu The MMU that translated the access.
/// @param virtual_address The address of the byte.
/// @return Whether there is one.
bool translate_watched_byte(word &physical_address, VirtualMemory &mmu, const word virtual_address)
{
    try
    {
        physical_address = mmu.translate_address(virtual_address);
    }
    catch (const std::exception &)
    {
        return false;
    }
    return true;
}

} // namespace

bool Emulator32bit::register_watch_hit(RunResult &result)
{
    for (unsigned i = 0; i < m_regwatches.count; i++)
    {
        const word now = read_reg(m_regwatches.reg[i]);
        const word before = m_regwatches.last[i];
        // Register value did not change.
        if (now == before) continue;
        m_regwatches.last[i] = now;

        // Register value does not match the watchpoint's conditioned value.
        if (m_regwatches.has_value.test(i) && now != m_regwatches.value[i]) continue;

        result.status = RunResult::Status::BREAKPOINT;
        result.message =
            std::format("Register watch {}: {:#x} -> {:#x}, next instruction at {:#010x}",
                        register_name(m_regwatches.reg[i]), before, now, m_pc);
        return true;
    }
    return false;
}

Emulator32bit::RunResult Emulator32bit::run(const U64 instructions)
{
    RunResult result{
        .status = RunResult::Status::LIMIT_REACHED,
        .instructions_ran = 0,
        .message = "",
    };

    // The debugger may have changed it between runs.
    m_watchpoints.hit_message.clear();
    for (unsigned i = 0; i < m_regwatches.count; i++)
    {
        m_regwatches.last[i] = read_reg(m_regwatches.reg[i]);
    }

    // Whether anything looks at each instruction (the debugger's breakpoints and watches, the
    // trace, the history). They are set between runs, so the loop tests this one local instead of
    // each of them, which are loads from memory.
    const bool hooked = m_regwatches.count != 0 || !m_breakpoints.empty() || m_history.size != 0
                        || m_trace.out != nullptr || m_watchpoints.count != 0
                        || system_bus->console.has_host_input();
    SystemBus &bus = *system_bus;

    try
    {
        // 0 instructions means to run until something stops the program.
        //
        // The pc that a run starts at is not checked against the breakpoints. The debugger goes
        // on from the breakpoint it stopped at (continue, step, the `c` of a gdb client), and if
        // that pc counted the run would stop at once, before any instruction, and never leave. Only
        // a pc that the program arrives at is a breakpoint hit.
        bool leaving_start = true;
        while (instructions == 0 || result.instructions_ran < instructions)
        {
            if (UNLIKELY(hooked))
            {
                // Input from the host raises its interrupt here, between instructions.
                bus.console.poll_host_input();

                if (m_regwatches.count != 0 && register_watch_hit(result)) break;

                if (!leaving_start && !m_breakpoints.empty() && m_breakpoints.contains(m_pc))
                {
                    result.status = RunResult::Status::BREAKPOINT;
                    result.message = std::format("Breakpoint at {:#010x}", m_pc);
                    break;
                }
                leaving_start = false;
            }

            // An interrupt is taken between two instructions, if the program has installed a
            // vector table and unmasked IRQs. The resume address is the instruction that would
            // have executed.
            if (UNLIKELY(bus.intc.has_pending()) && m_vbar != 0 && !test_bit<kIrqMaskBit>(m_pstate))
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

            if (UNLIKELY(hooked) && m_history.size != 0) record_history(instr);

            // The handler of an exception needs the instruction, to work out the address of a data
            // abort. It is kept here and not in `instr`, which would then be live across every
            // handler call, and the handlers use all the registers that a call may change: the
            // compiler kept `instr` in memory for the whole loop, and the dispatch waited for it.
            m_instr_in_flight = instr;
            try
            {
                if (UNLIKELY(hooked)) execute_hooked(instr);
                else execute(instr);
            }
            catch (const std::exception &error)
            {
                if (!deliver_exception(error, false, m_instr_in_flight)) throw;
                m_pc_written = false;
                continue;
            }

            // Whatever wrote the pc (ERET, an exception) wrote the address of the next
            // instruction itself.
            if (LIKELY(!m_pc_written)) m_pc += kInstructionSize;
            m_pc_written = false;
            m_just_took_exception = false;
            result.instructions_ran++;
            bus.timer.tick();
            bus.block.tick();

            if (UNLIKELY(hooked) && !m_watchpoints.hit_message.empty())
            {
                result.status = RunResult::Status::BREAKPOINT;
                result.message = std::move(m_watchpoints.hit_message);
                m_watchpoints.hit_message.clear();
                break;
            }
        }

        // The loop looks at the registers before each instruction, we need to also check again
        // after the last instruction is executed.
        if (result.status == RunResult::Status::LIMIT_REACHED && UNLIKELY(m_regwatches.count != 0))
        {
            register_watch_hit(result);
        }
    }
    catch (const Trap &e)
    {
        if (e.get_type() == TrapType::HALT_INSTR)
        {
            AEMU_DEBUG("Halted after {} instructions.", result.instructions_ran);
            result.status = RunResult::Status::HALTED;
            result.message = e.what();
            // The hlt ran, if it was the first instruction of a handler that is not a double fault.
            m_just_took_exception = false;
        }
        else if (e.get_type() == TrapType::BREAK_INSTR)
        {
            // The brk completed, the debugger goes on with the next instruction.
            m_pc += kInstructionSize;
            m_just_took_exception = false;
            result.instructions_ran++;
            result.status = RunResult::Status::BREAKPOINT;
            result.message = e.what();
        }
        else
        {
            fault(result, "Emulator exception", e.what());
        }
    }
    catch (const SystemBus::BusError &e)
    {
        fault(result, "System bus exception", e.what());
    }
    catch (const VirtualMemory::Error &e)
    {
        // E.g. an access to an unmapped virtual page, or to memory that the page does not allow.
        fault(result, "Virtual memory exception", e.what());
    }
    catch (const FreeBlockList::Error &e)
    {
        // There are no more pages on the disk, or no more process numbers.
        fault(result, "Out of resources", e.what());
    }
    catch (const Disk::ReadError &e)
    {
        fault(result, "Disk read error", e.what());
    }
    catch (const Disk::WriteError &e)
    {
        fault(result, "Disk write error", e.what());
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

unsigned Emulator32bit::find_watchpoint(const word address, const bool physical) const
{
    unsigned i = 0;
    while (i < m_watchpoints.count
           && !(m_watchpoints.address[i] == address && m_watchpoints.physical.test(i) == physical))
    {
        i++;
    }
    return i; // The count, if there is none.
}

bool Emulator32bit::add_watchpoint(const word address, const word length, const WatchKind kind,
                                   const bool physical, const WatchCompare compare,
                                   const word compare_value)
{
    AEMU_CHECK(length != 0, "A watchpoint needs a length of at least 1");
    const unsigned i = find_watchpoint(address, physical);
    if (i == m_watchpoints.count)
    {
        if (m_watchpoints.count == kMaxWatchpoints) return false;
        m_watchpoints.count++;
    }
    m_watchpoints.address[i] = address;
    m_watchpoints.length[i] = length;
    m_watchpoints.kind[i] = kind;
    m_watchpoints.physical.assign(i, physical);
    m_watchpoints.compare[i] = compare;
    m_watchpoints.compare_value[i] = compare_value;
    return true;
}

bool Emulator32bit::remove_watchpoint(const word address, const bool physical)
{
    const unsigned i = find_watchpoint(address, physical);
    if (i == m_watchpoints.count)
    {
        return false;
    }
    const unsigned last = --m_watchpoints.count;
    m_watchpoints.address[i] = m_watchpoints.address[last];
    m_watchpoints.length[i] = m_watchpoints.length[last];
    m_watchpoints.kind[i] = m_watchpoints.kind[last];
    m_watchpoints.physical.copy(last, i);
    m_watchpoints.compare[i] = m_watchpoints.compare[last];
    m_watchpoints.compare_value[i] = m_watchpoints.compare_value[last];
    return true;
}

bool Emulator32bit::watch_compare_holds(const WatchCompare compare, const word value,
                                        const word against)
{
    switch (compare)
    {
    case WatchCompare::NONE:
        return true;
    case WatchCompare::EQ:
        return value == against;
    case WatchCompare::NE:
        return value != against;
    case WatchCompare::LT:
        return value < against;
    case WatchCompare::LE:
        return value <= against;
    case WatchCompare::GT:
        return value > against;
    case WatchCompare::GE:
        return value >= against;
    }
    return false;
}

void Emulator32bit::clear_watchpoints()
{
    m_watchpoints.count = 0;
    m_watchpoints.physical.clear();
}

Emulator32bit::Watchpoint Emulator32bit::watchpoint(const unsigned i) const
{
    AEMU_CHECK(i < m_watchpoints.count, "There is no watchpoint {}", i);
    return {.address = m_watchpoints.address[i],
            .length = m_watchpoints.length[i],
            .kind = m_watchpoints.kind[i],
            .physical = m_watchpoints.physical.test(i),
            .compare = m_watchpoints.compare[i],
            .compare_value = m_watchpoints.compare_value[i]};
}

unsigned Emulator32bit::find_register_watch(const U8 reg) const
{
    unsigned i = 0;
    while (i < m_regwatches.count && m_regwatches.reg[i] != reg) i++;
    return i; // The count, if there is none.
}

bool Emulator32bit::set_register_watch(const U8 reg, const bool has_value, const word value)
{
    AEMU_CHECK(reg <= static_cast<U8>(Register::SP), "Cannot watch register {}", reg);
    const unsigned i = find_register_watch(reg);
    if (i == m_regwatches.count)
    {
        if (m_regwatches.count == kMaxRegisterWatches) return false;
        m_regwatches.count++;
    }
    m_regwatches.reg[i] = reg;
    m_regwatches.has_value.assign(i, has_value);
    m_regwatches.value[i] = value;
    m_regwatches.last[i] = read_reg(reg);
    return true;
}

bool Emulator32bit::add_register_watch(const U8 reg)
{
    return set_register_watch(reg, false, 0);
}

bool Emulator32bit::add_register_watch(const U8 reg, const word value)
{
    return set_register_watch(reg, true, value);
}

bool Emulator32bit::remove_register_watch(const U8 reg)
{
    const unsigned i = find_register_watch(reg);
    if (i == m_regwatches.count) return false;
    const unsigned last = --m_regwatches.count;
    m_regwatches.reg[i] = m_regwatches.reg[last];
    m_regwatches.has_value.copy(last, i);
    m_regwatches.value[i] = m_regwatches.value[last];
    m_regwatches.last[i] = m_regwatches.last[last];
    return true;
}

void Emulator32bit::clear_register_watches()
{
    m_regwatches.count = 0;
    m_regwatches.has_value.clear();
}

Emulator32bit::RegisterWatch Emulator32bit::register_watch(const unsigned i) const
{
    AEMU_CHECK(i < m_regwatches.count, "There is no register watch {}", i);
    return {.reg = m_regwatches.reg[i],
            .has_value = m_regwatches.has_value.test(i),
            .value = m_regwatches.value[i],
            .last = m_regwatches.last[i]};
}

word Emulator32bit::watch_old_value(const word address, const word length)
{
    try
    {
        return length == 1   ? word(memory.read<byte>(address))
               : length == 2 ? word(memory.read<hword>(address))
                             : memory.read<word>(address);
    }
    catch (const std::exception &)
    {
        return 0; // The store faults as well, and nothing is reported for it.
    }
}

void Emulator32bit::watch_access(const word address, const word length, const bool write,
                                 const word value, const word old_value)
{
    // The first hit of the instruction is the one that is reported,
    // do not overwrite it with any subsequent hits.
    if (!m_watchpoints.hit_message.empty()) return;

    const U64 first = address;
    const U64 last = first + length; // One past, 64 bit so it cannot wrap.
    for (unsigned i = 0; i < m_watchpoints.count; i++)
    {
        const WatchKind kind = m_watchpoints.kind[i];
        const bool kind_matches =
            (static_cast<U8>(kind)
             & (write ? static_cast<U8>(WatchKind::WRITE) : static_cast<U8>(WatchKind::READ)))
            != 0;
        if (!kind_matches
            || !watch_compare_holds(m_watchpoints.compare[i], value,
                                    m_watchpoints.compare_value[i]))
        {
            continue;
        }

        const word watched = m_watchpoints.address[i];
        const U64 watched_end = U64(watched) + m_watchpoints.length[i];
        const bool physical = m_watchpoints.physical.test(i);
        bool overlaps = false;
        if (physical)
        {
            for (word k = 0; k < length && !overlaps; k++)
            {
                word target;
                overlaps = translate_watched_byte(target, *mmu, address + k) && target >= watched
                           && U64(target) < watched_end;
            }
        }
        else
        {
            overlaps = first < watched_end && U64(watched) < last;
        }

        if (overlaps)
        {
            m_watchpoints.hit_message =
                std::format("Watchpoint {}{:#010x}: {} of {:#x} ({} byte{}) at {:#010x} by "
                            "the instruction at {:#010x}",
                            physical ? "physical " : "", watched, write ? "write" : "read", value,
                            length, length == 1 ? "" : "s", address, m_pc);
            if (write)
            {
                m_watchpoints.hit_message += std::format(", the old value was {:#x}", old_value);
            }
            m_watchpoints.last_hit = {.address = address, .kind = kind};
            return;
        }
    }
}

void Emulator32bit::set_trace(std::ostream *out)
{
    m_trace.out = out;
}

void Emulator32bit::set_history_size(const size_t count)
{
    AEMU_CHECK(count <= kMaxHistory, "The history keeps at most {} instructions", kMaxHistory);
    // The newest entries that fit, oldest first, are what the new ring starts with.
    History old;
    const size_t old_count = history(old);
    const size_t kept = old_count < count ? old_count : count;

    for (size_t i = 0; i < kept; i++)
    {
        const ExecutedInstruction &entry = old[old_count - kept + i];
        m_history.pc[i] = entry.pc;
        m_history.instruction[i] = entry.instruction;
    }
    m_history.size = count;
    m_history.count = kept;
    m_history.head = kept == count ? 0 : kept;
}

size_t Emulator32bit::history_size() const
{
    return m_history.size;
}

size_t Emulator32bit::history(History &out) const
{
    // The oldest entry is `count` before the head, going around.
    size_t i = m_history.head >= m_history.count
                   ? m_history.head - m_history.count
                   : m_history.head + m_history.size - m_history.count;
    for (size_t n = 0; n < m_history.count; n++)
    {
        out[n] = {.pc = m_history.pc[i], .instruction = m_history.instruction[i]};
        if (++i == m_history.size) i = 0;
    }
    return m_history.count;
}

void Emulator32bit::set_symbols(const SymbolMap *symbols)
{
    m_trace.symbols = symbols;
}

namespace
{

/// "NzCv": an upper case letter is a flag that is set.
std::string flag_letters(const word pstate)
{
    std::string letters;
    letters += test_bit<kNFlagBit>(pstate) ? 'N' : 'n';
    letters += test_bit<kZFlagBit>(pstate) ? 'Z' : 'z';
    letters += test_bit<kCFlagBit>(pstate) ? 'C' : 'c';
    letters += test_bit<kVFlagBit>(pstate) ? 'V' : 'v';
    return letters;
}

} // namespace

void Emulator32bit::execute_hooked(const word instr)
{
    // The old value of what a store replaces is read before the store, from the registers the
    // instruction is going to use.
    if (m_watchpoints.count != 0)
    {
        const U8 opcode = bitfield_unsigned<26, 6>(instr);
        if (opcode == _op_str || opcode == _op_strh || opcode == _op_strb)
        {
            m_watchpoints.old_value =
                watch_old_value(data_address_of(instr), opcode == _op_str    ? 4
                                                        : opcode == _op_strh ? 2
                                                                             : 1);
        }
    }

    if (m_trace.out != nullptr) execute_traced(instr);
    else execute(instr);
}

void Emulator32bit::execute_traced(const word instr)
{
    const word pc = m_pc;
    word regs_before[kNumReg];
    for (U8 r = 0; r < kNumReg; r++) regs_before[r] = m_x[r];
    const word pstate_before = m_pstate;

    std::string line = std::format("{:#010x}", pc);
    if (m_trace.symbols != nullptr)
    {
        const std::string name = m_trace.symbols->describe(pc);
        if (!name.empty()) line += " <" + name + ">";
    }
    line += ": " + disassemble_instr(instr);

    try
    {
        execute(instr);
    }
    catch (const Trap &e)
    {
        *m_trace.out << line << " ; " << (e.get_type() == TrapType::HALT_INSTR ? "halt" : e.what())
                     << "\n";
        throw;
    }
    catch (const std::exception &e)
    {
        *m_trace.out << line << " ; " << e.what() << "\n";
        throw;
    }

    std::string changes;
    for (U8 r = 0; r < kNumReg; r++)
    {
        if (m_x[r] != regs_before[r])
        {
            changes += std::format(" {}={:#x}->{:#x}", register_name(r), regs_before[r], m_x[r]);
        }
    }

    if (m_pstate != pstate_before)
    {
        changes += " NZCV=" + flag_letters(pstate_before) + "->" + flag_letters(m_pstate);
    }

    const word next_pc = m_pc_written ? m_pc : m_pc + kInstructionSize;
    if (next_pc != pc + kInstructionSize) changes += std::format(" pc={:#x}", next_pc);

    *m_trace.out << line << (changes.empty() ? "" : " ;" + changes) << "\n";
}

std::string Emulator32bit::disassemble_instr(const word instr)
{
    return disassembler::disassemble(instr);
}

const char *Emulator32bit::exception_class_name(const ExceptionClass cls)
{
    using Class = ExceptionClass;
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

std::optional<U8> Emulator32bit::sysreg_id(const std::string &name)
{
    std::string lower = name;
    for (char &c : lower) c = char(std::tolower(static_cast<unsigned char>(c)));
    for (const SysregName &sysreg : kSysregNames)
    {
        if (lower == sysreg.name) return sysreg.id;
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
    case kSysregId_fpcr:
        return m_fpcr;
    case kSysregId_fpsr:
        return m_fpsr;
    default:
        throw Trap(TrapType::BAD_REG, "System register " + std::to_string(id) + " unimplemented.",
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
        set_user_mode(test_bit<kUserModeBit>(value));
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
        m_vbar = value & ~(kVectorSize - 1);
        break;
    case kSysregId_usp:
        m_sp_other = value;
        break;
    case kSysregId_ptbr:
        m_ptbr = value & ~word(kPageSize - 1); // The first level table is a page.
        mmu->set_page_table_base(m_ptbr);
        break;
    case kSysregId_sctlr:
        m_sctlr = value & kSctlrMmuEnable; // The only bit there is.
        mmu->set_walk_enabled(test_bit<0>(m_sctlr));
        break;
    case kSysregId_fpcr:
        m_fpcr = value & fpu::kFpcrMask;
        break;
    case kSysregId_fpsr:
        m_fpsr = value & fpu::kFpsrMask;
        break;
    default:
        throw Trap(TrapType::BAD_REG, "System register " + std::to_string(id) + " unimplemented.",
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
        // The stack pointer in use goes to the other mode, and the one of the new mode comes back.
        word &sp = m_x[register_to_U8(Register::SP)];
        const word sp_of_old_mode = sp;
        sp = m_sp_other;
        m_sp_other = sp_of_old_mode;
    }
    m_pstate = set_bit<kUserModeBit>(m_pstate, user);
    mmu->set_user_mode(user);
}

void Emulator32bit::require_kernel()
{
    if (UNLIKELY(user_mode()))
    {
        throw Trap(TrapType::BAD_INSTR, "Privileged instruction in user mode.",
                   kUndefinedIss_privileged);
    }
}

void Emulator32bit::enter_exception(const ExceptionClass cls, const word iss, const word far,
                                    const word elr)
{
    if (m_just_took_exception)
    {
        throw Trap(TrapType::DOUBLE_FAULT,
                   std::format("Double fault: {} raised at {:#010x} before any instruction of "
                               "the handler ran",
                               exception_class_name(cls), m_pc));
    }
    m_just_took_exception = true;

    if (m_exception_depth != kMaxExceptionDepth) m_exception_depth++;
    m_elr = elr;
    m_spsr = m_pstate;
    m_esr = (word(cls) << kEsrClassShift) | (iss & kEsrIssMask);
    if (cls == ExceptionClass::INSTRUCTION_ABORT || cls == ExceptionClass::DATA_ABORT)
    {
        m_far = far;
    }

    set_user_mode(false);
    m_pstate = set_bit<kIrqMaskBit>(m_pstate, 1);
    m_pc = m_vbar + kVectorSize * word(cls);
    m_pc_written = true;

    if (m_trace.out != nullptr)
    {
        *m_trace.out << std::format("-- exception: {}, ESR={:#x}, ELR={:#x}, FAR={:#x} -> {:#x}\n",
                                    exception_class_name(cls), m_esr, m_elr, m_far, m_pc);
    }
}

bool Emulator32bit::deliver_exception(const std::exception &error, const bool fetching,
                                      const word instr)
{
    if (m_vbar == 0) return false;

    if (const auto *emu = dynamic_cast<const Trap *>(&error))
    {
        if (emu->get_type() == TrapType::MISALIGNED)
        {
            // A pc that is not aligned, or a load or a store at such an address.
            if (fetching)
            {
                enter_exception(ExceptionClass::INSTRUCTION_ABORT, kAbortIss_alignment, m_pc, m_pc);
            }
            else
            {
                enter_exception(ExceptionClass::DATA_ABORT, emu->get_iss(), data_address_of(instr),
                                m_pc);
            }
            return true;
        }

        if (emu->get_type() != TrapType::BAD_INSTR && emu->get_type() != TrapType::BAD_REG)
        {
            return false; // Halt, a failed assertion, brk with a debugger, a double fault.
        }

        enter_exception(ExceptionClass::UNDEFINED_INSTRUCTION, emu->get_iss(), 0, m_pc);
        return true;
    }

    const ExceptionClass abort_class =
        fetching ? ExceptionClass::INSTRUCTION_ABORT : ExceptionClass::DATA_ABORT;

    if (const auto *fault = dynamic_cast<const VirtualMemory::PageFault *>(&error))
    {
        using Reason = VirtualMemory::PageFault::Reason;
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

    if (dynamic_cast<const SystemBus::BusError *>(&error) != nullptr)
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
    for (word &reg : m_x) reg = 0;

    // Kernel mode with IRQs masked, and no vector table.
    m_pstate = word(1) << kIrqMaskBit;
    m_pc = 0;

    m_sp_other = 0;
    m_elr = m_spsr = m_esr = m_far = m_vbar = m_ptbr = m_sctlr = 0;
    m_fpcr = m_fpsr = 0;
    mmu->set_user_mode(false);
    mmu->set_page_table_base(0);
    mmu->set_walk_enabled(false);
    m_pc_written = false;
    m_just_took_exception = false;
    m_exception_depth = 0;
}