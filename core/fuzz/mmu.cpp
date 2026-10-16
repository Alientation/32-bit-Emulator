// Fuzz target of the MMU: the input is a list of operations on the virtual memory (processes,
// mappings, permissions, page table entries, TLBI, the mode) and of loads, stores and fetches
// through the MemoryPort. A small model knows what each access has to give: its value, or the
// fault and the page. The first byte picks the swapping memory of the processes (docs: the
// VirtualMemory class) or the page tables in memory (docs/mmu.md). A difference from the model, a
// failed check_consistency, a crash or a sanitizer report is a bug.
//
// The model of the swapping memory is the bytes of each virtual page of each process: where a page
// is (a frame, the disk) does not show, so eviction, the clock and the TLB are all tested by the
// contents staying put. The model of the page tables is the tables (with the accessed and dirty bits
// the hardware sets) and the data pages; a change of an entry is followed by the TLBI that
// docs/mmu.md asks for, and not when it allows none (a page that only gains permissions).

#include "emulator32bit/emulator32bit.h"
#include "util/logger.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <format>
#include <iostream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

namespace
{

using Reason = VirtualMemory::PageFault::Reason;
using Access = VirtualMemory::AccessType;

/// The bytes of the input, read one at a time; 0 once they are used up.
class Input
{
  public:
    Input(const uint8_t *data, const size_t size) :
        m_data(data),
        m_size(size)
    {
    }

    bool more() const
    {
        return m_at < m_size;
    }

    U8 next()
    {
        return m_at < m_size ? m_data[m_at++] : 0;
    }

    word next_word()
    {
        word value = 0;
        for (size_t i = 0; i < sizeof(word); i++) value |= word(next()) << (8 * i);
        return value;
    }

  private:
    const uint8_t *m_data;
    size_t m_size;
    size_t m_at = 0;
};

/// What an operation ended in.
enum class End : U8
{
    OK,
    FAULT,
    BUS_ERROR,
    INVALID_PID,
    INVALID_VPAGE,
    ALREADY_MAPPED,
};

/// The end of an access: the value, or the fault and its page.
struct Result
{
    End end = End::OK;
    Reason reason = Reason::UNMAPPED;
    word vpage = 0;
    word value = 0;

