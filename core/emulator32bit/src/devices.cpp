#include "emulator32bit/devices.h"

#include "emulator32bit/host_input.h"

#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>

Device::Device(const word base_address) :
    BaseMemory(1, base_address >> kNumPageOffsetBits)
{
}

byte Device::read_byte(const word address)
{
    return byte(read_register(address & 0xFFC) >> (8 * (address & 3)));
}

hword Device::read_hword(const word address)
{
    return hword(read_register(address & 0xFFC) >> (8 * (address & 2)));
}

word Device::read_word(const word address)
{
    return read_register(address & 0xFFC);
}

void Device::write_byte(const word address, const byte value)
{
    write_register(address & 0xFFC, word(value) << (8 * (address & 3)));
}

void Device::write_hword(const word address, const hword value)
{
    write_register(address & 0xFFC, word(value) << (8 * (address & 2)));
}

void Device::write_word(const word address, const word value)
{
    write_register(address & 0xFFC, value);
}

// ---------------------------------------------------------------------------------------------
// Interrupt controller

InterruptController::InterruptController() :
    Device(kIntcBase)
{
}

void InterruptController::raise(const word line)
{
    if (line >= kLines || !test_bit(m_enable, line) || test_bit(m_queued, line)) return;

    m_queued = set_bit(m_queued, line, true);
    m_ring[(m_head + m_count) & (kLines - 1)] = U8(line);
    ++m_count;
}

word InterruptController::claim()
{
    if (m_count == 0) return kNoLine;

    const U8 line = m_ring[m_head];
    m_head = U8((m_head + 1) & (kLines - 1));
    --m_count;
    m_queued = set_bit(m_queued, line, false);
    return line;
}

void InterruptController::reset()
{
    m_count = 0;
    m_queued = 0;
    m_enable = 0;
}

word InterruptController::read_register(const word offset)
{
    switch (offset)
    {
    case kRegClaim:
        return claim();
    case kRegCount:
        return m_count;
    case kRegEnable:
        return m_enable;
    case kRegPending:
        return m_queued;
    default:
        return 0;
    }
}

void InterruptController::write_register(const word offset, const word value)
{
    switch (offset)
    {
    case kRegEnable:
        m_enable = value;
        break;
    case kRegRaise:
        raise(value);
        break;
    default:
        break;
    }
}

// ---------------------------------------------------------------------------------------------
// Timer

Timer::Timer(InterruptController &intc) :
    Device(kTimerBase),
    m_intc(intc)
{
}

void Timer::fire()
{
    m_intc.raise(kIrqLineTimer);
    if ((m_ctrl & kPeriodic) && m_interval != 0)
    {
        m_compare += m_interval;
    }
    else
    {
        m_ctrl &= ~kEnabled;
    }
}

word Timer::cycles_until_event() const
{
    // A compare equal to the count only matches after the count wraps, which is not waited for.
    if ((m_ctrl & kEnabled) && m_compare != m_count) return m_compare - m_count;
    return word(-1);
}

void Timer::advance(const word count)
{
    m_count += count;
    if ((m_ctrl & kEnabled) && m_count == m_compare) fire();
}

void Timer::reset()
{
    m_count = m_compare = m_ctrl = m_interval = 0;
}

word Timer::read_register(const word offset)
{
    switch (offset)
    {
    case kRegCount:
        return m_count;
    case kRegCompare:
        return m_compare;
    case kRegCtrl:
        return m_ctrl;
    case kRegInterval:
        return m_interval;
    default:
        return 0;
    }
}

void Timer::write_register(const word offset, const word value)
{
    switch (offset)
    {
    case kRegCount:
        m_count = value;
        break;
    case kRegCompare:
        m_compare = value;
        break;
    case kRegCtrl:
        m_ctrl = value & (kEnabled | kPeriodic);
        break;
    case kRegInterval:
        m_interval = value;
        break;
    default:
        break;
    }
}

