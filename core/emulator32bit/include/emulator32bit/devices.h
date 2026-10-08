#pragma once

#include "emulator32bit/emulator32bit_util.h"
#include "emulator32bit/memory.h"

#include <cstdint>
#include <deque>
#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

/// The memory mapped devices (docs/devices.md). Each is one page of registers at a physical address
/// in the device window, and is reached through the system bus like memory. A register is a word;
/// a byte or half-word read gives part of it, a byte or half-word write is a write of the word with
/// the value in that position (so a byte store to the console data register sends that byte).
inline constexpr word kIntcBase = kDeviceBase;
inline constexpr word kTimerBase = kDeviceBase + 0x1000;
inline constexpr word kConsoleBase = kDeviceBase + 0x2000;
inline constexpr word kBlockBase = kDeviceBase + 0x3000;

/// The interrupt lines.
inline constexpr word kIrqLineTimer = 0;
inline constexpr word kIrqLineConsole = 1;
inline constexpr word kIrqLineBlock = 2;

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

    /// For WFI: the number of instructions until the timer raises its interrupt, if it is going to.
    std::optional<word> cycles_until_event() const;

    /// For WFI: `count` instructions pass at once (no more than cycles_until_event()).
    void advance(word count);

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

/// A block device for the operating system: a disk of 512 byte sectors that is read and written one
/// sector at a time through a buffer in the device, with a data register (no DMA yet). A command
/// takes `LATENCY` retired instructions to complete, then the status says so and line 2 is raised if
/// that is enabled, so the kernel can wait with `WFI`.
///
/// | Offset | Name     | Access | |
/// | 0x00   | COMMAND  | write  | 1 read the sector into the buffer, 2 write the buffer to the sector, 3 flush the storage to its file |
/// | 0x04   | STATUS   | r/w    | bit 0: busy, bit 1: done, bit 2: error. A write clears done and error |
/// | 0x08   | SECTOR   | r/w    | the sector number of the next command |
/// | 0x0C   | DATA     | r/w    | the word of the buffer at CURSOR, and CURSOR moves on by 4 (use word accesses) |
/// | 0x10   | CURSOR   | r/w    | byte offset in the buffer, a multiple of 4 below 512. Set to 0 when a read completes |
/// | 0x14   | CAPACITY | read   | the number of sectors, 0 when there is no disk |
/// | 0x18   | CTRL     | r/w    | bit 0: raise line 2 when a command completes |
/// | 0x1C   | LATENCY  | r/w    | instructions a command takes (at least 1). Reset: 100 |
///
/// A command given while busy is ignored and sets error. A sector outside of the disk completes with
/// error and changes nothing. The contents of the disk survive a reset; they are in memory, or in a
/// file the host gave (`open_file`) that FLUSH, `save` and destroying the machine write.
class BlockDevice : public Device
{
  public:
    static constexpr word kSectorSize = 512;

    static constexpr word kBusy = 1;
    static constexpr word kDone = 2;
    static constexpr word kError = 4;

    static constexpr word kCmdRead = 1;
    static constexpr word kCmdWrite = 2;
    static constexpr word kCmdFlush = 3;

    explicit BlockDevice(InterruptController &intc);

    /// A disk in memory of this many sectors, all zeros.
    void set_capacity(word sectors);

    /// A disk kept in the file. A file that exists is read (its size, rounded up to sectors, is the
    /// capacity unless `sectors` is more); one that does not is made with `sectors` sectors.
    /// Returns false if the file cannot be read.
    bool open_file(const std::string &path, word sectors = 0);

    /// Writes the disk to its file, if it has one. Returns false on an error.
    bool save();

    /// One instruction retired.
    void tick()
    {
        if (UNLIKELY(m_busy))
        {
            if (--m_remaining == 0)
            {
                complete();
            }
        }
    }

    /// For WFI: the number of instructions until the command in progress completes.
    std::optional<word> cycles_until_event() const;

    /// For WFI: `count` instructions pass at once (no more than cycles_until_event()).
    void advance(word count);

    void reset() override;

    /// For tests: the bytes of a sector.
    std::vector<byte> sector(word number) const;

  protected:
    word read_register(word offset) override;
    void write_register(word offset, word value) override;

  private:
    void start(word command);
    void complete();

    InterruptController &m_intc;
    std::vector<byte> m_storage;
    std::string m_path;

    byte m_buffer[kSectorSize] = {};
    word m_cursor = 0;
    word m_sector = 0;
    word m_ctrl = 0;
    word m_latency = 100;
    word m_status = 0;

    bool m_busy = false;
    word m_command = 0;
    word m_command_sector = 0;
    word m_remaining = 0;
    byte m_write_data[kSectorSize] = {}; ///< the buffer when a write command was given
};
