#include "emulator32bit/devices.h"

#include <algorithm>
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
    if (line >= 32 || !(m_enable & (word(1) << line)) || (m_queued & (word(1) << line)))
    {
        return;
    }
    m_queued |= word(1) << line;
    m_queue.push_back(line);
}

word InterruptController::claim()
{
    if (m_queue.empty())
    {
        return 0xFFFFFFFF;
    }
    const word line = m_queue.front();
    m_queue.pop_front();
    m_queued &= ~(word(1) << line);
    return line;
}

void InterruptController::reset()
{
    m_queue.clear();
    m_queued = 0;
    m_enable = 0;
}

word InterruptController::read_register(const word offset)
{
    switch (offset)
    {
    case 0x00:
        return claim();
    case 0x04:
        return word(m_queue.size());
    case 0x08:
        return m_enable;
    case 0x10:
        return m_queued;
    default:
        return 0;
    }
}

void InterruptController::write_register(const word offset, const word value)
{
    switch (offset)
    {
    case 0x08:
        m_enable = value;
        break;
    case 0x0C:
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

std::optional<word> Timer::cycles_until_event() const
{
    // A compare equal to the count only matches after the count wraps, which is not waited for.
    if ((m_ctrl & kEnabled) && m_compare != m_count)
    {
        return m_compare - m_count;
    }
    return std::nullopt;
}

void Timer::advance(const word count)
{
    m_count += count;
    if ((m_ctrl & kEnabled) && m_count == m_compare)
    {
        fire();
    }
}

void Timer::reset()
{
    m_count = m_compare = m_ctrl = m_interval = 0;
}

word Timer::read_register(const word offset)
{
    switch (offset)
    {
    case 0x00:
        return m_count;
    case 0x04:
        return m_compare;
    case 0x08:
        return m_ctrl;
    case 0x0C:
        return m_interval;
    default:
        return 0;
    }
}

void Timer::write_register(const word offset, const word value)
{
    switch (offset)
    {
    case 0x00:
        m_count = value;
        break;
    case 0x04:
        m_compare = value;
        break;
    case 0x08:
        m_ctrl = value & (kEnabled | kPeriodic);
        break;
    case 0x0C:
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
        if (m_ctrl & 1)
        {
            m_intc.raise(kIrqLineConsole);
        }
    }
}

void Console::reset()
{
    m_input.clear();
    m_ctrl = 0;
}

word Console::read_register(const word offset)
{
    switch (offset)
    {
    case 0x00:
    {
        if (m_input.empty())
        {
            return 0;
        }
        const byte next = m_input.front();
        m_input.pop_front();
        return next;
    }
    case 0x04:
        return (m_input.empty() ? 0 : 1) | 2;
    case 0x08:
        return m_ctrl;
    default:
        return 0;
    }
}

void Console::write_register(const word offset, const word value)
{
    switch (offset)
    {
    case 0x00:
        if (m_out != nullptr)
        {
            m_out->put(char(value & 0xFF));
            m_out->flush();
        }
        break;
    case 0x08:
        m_ctrl = value & 1;
        // Input that is already waiting counts as arriving now.
        if ((m_ctrl & 1) && !m_input.empty())
        {
            m_intc.raise(kIrqLineConsole);
        }
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
        return false; // exists, cannot be read
    }

    const size_t wanted =
        std::max<size_t>(size_t(sectors) * kSectorSize,
                         (m_storage.size() + kSectorSize - 1) / kSectorSize * kSectorSize);
    m_storage.resize(wanted, 0);
    return true;
}

bool BlockDevice::save()
{
    if (m_path.empty())
    {
        return true;
    }
    std::ofstream out(m_path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char *>(m_storage.data()), std::streamsize(m_storage.size()));
    return bool(out);
}

std::vector<byte> BlockDevice::sector(const word number) const
{
    const size_t start = size_t(number) * kSectorSize;
    if (start + kSectorSize > m_storage.size())
    {
        return {};
    }
    return {m_storage.begin() + std::ptrdiff_t(start),
            m_storage.begin() + std::ptrdiff_t(start + kSectorSize)};
}

void BlockDevice::start(const word command)
{
    if (m_busy)
    {
        m_status |= kError; // not accepted
        return;
    }
    if (command < kCmdRead || command > kCmdDmaWrite)
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
    m_remaining = std::max<word>(1, m_latency);
    if (command == kCmdDmaRead || command == kCmdDmaWrite)
    {
        // Per sector. The count is capped here so the time cannot overflow; a count that big fails.
        m_remaining *= std::clamp<word>(m_dma_count, 1, 1 << 16);
    }
    if (command == kCmdWrite)
    {
        std::copy(std::begin(m_buffer), std::end(m_buffer), std::begin(m_write_data));
    }
}

bool BlockDevice::transfer(const bool to_disk)
{
    const U64 bytes = U64(m_command_count) * kSectorSize;
    const U64 disk_start = U64(m_command_sector) * kSectorSize;
    if (m_dma_memory == nullptr || m_command_count == 0 || m_command_addr % 4 != 0
        || disk_start + bytes > m_storage.size() || U64(m_command_addr) + bytes > (U64(1) << 32)
        || !m_dma_memory->in_bounds(m_command_addr)
        || !m_dma_memory->in_bounds(word(m_command_addr + bytes - 1)))
    {
        return false;
    }

    byte *disk = m_storage.data() + disk_start;
    if (to_disk)
    {
        m_dma_memory->read_block(m_command_addr, disk, word(bytes));
    }
    else
    {
        m_dma_memory->write_block(m_command_addr, disk, word(bytes));
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
        std::copy_n(m_storage.begin() + std::ptrdiff_t(start), kSectorSize, std::begin(m_buffer));
        m_cursor = 0;
        break;
    case kCmdWrite:
        if (start + kSectorSize > m_storage.size())
        {
            error = true;
            break;
        }
        std::copy(std::begin(m_write_data), std::end(m_write_data),
                  m_storage.begin() + std::ptrdiff_t(start));
        break;
    case kCmdDmaRead:
    case kCmdDmaWrite:
        error = !transfer(m_command == kCmdDmaWrite);
        break;
    default:
        error = !save();
        break;
    }

    m_busy = false;
    m_remaining = 0;
    m_status |= kDone | (error ? kError : 0);
    if (m_ctrl & 1)
    {
        m_intc.raise(kIrqLineBlock);
    }
}

std::optional<word> BlockDevice::cycles_until_event() const
{
    if (m_busy)
    {
        return m_remaining;
    }
    return std::nullopt;
}

void BlockDevice::advance(const word count)
{
    if (m_busy)
    {
        m_remaining = count >= m_remaining ? 0 : m_remaining - count;
        if (m_remaining == 0)
        {
            complete();
        }
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
    m_latency = 100;
    m_dma_addr = 0;
    m_dma_count = 0;
    std::fill(std::begin(m_buffer), std::end(m_buffer), byte(0));
}

word BlockDevice::read_register(const word offset)
{
    switch (offset)
    {
    case 0x04:
        return m_status | (m_busy ? kBusy : 0);
    case 0x08:
        return m_sector;
    case 0x0C:
    {
        if (m_cursor >= kSectorSize)
        {
            return 0;
        }
        const byte *p = m_buffer + m_cursor;
        m_cursor += 4;
        return word(p[0]) | (word(p[1]) << 8) | (word(p[2]) << 16) | (word(p[3]) << 24);
    }
    case 0x10:
        return m_cursor;
    case 0x14:
        return word(m_storage.size() / kSectorSize);
    case 0x18:
        return m_ctrl;
    case 0x1C:
        return m_latency;
    case 0x20:
        return m_dma_addr;
    case 0x24:
        return m_dma_count;
    default:
        return 0;
    }
}

void BlockDevice::write_register(const word offset, const word value)
{
    switch (offset)
    {
    case 0x00:
        start(value);
        break;
    case 0x04:
        m_status &= ~(kDone | kError);
        break;
    case 0x08:
        m_sector = value;
        break;
    case 0x0C:
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
    case 0x10:
        m_cursor = value & ~word(3);
        break;
    case 0x18:
        m_ctrl = value & 1;
        break;
    case 0x1C:
        m_latency = value;
        break;
    case 0x20:
        m_dma_addr = value;
        break;
    case 0x24:
        m_dma_count = value;
        break;
    default:
        break;
    }
}
