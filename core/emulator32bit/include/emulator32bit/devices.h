#pragma once

#include "emulator32bit/emulator32bit_util.h"
#include "emulator32bit/memory.h"

#include <cstdint>
#include <deque>
#include <iosfwd>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class HostInput;

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

/// A device: one page of registers. The reads and writes of the sizes of BaseMemory are mapped
/// onto the word registers of the subclass.
class Device : public BaseMemory
{
  public:
    /// @param base_address the physical address of the page of registers
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
    /// @param offset the offset of the register in the page, a multiple of 4
    /// @return the value of the register
    virtual word read_register(word offset) = 0;

    /// @param offset the offset of the register in the page, a multiple of 4
    /// @param value the value written to the register
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
    ///
    /// @param line the number of the line, 0-31
    void raise(word line);

    /// @return whether any line is in the queue
    bool has_pending() const
    {
        return !m_queue.empty();
    }

    /// Removes the line at the head of the queue.
    ///
    /// @return the line, 0xFFFFFFFF if the queue is empty
    word claim();

    void reset() override;

    /// @return the ENABLE register: bit n is set when line n can be raised
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
    /// @param intc the controller that gets the interrupt
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

    /// For WFI: how long until the timer raises its interrupt.
    ///
    /// @return the number of instructions, or nothing if it is not going to
    std::optional<word> cycles_until_event() const;

    /// For WFI: `count` instructions pass at once.
    ///
    /// @param count the number of instructions, no more than cycles_until_event()
    void advance(word count);

    void reset() override;

    /// @return the COUNT register: the instructions since reset
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
    /// The count reached COMPARE: raises the interrupt, then sets the next COMPARE if the timer is
    /// periodic and disables it otherwise.
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
    /// @param intc the controller that gets the interrupt
    explicit Console(InterruptController &intc);

    /// Where the bytes written go. The default is std::cout.
    ///
    /// @param out the stream, which must outlive the console, or null to discard the bytes
    void set_output(std::ostream *out);

    /// Queues bytes as received input.
    ///
    /// @param bytes the bytes to receive
    void push_input(const std::string &bytes);

    /// @return whether a received byte is waiting to be read
    bool input_waiting() const
    {
        return !m_input.empty();
    }

    /// Takes input that another thread supplies while the machine runs (a terminal). The console
    /// moves it into its queue on the thread of the emulator: when a register is read, when
    /// `Emulator32bit::run` calls `poll_host_input` between instructions, and when `WFI` waits.
    ///
    /// @param input the input, which the console shares
    void attach_host_input(std::shared_ptr<HostInput> input);

    /// @return whether there is a live input
    bool has_host_input() const
    {
        return m_host != nullptr;
    }

    /// Takes the bytes the host has supplied, and raises the interrupt if that is enabled.
    void poll_host_input();

    /// @return whether a received byte raises the interrupt (CTRL bit 0)
    bool receive_interrupt_enabled() const
    {
        return (m_ctrl & 1) != 0;
    }

    /// For WFI with nothing else to wait for: blocks until the host supplies a byte.
    ///
    /// @return false if the input has ended and no byte will come (or there is no live input)
    bool wait_host_input();

    void reset() override;

  protected:
    word read_register(word offset) override;
    void write_register(word offset, word value) override;

  private:
    InterruptController &m_intc;
    std::ostream *m_out;
    std::deque<byte> m_input;
    std::shared_ptr<HostInput> m_host;
    word m_ctrl = 0;
};

/// A block device for the operating system: a disk of 512 byte sectors that is read and written one
/// sector at a time through a buffer in the device and a data register, or many sectors at once by
/// DMA straight to and from physical RAM. A command takes `LATENCY` retired instructions (per
/// sector for DMA) to complete, then the status says so and line 2 is raised if that is enabled, so
/// the kernel can wait with `WFI`.
///
/// | Offset | Name     | Access | |
/// | 0x00   | COMMAND  | write  | 1 read the sector into the buffer, 2 write the buffer to the sector, 3 flush the storage to its file, 4 DMA read, 5 DMA write, 6 scatter/gather read, 7 scatter/gather write |
/// | 0x20   | DMA_ADDR | r/w    | physical RAM address of a DMA transfer, a multiple of 4 |
/// | 0x24   | DMA_COUNT| r/w    | the number of sectors of a DMA transfer, from SECTOR on (at least 1); for 6 and 7 the number of descriptors |
/// | 0x28   | DMA_LIST | r/w    | physical RAM address of the descriptors of a command 6 or 7, a multiple of 4 |
/// | 0x04   | STATUS   | r/w    | bit 0: busy, bit 1: done, bit 2: error. A write clears done and error |
/// | 0x08   | SECTOR   | r/w    | the sector number of the next command |
/// | 0x0C   | DATA     | r/w    | the word of the buffer at CURSOR, and CURSOR moves on by 4 (use word accesses) |
/// | 0x10   | CURSOR   | r/w    | byte offset in the buffer, a multiple of 4 below 512. Set to 0 when a read completes |
/// | 0x14   | CAPACITY | read   | the number of sectors, 0 when there is no disk |
/// | 0x18   | CTRL     | r/w    | bit 0: raise line 2 when a command completes |
/// | 0x1C   | LATENCY  | r/w    | instructions a command takes (at least 1). Reset: 100 |
///
/// A scatter/gather command moves the disk to or from several buffers in one command: DMA_LIST
/// points at DMA_COUNT descriptors of three words, `sector`, `count` (sectors) and `address` (RAM,
/// a multiple of 4), each like a DMA command of its own. The list is read when the command is
/// given, the data moves when it completes, and it fails as a whole, moving nothing, if any
/// descriptor is not valid.
///
/// A command given while busy is ignored and sets error. A sector outside of the disk completes with
/// error and changes nothing. So does a DMA transfer with a count of 0, an address that is not word
/// aligned, or a range that is not all in RAM. The data is moved when the command completes, so the
/// RAM buffer must stay in place and unchanged until then (DMA write reads it at that moment). The
/// contents of the disk survive a reset; they are in memory, or in a file the host gave
/// (`open_file`) that FLUSH, `save` and destroying the machine write.
class BlockDevice : public Device
{
  public:
    /// The size of a sector in bytes.
    static constexpr word kSectorSize = 512;