    bool operator==(const Result &other) const = default;
};

std::string describe(const Result &result)
{
    switch (result.end)
    {
    case End::OK:
        return std::format("ok, {:#x}", result.value);
    case End::FAULT:
        return std::format("fault {} on page {:#x}", U8(result.reason), result.vpage);
    case End::BUS_ERROR:
        return "bus error";
    case End::INVALID_PID:
        return "invalid pid";
    case End::INVALID_VPAGE:
        return "invalid vpage";
    case End::ALREADY_MAPPED:
        return "already mapped";
    }
    return "?";
}

[[noreturn]] void fail(const std::string &what)
{
    std::cerr << "mmu: " << what << "\n";
    std::abort();
}

void require_result(const Result &expected, const Result &actual, const std::string &what)
{
    if (expected == actual) return;
    fail(std::format("{}: the model says {}, the MMU {}", what, describe(expected),
                     describe(actual)));
}

/// How an access is made through the MemoryPort.
enum class Kind : U8
{
    READ,
    WRITE,
    FETCH,
};

/// An access, picked by the input.
struct Request
{
    Kind kind = Kind::READ;
    word address = 0;
    U8 size = 1;
    word value = 0;
};

/// Offsets in a page that an access starts at: the start, inside, and the last bytes, where an
/// access crosses into the next page.
constexpr word kOffsets[] = {
    0, 1, 2, 4, 0x7FC, kPageSize - 4, kPageSize - 3, kPageSize - 2, kPageSize - 1};
constexpr U8 kSizes[] = {1, 2, 4};

Request next_request(Input &in, const word vpage)
{
    Request request;
    const U8 op = in.next();
    request.kind = Kind(op % 3);
    request.size = request.kind == Kind::FETCH ? sizeof(word) : kSizes[(op / 3) % 3];
    request.address = (vpage << kNumPageOffsetBits) + kOffsets[in.next() % std::size(kOffsets)];
    if (request.kind == Kind::FETCH) request.address &= ~word(sizeof(word) - 1);
    if (request.kind == Kind::WRITE) request.value = in.next_word();
    return request;
}

/// Makes the access on the machine.
Result access(Emulator32bit &emu, const Request &request)
{
    Result result;
    try
    {
        switch (request.kind)
        {
        case Kind::READ:
            result.value = request.size == 1   ? word(emu.memory.read<byte>(request.address))
                           : request.size == 2 ? word(emu.memory.read<hword>(request.address))
                                               : emu.memory.read<word>(request.address);
            break;
        case Kind::WRITE:
            if (request.size == 1) emu.memory.write<byte>(request.address, byte(request.value));
            else if (request.size == 2)
                emu.memory.write<hword>(request.address, hword(request.value));
            else emu.memory.write<word>(request.address, request.value);
            break;
        case Kind::FETCH:
            result.value = emu.memory.fetch_instruction(request.address);
            break;
        }
    }
    catch (const VirtualMemory::PageFault &fault)
    {
        result.end = End::FAULT;
        result.reason = fault.get_reason();
        result.vpage = fault.get_vpage();
    }
    catch (const SystemBus::BusError &)
    {
        result.end = End::BUS_ERROR;
    }
    return result;
}

std::string describe(const Request &request)
{
    return std::format("{} of {} at {:#x}",
                       request.kind == Kind::READ    ? "read"
                       : request.kind == Kind::WRITE ? "write"
                                                     : "fetch",
                       request.size, request.address);
}

/// What a read or a fetch at a physical address gives: the RAM or the ROM, or a bus error.
Result expect_physical(Emulator32bit &emu, const Request &request)
{
    SystemBus &bus = *emu.system_bus;
    if (request.kind == Kind::FETCH)
    {
        if (bus.ram->in_bounds(request.address) || bus.rom->in_bounds(request.address))
        {
            return {.value = bus.find_memory(request.address)->read_word(request.address)};
        }
        return {.end = End::BUS_ERROR};
    }
    word value = 0;
    for (U8 i = request.size; i-- > 0;)
    {
        BaseMemory *memory = bus.find_memory(request.address + i);
        if (memory == nullptr) return {.end = End::BUS_ERROR};
        value = (value << 8) | memory->read_byte(request.address + i);
    }
    return {.value = value};
}

/// The bytes of an access, from its address on, that fall in each page: the page of byte i.
word page_of(const Request &request, const U8 i)
{
    return (request.address + i) >> kNumPageOffsetBits;
}

/// The value that the bytes make, little endian.
word value_of(const byte *bytes, const U8 size)
{
    word value = 0;
    for (U8 i = 0; i < size; i++) value |= word(bytes[i]) << (8 * i);
    return value;
}

// ---------------------------------------------------------------------------------------------
// The swapping memory of the processes.

/// The machine: few frames, so that pages are swapped all the time, and a ROM to map pages to.
constexpr word kSwapRamPages = 4;
constexpr word kSwapRomPages = 8;
constexpr word kMaxLiveProcesses = 4;

/// The virtual pages that operations pick: neighbours (an access crosses from one to the next) and
/// pages that share an entry of the TLB (4096 apart).
constexpr word kSwapVpages[] = {0, 1, 2, 3, 4, kMaxTLBSize, kMaxTLBSize + 1, 2 * kMaxTLBSize};

/// A pid that is never a process.
constexpr S64 kNoSuchPid = kMaxProcesses + 7;

class SwapModel
{
  public:
    SwapModel() :
        m_emu(std::make_unique<RAM>(kSwapRamPages, 0),
              std::make_unique<ROM>(kSwapRomPages, kSwapRamPages), std::make_unique<MockDisk>())
    {
    }

    void run(Input &in)
    {
        while (in.more())
        {
            step(in);
            m_emu.mmu->check_consistency();
        }
    }

  private:
    struct Page
    {
        bool write = false;
        bool execute = false;
        bool forced = false; /// Mapped to `ppage` by ensure_physical_page_mapping.
        word ppage = 0;
        std::vector<byte> bytes = std::vector<byte>(kPageSize);
    };

    struct Process
    {
        S64 pid = 0;
        bool kernel = false;
        std::map<word, Page> pages;
    };

