#pragma once

#include "emulator32bit/emulator32bit_util.h"
#include "emulator32bit/memory.h"

#include <cstdint>
#include <deque>
#include <iosfwd>
#include <string>

/// The memory mapped devices (docs/devices.md). Each is one page of registers at a physical address
/// in the device window, and is reached through the system bus like memory. A register is a word;
/// a byte or half-word read gives part of it, a byte or half-word write is a write of the word with
/// the value in that position (so a byte store to the console data register sends that byte).
inline constexpr word kIntcBase = kDeviceBase;
inline constexpr word kTimerBase = kDeviceBase + 0x1000;
inline constexpr word kConsoleBase = kDeviceBase + 0x2000;

/// The interrupt lines.
inline constexpr word kIrqLineTimer = 0;
inline constexpr word kIrqLineConsole = 1;

class Device : public BaseMemory
{
  public:
    explicit Device(word base_address);

    byte read_byte(word address) override;
    hword read_hword(word address) override;
    word read_word(word address) override;
    void write_byte(word address, byte value) override;
    void write_hword(word address, hword value) override;
    void write_word(word address, word value) override;

    /// Back to the state after power on.
    virtual void reset() = 0;

  protected:
    virtual word read_register(word offset) = 0;
    virtual void write_register(word offset, word value) = 0;
};

/// The interrupt controller. Lines are served first come, first served: a line that is raised
/// joins the end of a queue, a line that is already in it is not queued again, and the CPU takes
/// the interrupt exception while the queue is not empty and IRQs are not masked. The handler reads
/// CLAIM to get the line at the head, which removes it.
///
/// | Offset | Name    | Access | |
/// | 0x00   | CLAIM   | read   | the line at the head of the queue and remove it, 0xFFFFFFFF if none |
/// | 0x04   | COUNT   | read   | the number of lines in the queue |
/// | 0x08   | ENABLE  | r/w    | bit n: line n can be raised. A raise of a disabled line is lost. Reset: 0 |
/// | 0x0C   | RAISE   | write  | raises the line with that number (software interrupt) |
/// | 0x10   | PENDING | read   | bit n: line n is in the queue |
class InterruptController : public Device
{
  public:
    InterruptController();

    /// A device raises a line. Lost if the line is not enabled or is in the queue already.
    void raise(word line);

    bool has_pending() const
    {
        return !m_queue.empty();
    }

    /// Removes and returns the line at the head, 0xFFFFFFFF if the queue is empty.
    word claim();

    void reset() override;

    word enable_mask() const
    {
        return m_enable;
    }

  protected:
    word read_register(word offset) override;
    void write_register(word offset, word value) override;

  private:
    std::deque<word> m_queue;
    word m_queued = 0; ///< bit n: line n is in the queue
    word m_enable = 0;
};

/// A timer that counts retired instructions, so a run is the same every time. Raises line 0.
///
/// | Offset | Name     | Access | |
/// | 0x00   | COUNT    | r/w    | instructions since reset (or since it was written) |
/// | 0x04   | COMPARE  | r/w    | the interrupt is raised when COUNT reaches this value |
/// | 0x08   | CTRL     | r/w    | bit 0: enabled, bit 1: periodic |
/// | 0x0C   | INTERVAL | r/w    | periodic: COMPARE grows by this each time (0 stops the timer) |
///
/// Without `periodic` the timer disables itself after raising the interrupt.
class Timer : public Device
{
  public:
    explicit Timer(InterruptController &intc);

    /// One instruction retired.
    void tick()
    {
        m_count++;
        if (UNLIKELY(m_ctrl & kEnabled) && m_count == m_compare)
        {
            fire();
        }
    }

    /// For WFI: if the timer is going to raise its interrupt, jump to that moment and raise it.
    /// Returns whether an interrupt is pending afterwards.
    bool fast_forward();

    void reset() override;

    word count() const
    {
        return m_count;
    }

    static constexpr word kEnabled = 1;
    static constexpr word kPeriodic = 2;

  protected:
    word read_register(word offset) override;
    void write_register(word offset, word value) override;

  private:
    void fire();

    InterruptController &m_intc;
    word m_count = 0;
    word m_compare = 0;
    word m_ctrl = 0;
    word m_interval = 0;
};

/// A serial console. Output goes to a stream, input is a queue of bytes that the host supplies.
/// Raises line 1 when input arrives and the receive interrupt is enabled.
///
/// | Offset | Name   | Access | |
/// | 0x00   | DATA   | r/w    | write: send the low byte. Read: take the next received byte (0 if none) |
/// | 0x04   | STATUS | read   | bit 0: a received byte is waiting, bit 1: ready to send (always) |
/// | 0x08   | CTRL   | r/w    | bit 0: raise line 1 when a byte is received |
class Console : public Device
{
  public:
    explicit Console(InterruptController &intc);

    /// Where the bytes written go. The stream must outlive the console. The default is std::cout,
    /// null discards them.
    void set_output(std::ostream *out);

    /// Queues bytes as received input.
    void push_input(const std::string &bytes);

    bool input_waiting() const
    {
        return !m_input.empty();
    }

    void reset() override;

  protected:
    word read_register(word offset) override;
    void write_register(word offset, word value) override;

  private:
    InterruptController &m_intc;
    std::ostream *m_out;
    std::deque<byte> m_input;
    word m_ctrl = 0;
};