    /// The bits of STATUS.
    static constexpr word kBusy = 1;
    static constexpr word kDone = 2;
    static constexpr word kError = 4;

    /// The commands.
    static constexpr word kCmdRead = 1;
    static constexpr word kCmdWrite = 2;
    static constexpr word kCmdFlush = 3;
    static constexpr word kCmdDmaRead = 4;
    static constexpr word kCmdDmaWrite = 5;
    static constexpr word kCmdScatterRead = 6;
    static constexpr word kCmdScatterWrite = 7;

    /// The most descriptors of a scatter/gather command.
    static constexpr word kMaxDescriptors = 1024;

    /// @param intc the controller that gets the interrupt
    explicit BlockDevice(InterruptController &intc);

    /// The memory that DMA reads and writes (the RAM, by physical address). Without it, every DMA
    /// command fails.
    ///
    /// @param memory the memory, which is not owned
    void set_dma_memory(BaseMemory *memory)
    {
        m_dma_memory = memory;
    }

    /// Makes a disk in memory of zeros.
    ///
    /// @param sectors the number of sectors
    void set_capacity(word sectors);

    /// A disk kept in the file. A file that exists is read (its size, rounded up to sectors, is the
    /// capacity unless `sectors` is more); one that does not is made with `sectors` sectors.
    ///
    /// @param path the file
    /// @param sectors the minimum number of sectors
    /// @return false if the file cannot be read
    bool open_file(const std::string &path, word sectors = 0);

    /// Writes the disk to its file, if it has one.
    ///
    /// @return false on an error
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

    /// For WFI: how long until the command in progress completes.
    ///
    /// @return the number of instructions, or nothing if no command is in progress
    std::optional<word> cycles_until_event() const;

    /// For WFI: `count` instructions pass at once.
    ///
    /// @param count the number of instructions, no more than cycles_until_event()
    void advance(word count);

    /// A power cycle of the controller: the contents of the disk stay.
    void reset() override;

    /// For tests: the bytes of a sector.
    ///
    /// @param number the sector number
    /// @return the 512 bytes of the sector
    std::vector<byte> sector(word number) const;

  protected:
    word read_register(word offset) override;
    void write_register(word offset, word value) override;

  private:
    /// Takes a command: it is rejected with the error bit when one is in progress or the number
    /// is not a command, otherwise it becomes busy for LATENCY instructions (for each sector, for
    /// DMA).
    ///
    /// @param command the number written to COMMAND
    void start(word command);

    /// A piece of a DMA or scatter/gather command: `count` sectors from `sector` on, and the RAM
    /// at `address`.
    struct Segment
    {
        word sector;
        word count;
        word address;
    };

    /// @return whether the segment is inside the disk and inside the RAM, and aligned
    bool segment_is_valid(const Segment &segment) const;

    /// Reads the descriptors of a scatter/gather command from RAM.
    ///
    /// @param list the address of the list
    /// @param count the number of descriptors
    /// @return false if there are none, too many, or the list is not all in RAM
    bool read_descriptors(word list, word count);

    /// Moves the segments of the command between the disk and RAM. Nothing is moved unless all of
    /// them fit on both sides.
    ///
    /// @param to_disk true to copy from RAM to the disk, false for the other way
    /// @return false if the transfer is not valid
    bool transfer(bool to_disk);

    /// Finishes the command: does its work, sets done (and error if it failed) and raises the
    /// interrupt if that is enabled.
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
    BaseMemory *m_dma_memory = nullptr;
    word m_dma_addr = 0;
    word m_dma_count = 0;
    word m_dma_list = 0;
    word m_command_addr = 0;             ///< DMA_ADDR and DMA_COUNT when the command was given
    word m_command_count = 0;
    std::vector<Segment> m_segments;     ///< what the command moves, from the registers or the list
    bool m_segments_valid = false;       ///< whether they could be read
    byte m_write_data[kSectorSize] = {}; ///< the buffer when a write command was given
};