    void step(Input &in)
    {
        const U8 op = in.next();
        switch (op % 10)
        {
        case 0:
            begin_process(in.next() & 1);
            break;
        case 1:
            end_process(pick_pid(in));
            break;
        case 2:
            set_process(pick_pid(in));
            break;
        case 3:
        case 4:
        {
            const S64 pid = pick_pid(in);
            const word vpage = pick_vpage(in);
            const U8 bits = in.next();
            const word length = 1 + (bits >> 2) % 3;
            if (op % 10 == 3) add_vpage(pid, vpage, length, bits & 1, bits & 2);
            else set_vpage_permissions(pid, vpage, length, bits & 1, bits & 2);
            break;
        }
        case 5:
        {
            const S64 pid = pick_pid(in);
            const word vpage = pick_vpage(in);
            ensure_mapping(pid, vpage, in.next() % (kSwapRamPages + kSwapRomPages));
            break;
        }
        case 6:
            m_enabled = in.next() & 1;
            m_emu.mmu->set_enabled(m_enabled);
            break;
        case 7:
        {
            // Only the ROM: which frame a page that is not mapped to a page is in cannot be known.
            const U8 bits = in.next();
            const word first = kSwapRamPages + bits % kSwapRomPages;
            const word last =
                (bits & 8) && first + 1 < kSwapRamPages + kSwapRomPages ? first + 1 : first;
            const bool kernel_locked = bits & 0x20;
            m_emu.mmu->set_ppage_permissions(first, last, bits & 0x10, kernel_locked);
            for (word ppage = first; ppage <= last; ppage++) m_kernel_locked[ppage] = kernel_locked;
            break;
        }
        default:
            check_access(next_request(in, pick_vpage(in)));
            break;
        }
    }

    /// A live process, the current one, or one that is not a process.
    S64 pick_pid(Input &in)
    {
        const U8 pick = in.next();
        if (pick == 0xFF || m_processes.empty()) return kNoSuchPid;
        if (pick == 0xFE && m_current != nullptr) return m_current->pid;
        std::map<S64, Process>::const_iterator it = m_processes.begin();
        std::advance(it, pick % m_processes.size());
        return it->first;
    }

    word pick_vpage(Input &in)
    {
        return kSwapVpages[in.next() % std::size(kSwapVpages)];
    }

    /// @return The process with the pid, or null.
    Process *find(const S64 pid)
    {
        const std::map<S64, Process>::iterator it = m_processes.find(pid);
        return it == m_processes.end() ? nullptr : &it->second;
    }

    void begin_process(const bool kernel)
    {
        if (m_processes.size() >= kMaxLiveProcesses) return;
        const S64 pid = m_emu.mmu->begin_process(kernel);
        if (m_processes.contains(pid)) fail(std::format("begin_process gave {} again", pid));
        m_processes[pid] = {.pid = pid, .kernel = kernel, .pages = {}};
        m_current = &m_processes[pid];
    }

    void end_process(const S64 pid)
    {
        Result actual;
        try
        {
            m_emu.mmu->end_process(pid);
        }
        catch (const VirtualMemory::InvalidPid &)
        {
            actual.end = End::INVALID_PID;
        }
        Result expected;
        if (find(pid) == nullptr) expected.end = End::INVALID_PID;
        else
        {
            if (m_current == find(pid)) m_current = nullptr;
            m_processes.erase(pid);
        }
        require_result(expected, actual, std::format("end_process ({})", pid));
    }

    void set_process(const S64 pid)
    {
        Result actual;
        try
        {
            m_emu.mmu->set_process(pid);
        }
        catch (const VirtualMemory::InvalidPid &)
        {
            actual.end = End::INVALID_PID;
        }
        Result expected;
        if (find(pid) == nullptr) expected.end = End::INVALID_PID;
        else m_current = find(pid);
        require_result(expected, actual, std::format("set_process ({})", pid));
    }

    void add_vpage(const S64 pid, const word vpage, const word length, const bool write,
                   const bool execute)
    {
        Result actual;
        try
        {
            m_emu.mmu->add_vpage(pid, vpage, length, write, execute);
        }
        catch (const VirtualMemory::InvalidPid &)
        {
            actual.end = End::INVALID_PID;
        }
        catch (const VirtualMemory::InvalidVPage &)
        {
            actual.end = End::INVALID_VPAGE;
        }

        // The pages before one that is there already are added.
        Result expected;
        Process *process = find(pid);
        if (process == nullptr) expected.end = End::INVALID_PID;
        for (word i = 0; process != nullptr && i < length; i++)
        {
            if (process->pages.contains(vpage + i))
            {
                expected.end = End::INVALID_VPAGE;
                break;
            }
            process->pages[vpage + i] = {.write = write, .execute = execute};
        }
        require_result(expected, actual,
                       std::format("add_vpage ({}, {:#x}, {})", pid, vpage, length));
    }