// ---------------------------------------------------------------------------------------------
// Console

Console::Console(InterruptController &intc) :
    Device(kConsoleBase),
    m_intc(intc),
    m_out(&std::cout)
{
}

void Console::set_output(std::ostream *out)
{
    m_out = out;
}

void Console::push_input(const std::string &bytes)
{
    for (const char c : bytes)
    {
        m_input.push_back(byte(c));
        if (m_ctrl & kCtrlRxInterrupt) m_intc.raise(kIrqLineConsole);
    }
}

void Console::attach_host_input(std::shared_ptr<HostInput> input)
{
    m_host = std::move(input);
}

void Console::poll_host_input()
{
    if (m_host == nullptr || !m_host->pending()) return;

    const size_t before = m_input.size();
    m_host->take(m_input);

    if (m_input.size() != before && (m_ctrl & kCtrlRxInterrupt)) m_intc.raise(kIrqLineConsole);
}

bool Console::wait_host_input()
{
    if (m_host == nullptr) return false;
    const bool arrived = m_host->wait();
    poll_host_input();
    return arrived;
}

void Console::reset()
{
    m_input.clear();
    m_ctrl = 0;
}

word Console::read_register(const word offset)
{
    if (offset == kRegData || offset == kRegStatus) poll_host_input();

    switch (offset)
    {
    case kRegData:
    {
        if (m_input.empty()) return 0;
        const byte next = m_input.front();
        m_input.pop_front();
        return next;
    }
    case kRegStatus:
        return (m_input.empty() ? 0 : kStatusRxWaiting) | kStatusTxReady;
    case kRegCtrl:
        return m_ctrl;
    default:
        return 0;
    }
}

void Console::write_register(const word offset, const word value)
{
    switch (offset)
    {
    case kRegData:
        if (m_out != nullptr)
        {
            m_out->put(char(value & 0xFF));
            m_out->flush();
        }
        break;
    case kRegCtrl:
        m_ctrl = value & kCtrlRxInterrupt;
        // Input that is already waiting counts as arriving now.
        if ((m_ctrl & kCtrlRxInterrupt) && !m_input.empty()) m_intc.raise(kIrqLineConsole);
        break;
    default:
        break;
    }
}

// ---------------------------------------------------------------------------------------------
// Block device

BlockDevice::BlockDevice(InterruptController &intc) :
    Device(kBlockBase),
    m_intc(intc)
{
}

void BlockDevice::set_capacity(const word sectors)
{
    m_storage.assign(size_t(sectors) * kSectorSize, 0);
    m_path.clear();
}

