#include "emulator32bit/devices.h"

#include <iostream>

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

bool Timer::fast_forward()
{
    if (m_ctrl & kEnabled)
    {
        m_count = m_compare;
        fire();
    }
    return m_intc.has_pending();
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