    void set_vpage_permissions(const S64 pid, const word vpage, const word length, const bool write,
                               const bool execute)
    {
        Result actual;
        try
        {
            m_emu.mmu->set_vpage_permissions(pid, vpage, vpage + length - 1, write, execute);
        }
        catch (const VirtualMemory::InvalidPid &)
        {
            actual.end = End::INVALID_PID;
        }

        Result expected;
        Process *process = find(pid);
        if (process == nullptr) expected.end = End::INVALID_PID;
        for (word i = 0; process != nullptr && i < length; i++)
        {
            // A page that is not there is added.
            Page &page = process->pages[vpage + i];
            page.write = write;
            page.execute = execute;
        }
        require_result(expected, actual,
                       std::format("set_vpage_permissions ({}, {:#x}, {})", pid, vpage, length));
    }

    void ensure_mapping(const S64 pid, const word vpage, const word ppage)
    {
        Result actual;
        try
        {
            m_emu.mmu->ensure_physical_page_mapping(pid, vpage, ppage);
        }
        catch (const VirtualMemory::InvalidPid &)
        {
            actual.end = End::INVALID_PID;
        }
        catch (const VirtualMemory::VPageAlreadyMapped &)
        {
            actual.end = End::ALREADY_MAPPED;
        }

        Result expected;
        Process *process = find(pid);
        if (!m_enabled) expected.end = End::OK;
        else if (process == nullptr) expected.end = End::INVALID_PID;
        else if (!process->pages.contains(vpage))
        {
            process->pages[vpage] = {
                .write = true, .execute = true, .forced = true, .ppage = ppage};
        }
        else
        {
            const Page &page = process->pages[vpage];
            if (!page.forced)
            {
                // A page that is in the frame already is taken as mapped to it, which the model
                // cannot know. Either way nothing changes.
                return;
            }
            if (page.ppage != ppage) expected.end = End::ALREADY_MAPPED;
        }
        require_result(
            expected, actual,
            std::format("ensure_physical_page_mapping ({}, {:#x}, {})", pid, vpage, ppage));
    }

    /// The fault of a page for an access of the kind, or OK.
    Result page_fault(const word vpage, const Access what)
    {
        Result fault;
        fault.end = End::FAULT;
        fault.vpage = vpage;
        const std::map<word, Page>::const_iterator it = m_current->pages.find(vpage);
        if (it == m_current->pages.end())
        {
            fault.reason = Reason::UNMAPPED;
            return fault;
        }
        const Page &page = it->second;
        if (what == Access::WRITE && !page.write)
        {
            fault.reason = Reason::WRITE_DENIED;
            return fault;
        }
        if (page.forced && m_kernel_locked[page.ppage] && !m_current->kernel)
        {
            fault.reason = Reason::KERNEL_ONLY;
            return fault;
        }
        if (what == Access::EXECUTE && !page.execute)
        {
            fault.reason = Reason::EXECUTE_DENIED;
            return fault;
        }
        return {};
    }

    bool on_rom(const word vpage)
    {
        const Page &page = m_current->pages.at(vpage);
        return page.forced && page.ppage >= kSwapRamPages;
    }

    Result expect(const Request &request)
    {
        byte bytes[sizeof(word)] = {};
        switch (request.kind)
        {
        case Kind::READ:
            // The bytes are translated from the last one down.
            for (U8 i = request.size; i-- > 0;)
            {
                const Result fault = page_fault(page_of(request, i), Access::READ);
                if (fault.end != End::OK) return fault;
                const word offset = (request.address + i) & (kPageSize - 1);
                bytes[i] = m_current->pages.at(page_of(request, i)).bytes[offset];
            }
            return {.value = value_of(bytes, request.size)};
        case Kind::WRITE:
            // All are translated, then all are routed (the ROM is refused), then they are stored.
            for (U8 i = 0; i < request.size; i++)
            {
                const Result fault = page_fault(page_of(request, i), Access::WRITE);
                if (fault.end != End::OK) return fault;
            }
            for (U8 i = 0; i < request.size; i++)
            {
                if (on_rom(page_of(request, i))) return {.end = End::BUS_ERROR};
            }
            for (U8 i = 0; i < request.size; i++)
            {
                const word offset = (request.address + i) & (kPageSize - 1);
                m_current->pages.at(page_of(request, i)).bytes[offset] =
                    byte(request.value >> (8 * i));
            }
            return {};
        case Kind::FETCH:
        {
            // The page is translated (as a read), then looked at for execute.
            const word vpage = page_of(request, 0);
            Result fault = page_fault(vpage, Access::READ);
            if (fault.end == End::OK) fault = page_fault(vpage, Access::EXECUTE);
            if (fault.end != End::OK) return fault;
            const word offset = request.address & (kPageSize - 1);
            return {.value = value_of(&m_current->pages.at(vpage).bytes[offset], sizeof(word))};
        }
        }
        return {};
    }