bool BlockDevice::open_file(const std::string &path, const word sectors)
{
    m_path = path;
    m_storage.clear();

    std::ifstream in(path, std::ios::binary);
    if (in)
    {
        m_storage.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
    else if (std::ifstream(path).good())
    {
        return false; // The file exists, but cannot be read.
    }

    const size_t sectors_size = size_t(sectors) * kSectorSize;
    const size_t whole_sectors_size =
        (m_storage.size() + kSectorSize - 1) / kSectorSize * kSectorSize;
    const size_t wanted = sectors_size > whole_sectors_size ? sectors_size : whole_sectors_size;
    m_storage.resize(wanted, 0);
    return true;
}

bool BlockDevice::save()
{
    if (m_path.empty()) return true;

    std::ofstream out(m_path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char *>(m_storage.data()), std::streamsize(m_storage.size()));
    return bool(out);
}

std::vector<byte> BlockDevice::sector(const word number) const
{
    const size_t start = size_t(number) * kSectorSize;
    if (start + kSectorSize > m_storage.size()) return {};

    return {m_storage.begin() + std::ptrdiff_t(start),
            m_storage.begin() + std::ptrdiff_t(start + kSectorSize)};
}

void BlockDevice::start(const word command)
{
    // The most sectors that a command is timed for, which keeps the time from overflowing. A command
    // for more than that fails.
    constexpr word kMaxTimedSectors = 1 << 16;

    if (m_busy)
    {
        m_status |= kError; // The command is not accepted.
        return;
    }
    if (command < kCmdRead || command > kCmdScatterWrite)
    {
        m_status |= kError;
        return;
    }

    m_status &= ~(kDone | kError);
    m_busy = true;
    m_command = command;
    m_command_sector = m_sector;
    m_command_addr = m_dma_addr;
    m_command_count = m_dma_count;
    // A command takes at least one instruction.
    m_remaining = m_latency != 0 ? m_latency : 1;
    m_segments.clear();
    m_segments_valid = true;
    word sectors = 1;
    if (command == kCmdDmaRead || command == kCmdDmaWrite)
    {
        m_segments.push_back(
            {.sector = m_command_sector, .count = m_command_count, .address = m_command_addr});
        sectors = m_command_count;
    }
    else if (command == kCmdScatterRead || command == kCmdScatterWrite)
    {
        m_segments_valid = read_descriptors(m_dma_list, m_command_count);
        U64 total = 0;
        for (const Segment &segment : m_segments) total += segment.count;
        sectors = total < kMaxTimedSectors ? word(total) : kMaxTimedSectors;
    }
    // Per sector. The count is capped so the time cannot overflow; a count that big fails.
    if (sectors == 0) sectors = 1;
    else if (sectors > kMaxTimedSectors) sectors = kMaxTimedSectors;
    m_remaining *= sectors;
    if (command == kCmdWrite)
    {
        std::memcpy(m_write_data, m_buffer, kSectorSize);
    }
}

bool BlockDevice::segment_is_valid(const Segment &segment) const
{
    const U64 bytes = U64(segment.count) * kSectorSize;
    const U64 disk_start = U64(segment.sector) * kSectorSize;
    return m_dma_memory != nullptr && segment.count != 0 && segment.address % 4 == 0
           && disk_start + bytes <= m_storage.size()
           && U64(segment.address) + bytes <= (U64(1) << 32)
           && m_dma_memory->in_bounds(segment.address)
           && m_dma_memory->in_bounds(word(segment.address + bytes - 1));
}

bool BlockDevice::read_descriptors(const word list, const word count)
{
    // A descriptor is three words: the sector, the number of sectors and the RAM address.
    constexpr word kDescriptorSectorOffset = 0;
    constexpr word kDescriptorCountOffset = 4;
    constexpr word kDescriptorAddressOffset = 8;
    constexpr word kDescriptorSize = 12;
    const U64 bytes = U64(count) * kDescriptorSize;
    if (m_dma_memory == nullptr || count == 0 || count > kMaxDescriptors || list % 4 != 0
        || U64(list) + bytes > (U64(1) << 32) || !m_dma_memory->in_bounds(list)
        || !m_dma_memory->in_bounds(word(list + bytes - 1)))
    {
        return false;
    }
    for (word i = 0; i < count; i++)
    {
        const word at = list + i * kDescriptorSize;
        m_segments.push_back({.sector = m_dma_memory->read_word(at + kDescriptorSectorOffset),
                              .count = m_dma_memory->read_word(at + kDescriptorCountOffset),
                              .address = m_dma_memory->read_word(at + kDescriptorAddressOffset)});
    }
    return true;
}

bool BlockDevice::transfer(const bool to_disk)
{
    if (!m_segments_valid || m_segments.empty()) return false;

    for (const Segment &segment : m_segments)
    {
        if (!segment_is_valid(segment)) return false;
    }

    // In order, so where two segments overlap the later one wins.
    for (const Segment &segment : m_segments)
    {
        const word bytes = segment.count * kSectorSize;
        byte *disk = m_storage.data() + size_t(segment.sector) * kSectorSize;
        if (to_disk)
        {
            m_dma_memory->read_block(disk, segment.address, bytes);
        }
        else
        {
            m_dma_memory->write_block(segment.address, disk, bytes);
        }
    }
    return true;
}

void BlockDevice::complete()
{
    const size_t start = size_t(m_command_sector) * kSectorSize;
    bool error = false;
    switch (m_command)
    {
    case kCmdRead:
        if (start + kSectorSize > m_storage.size())
        {
            error = true;
            break;
        }
        std::memcpy(m_buffer, m_storage.data() + start, kSectorSize);
        m_cursor = 0;
        break;
    case kCmdWrite:
        if (start + kSectorSize > m_storage.size())
        {
            error = true;
            break;
        }
        std::memcpy(m_storage.data() + start, m_write_data, kSectorSize);
        break;
    case kCmdDmaRead:
    case kCmdDmaWrite:
    case kCmdScatterRead:
    case kCmdScatterWrite:
        error = !transfer(m_command == kCmdDmaWrite || m_command == kCmdScatterWrite);
        break;
    default:
        error = !save();
        break;
    }

    m_busy = false;
    m_remaining = 0;
    m_status |= kDone | (error ? kError : 0);
    if (m_ctrl & kCtrlDoneInterrupt) m_intc.raise(kIrqLineBlock);
}

word BlockDevice::cycles_until_event() const
{
    if (m_busy) return m_remaining;
    return word(-1);
}

void BlockDevice::advance(const word count)
{
    if (m_busy)
    {
        m_remaining = count >= m_remaining ? 0 : m_remaining - count;
        if (m_remaining == 0) complete();
    }
}

void BlockDevice::reset()
{
    m_busy = false;
    m_remaining = 0;
    m_status = 0;
    m_cursor = 0;
    m_sector = 0;
    m_ctrl = 0;
    m_latency = kDefaultLatency;
    m_dma_addr = 0;
    m_dma_count = 0;
    m_dma_list = 0;
    std::memset(m_buffer, 0, kSectorSize);
}

word BlockDevice::read_register(const word offset)
{
    switch (offset)
    {
    case kRegStatus:
        return m_status | (m_busy ? kBusy : 0);
    case kRegSector:
        return m_sector;
    case kRegData:
    {
        if (m_cursor >= kSectorSize) return 0;
        const byte *p = m_buffer + m_cursor;
        m_cursor += 4;
        return word(p[0]) | (word(p[1]) << 8) | (word(p[2]) << 16) | (word(p[3]) << 24);
    }
    case kRegCursor:
        return m_cursor;
    case kRegCapacity:
        return word(m_storage.size() / kSectorSize);
    case kRegCtrl:
        return m_ctrl;
    case kRegLatency:
        return m_latency;
    case kRegDmaAddr:
        return m_dma_addr;
    case kRegDmaCount:
        return m_dma_count;
    case kRegDmaList:
        return m_dma_list;
    default:
        return 0;
    }
}

void BlockDevice::write_register(const word offset, const word value)
{
    switch (offset)
    {
    case kRegCommand:
        start(value);
        break;
    case kRegStatus:
        m_status &= ~(kDone | kError);
        break;
    case kRegSector:
        m_sector = value;
        break;
    case kRegData:
        if (m_cursor < kSectorSize)
        {
            byte *p = m_buffer + m_cursor;
            p[0] = byte(value);
            p[1] = byte(value >> 8);
            p[2] = byte(value >> 16);
            p[3] = byte(value >> 24);
            m_cursor += 4;
        }
        break;
    case kRegCursor:
        m_cursor = value & ~word(3);
        break;
    case kRegCtrl:
        m_ctrl = value & kCtrlDoneInterrupt;
        break;
    case kRegLatency:
        m_latency = value;
        break;
    case kRegDmaAddr:
        m_dma_addr = value;
        break;
    case kRegDmaCount:
        m_dma_count = value;
        break;
    case kRegDmaList:
        m_dma_list = value;
        break;
    default:
        break;
    }
}