    void check_access(const Request &request)
    {
        // Without a process or with the virtual memory off the addresses are physical. A store is
        // not made: the model would not know which virtual page it changed.
        if (m_current == nullptr || !m_enabled)
        {
            if (request.kind == Kind::WRITE) return;
            const Result expected = expect_physical(m_emu, request);
            require_result(expected, access(m_emu, request),
                           describe(request) + " at a physical address");
            return;
        }
        const Result expected = expect(request);
        const Result actual = access(m_emu, request);
        require_result(expected, actual,
                       std::format("{} by process {}", describe(request), m_current->pid));
    }

    Emulator32bit m_emu;
    std::map<S64, Process> m_processes;
    Process *m_current = nullptr;
    bool m_enabled = true;
    bool m_kernel_locked[kSwapRamPages + kSwapRomPages] = {};
};

// ---------------------------------------------------------------------------------------------
// The page tables in memory.

/// The physical pages: the first level table, three second level tables, the data pages of the
/// RAM and of the ROM (read only: no entry gives them W).
constexpr word kWalkRamPages = 16;
constexpr word kWalkRomPages = 8;
constexpr word kL1Page = 0;
constexpr word kL2Pages[] = {1, 2, 3};
constexpr word kNumTablePages = 4;
constexpr word kFirstDataPage = kNumTablePages;
constexpr word kNumDataPages = kWalkRamPages + kWalkRomPages - kFirstDataPage;
constexpr word kEntriesPerTable = kPageSize / sizeof(word);

/// The first level indices that operations pick (0 and 4 share the entries of the TLB of their
/// pages), and the second level ones (1023 is where an access crosses into the next table).
constexpr word kL1Indices[] = {0, 1, 4};
constexpr word kL2Indices[] = {0, 1, 2, 3, kEntriesPerTable - 1};

constexpr word kPteFlags = VirtualMemory::kPteValid | VirtualMemory::kPteWrite
                           | VirtualMemory::kPteExecute | VirtualMemory::kPteUser
                           | VirtualMemory::kPteAccessed | VirtualMemory::kPteDirty;

/// The bits that the operating system may use, which the hardware ignores.
constexpr word kPteFree = 0xC0;

class WalkModel
{
  public:
    WalkModel() :
        m_emu(std::make_unique<RAM>(kWalkRamPages, 0),
              std::make_unique<ROM>(kWalkRomPages, kWalkRamPages), std::make_unique<MockDisk>())
    {
        // First a process of the swapping memory uses the same virtual pages, so that its
        // translations are cached when the walk is turned on, and must not be used.
        const S64 pid = m_emu.mmu->begin_process();
        for (const word l1 : kL1Indices)
        {
            for (const word l2 : kL2Indices)
            {
                const word vpage = (l1 << 10) | l2;
                m_emu.mmu->add_vpage(pid, vpage, 1, true, true);
                m_emu.memory.read<word>(vpage << kNumPageOffsetBits);
                m_emu.memory.fetch_instruction(vpage << kNumPageOffsetBits);
            }
        }

        const std::vector<byte> zeros(kPageSize);
        for (word page = 0; page < kNumTablePages; page++)
        {
            m_emu.system_bus->write_block(page * kPageSize, zeros.data(), kPageSize);
        }
        for (word page = kFirstDataPage; page < kWalkRamPages + kWalkRomPages; page++)
        {
            std::vector<byte> &bytes = m_data[page - kFirstDataPage];
            bytes.resize(kPageSize);
            for (word i = 0; i < kPageSize; i++) bytes[i] = byte((page * 131 + i * 7) ^ (i >> 8));
            m_emu.system_bus->write_block(page * kPageSize, bytes.data(), kPageSize);
        }
        m_emu.mmu->set_page_table_base(kL1Page * kPageSize);
        m_emu.mmu->set_walk_enabled(true);
    }

    void run(Input &in)
    {
        while (in.more())
        {
            step(in);
            check_tables();
        }
        check_data();
    }

  private:
    void step(Input &in)
    {
        const U8 op = in.next();
        switch (op % 10)
        {
        case 0:
        case 1:
            set_entry(in);
            break;
        case 2:
        {
            // A first level entry: any page below it may change, so all are forgotten.
            const word index = kL1Indices[in.next() % std::size(kL1Indices)];
            const U8 bits = in.next();
            const word entry = (kL2Pages[bits % std::size(kL2Pages)] * kPageSize)
                               | ((bits & 0x80) ? VirtualMemory::kPteValid : 0);
            write_table(kL1Page, index, entry);
            m_emu.mmu->invalidate_translations();
            break;
        }
        case 3:
            m_user = in.next() & 1;
            m_emu.mmu->set_user_mode(m_user);
            break;
        case 4:
            if (in.next() & 1) m_emu.mmu->invalidate_translations();
            else m_emu.mmu->invalidate_translation(pick_vpage(in) << kNumPageOffsetBits);
            break;
        case 5:
            if (in.next() & 1) m_emu.mmu->set_page_table_base(kL1Page * kPageSize);
            else
            {
                m_emu.mmu->set_walk_enabled(false);
                m_emu.mmu->set_walk_enabled(true);
            }
            break;
        default:
            check_access(next_request(in, pick_vpage(in)));
            break;
        }
    }

    word pick_vpage(Input &in)
    {
        const U8 pick = in.next();
        return (kL1Indices[pick % std::size(kL1Indices)] << 10)
               | kL2Indices[(pick / std::size(kL1Indices)) % std::size(kL2Indices)];
    }

    word read_table(const word page, const word index) const
    {
        return m_tables[page][index];
    }

    void write_table(const word page, const word index, const word entry)
    {
        m_tables[page][index] = entry;
        if (!m_emu.system_bus->write_physical_word(page * kPageSize + index * sizeof(word), entry))
        {
            fail("a page table is not in memory");
        }
    }

    /// The table and index of the second level entry of a page, or false if the first level
    /// entry is not valid.
    bool locate(word &table, word &index, const word vpage) const
    {
        const word first = read_table(kL1Page, vpage >> 10);
        if (!(first & VirtualMemory::kPteValid)) return false;
        table = first >> kNumPageOffsetBits;
        index = vpage & (kEntriesPerTable - 1);
        return true;
    }

    /// Whether the entry `after` allows all that `before` did, to the same page, so that a cached
    /// translation of `before` is still right: docs/mmu.md needs no TLBI then. A page that was not
    /// valid has no cached translation.
    static bool only_gains(const word before, const word after)
    {
        if (!(before & VirtualMemory::kPteValid)) return true;
        const word kept = VirtualMemory::kPteValid | VirtualMemory::kPteWrite
                          | VirtualMemory::kPteExecute | VirtualMemory::kPteAccessed
                          | VirtualMemory::kPteDirty;
        return (before & VirtualMemory::kPteFrameMask) == (after & VirtualMemory::kPteFrameMask)
               && (before & VirtualMemory::kPteUser) == (after & VirtualMemory::kPteUser)
               && (after & before & kept) == (before & kept);
    }

    void set_entry(Input &in)
    {
        const word vpage = pick_vpage(in);
        word table;
        word index;
        if (!locate(table, index, vpage)) return;

        const word frame = kFirstDataPage + in.next() % kNumDataPages;
        word entry = frame * kPageSize | (in.next() & (kPteFlags | kPteFree));
        if (frame >= kWalkRamPages) entry &= ~VirtualMemory::kPteWrite;

        const word before = read_table(table, index);
        write_table(table, index, entry);
        const U8 tlbi = in.next() % 3;
        if (tlbi == 2) m_emu.mmu->invalidate_translations();
        else if (tlbi == 1 || !only_gains(before, entry))
        {
            // Every page whose entry this is: two first level entries can share a table.
            for (const word l1 : kL1Indices)
            {
                word other_table;
                word other_index;
                const word other = (l1 << 10) | index;
                if (locate(other_table, other_index, other) && other_table == table)
                {
                    m_emu.mmu->invalidate_translation(other << kNumPageOffsetBits);
                }
            }
        }
    }

    /// Translates a page as the walk does, marking the entry; the fault if it may not.
    Result translate(word &ppage, const word vpage, const Access what)
    {
        Result fault;
        fault.end = End::FAULT;
        fault.vpage = vpage;
        word table;
        word index;
        if (!locate(table, index, vpage))
        {
            fault.reason = Reason::UNMAPPED;
            return fault;
        }
        const word entry = read_table(table, index);
        if (!(entry & VirtualMemory::kPteValid))
        {
            fault.reason = Reason::UNMAPPED;
            return fault;
        }
        if (m_user && !(entry & VirtualMemory::kPteUser))
        {
            fault.reason = Reason::KERNEL_ONLY;
            return fault;
        }
        if (what == Access::WRITE && !(entry & VirtualMemory::kPteWrite))
        {
            fault.reason = Reason::WRITE_DENIED;
            return fault;
        }
        if (what == Access::EXECUTE
            && (!(entry & VirtualMemory::kPteExecute)
                || (!m_user && (entry & VirtualMemory::kPteUser))))
        {
            fault.reason = Reason::EXECUTE_DENIED;
            return fault;
        }
        m_tables[table][index] = entry | VirtualMemory::kPteAccessed
                                 | (what == Access::WRITE ? VirtualMemory::kPteDirty : 0);
        ppage = entry >> kNumPageOffsetBits;
        return {};
    }

    byte &data(const word ppage, const word address)
    {
        return m_data[ppage - kFirstDataPage][address & (kPageSize - 1)];
    }

    Result expect(const Request &request)
    {
        byte bytes[sizeof(word)] = {};
        word ppages[sizeof(word)] = {};
        switch (request.kind)
        {
        case Kind::READ:
            for (U8 i = request.size; i-- > 0;)
            {
                const Result fault = translate(ppages[i], page_of(request, i), Access::READ);
                if (fault.end != End::OK) return fault;
                bytes[i] = data(ppages[i], request.address + i);
            }
            return {.value = value_of(bytes, request.size)};
        case Kind::WRITE:
            for (U8 i = 0; i < request.size; i++)
            {
                const Result fault = translate(ppages[i], page_of(request, i), Access::WRITE);
                if (fault.end != End::OK) return fault;
            }
            for (U8 i = 0; i < request.size; i++)
            {
                data(ppages[i], request.address + i) = byte(request.value >> (8 * i));
            }
            return {};
        case Kind::FETCH:
        {
            const Result fault = translate(ppages[0], page_of(request, 0), Access::EXECUTE);
            if (fault.end != End::OK) return fault;
            return {.value = value_of(&data(ppages[0], request.address), sizeof(word))};
        }
        }
        return {};
    }

    void check_access(const Request &request)
    {
        const Result expected = expect(request);
        const Result actual = access(m_emu, request);
        require_result(expected, actual,
                       std::format("{} in {} mode", describe(request), m_user ? "user" : "kernel"));
    }

    /// The entries, with the accessed and dirty bits that the walk wrote.
    void check_tables()
    {
        for (word page = 0; page < kNumTablePages; page++)
        {
            for (word index = 0; index < kEntriesPerTable; index++)
            {
                word entry = 0;
                m_emu.system_bus->read_physical_word(entry, page * kPageSize + index * 4);
                if (entry != m_tables[page][index])
                {
                    fail(std::format("the entry {} of table page {} is {:#x}, the model has {:#x}",
                                     index, page, entry, m_tables[page][index]));
                }
            }
        }
    }

    void check_data()
    {
        std::vector<byte> bytes(kPageSize);
        for (word page = kFirstDataPage; page < kWalkRamPages; page++)
        {
            m_emu.system_bus->read_page(bytes.data(), page);
            if (bytes != m_data[page - kFirstDataPage])
            {
                fail(std::format("the data page {} differs from the model", page));
            }
        }
    }

    Emulator32bit m_emu;
    word m_tables[kNumTablePages][kEntriesPerTable] = {};
    std::vector<byte> m_data[kNumDataPages];
    bool m_user = false;
};

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    aemu::log::set_level(aemu::log::Level::Off);
    aemu::log::set_fatal_action(aemu::log::FatalAction::Throw);
    if (size == 0) return 0;

    Input in(data + 1, size - 1);
    if (data[0] & 1)
    {
        WalkModel model;
        model.run(in);
    }
    else
    {
        SwapModel model;
        model.run(in);
    }
    return 0;
}
