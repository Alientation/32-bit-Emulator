#pragma once

#include "emulator32bit/emulator32bit_util.h"
#include "emulator32bit/memory.h"

#include <cstdint>
#include <deque>
#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

class DeviceClock;
class HostDisplay;
class HostInput;

/// The memory mapped devices (docs/devices.md). Each is one page of registers at a physical address
/// in the device window, and is reached through the system bus like memory. A register is a word;
/// a byte or half-word read gives part of it, a byte or half-word write is a write of the word with
/// the value in that position (so a byte store to the console data register sends that byte).
inline constexpr word kIntcBase = kDeviceBase;
inline constexpr word kTimerBase = kDeviceBase + 0x1000;
inline constexpr word kConsoleBase = kDeviceBase + 0x2000;
inline constexpr word kBlockBase = kDeviceBase + 0x3000;
inline constexpr word kDisplayBase = kDeviceBase + 0x4000;
inline constexpr word kSysctlBase = kDeviceBase + 0x5000;
inline constexpr word kRtcBase = kDeviceBase + 0x6000;
inline constexpr word kRngBase = kDeviceBase + 0x7000;
inline constexpr word kInputBase = kDeviceBase + 0x8000;

/// The devices fill the pages of the window from kDeviceBase on, with no gap. An address after the
/// last one is a bus error.
inline constexpr word kNumDevices = 9;
inline constexpr word kDeviceEnd = kDeviceBase + kNumDevices * kPageSize;

/// The interrupt lines.
inline constexpr word kIrqLineTimer = 0;
inline constexpr word kIrqLineConsole = 1;
inline constexpr word kIrqLineBlock = 2;
inline constexpr word kIrqLineDisplay = 3;
inline constexpr word kIrqLineInput = 4;

/// The types of device, in bits 31-16 of the ID register of each one.
inline constexpr word kDeviceTypeInterruptController = 1;
inline constexpr word kDeviceTypeTimer = 2;
inline constexpr word kDeviceTypeConsole = 3;
inline constexpr word kDeviceTypeBlock = 4;
inline constexpr word kDeviceTypeDisplay = 5;
inline constexpr word kDeviceTypeSystemController = 6;
inline constexpr word kDeviceTypeRealTimeClock = 7;
inline constexpr word kDeviceTypeRandom = 8;
inline constexpr word kDeviceTypeInput = 9;

/// The version of every device so far, in bits 15-8 of its ID. A device that changes what it does
/// gets a higher one.
inline constexpr word kDeviceVersion = 1;

/// The interrupt line in the ID of a device that has none.
inline constexpr word kIdNoLine = 0xFF;

/// @param[in] type The type of the device (kDeviceType*).
/// @param[in] line The interrupt line it raises, or kIdNoLine.
/// @return The value of its ID register: the type in bits 31-16, kDeviceVersion in bits 15-8 and
///         the line in bits 7-0.
constexpr word device_id(const word type, const word line)
{
    return (type << 16) | (kDeviceVersion << 8) | line;
}

/// A device: one page of registers. The reads and writes of the sizes of BaseMemory are mapped
/// onto the word registers of the subclass. The last word of the page is the ID register, which
/// says what the device is; it is read only (a subclass ignores a write to an offset it does not
/// have).
class Device : public BaseMemory
{
  public:
    /// The offset of the ID register.
    static constexpr word kRegId = 0xFFC;

    /// @param[in] base_address The physical address of the page of registers.
    /// @param[in] id The value of the ID register (device_id).
    Device(word base_address, word id);

    /// @return The value of the ID register.
    word id() const
    {
        return m_id;
    }

    /// A device that counts instructions is brought up to date by the clock before each access to
    /// its registers, and the clock looks again at when it has something to do after each write.
    ///
    /// @param[in] clock The clock of the machine, which is not owned.
    void set_clock(DeviceClock *clock)
    {
        m_clock = clock;
    }

    byte read_byte(word address) override;
    hword read_hword(word address) override;
    word read_word(word address) override;
    void write_byte(word address, byte value) override;
    void write_hword(word address, hword value) override;
    void write_word(word address, word value) override;

    /// Back to the state after power on.
    virtual void reset() = 0;

  protected:
    /// @param[in] offset The offset of the register in the page, a multiple of 4.
    /// @return The value of the register.
    virtual word read_register(word offset) = 0;

    /// @param[in] offset The offset of the register in the page, a multiple of 4.
    /// @param[in] value The value written to the register.
    virtual void write_register(word offset, word value) = 0;

  private:
    /// read_register, with the device up to date (DeviceClock::catch_up).
    word read_synced(word offset);

    /// write_register, with the device up to date before and the clock told after.
    void write_synced(word offset, word value);

    DeviceClock *m_clock = nullptr;
    word m_id;
};

/// The interrupt controller. Lines are served first come, first served: a line that is raised
/// joins the end of a queue (a ring of 32 entries, which cannot overflow because a line that is
/// already in it is not queued again), and the CPU takes
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
    /// @param[in] line The number of the line, 0-31.
    void raise(word line);

    /// @return Whether any line is in the queue.
    bool has_pending() const
    {
        return m_count != 0;
    }

    /// Removes the line at the head of the queue.
    ///
    /// @return The line, kNoLine if the queue is empty.
    word claim();

    void reset() override;

    /// @return The ENABLE register: bit n is set when line n can be raised.
    word enable_mask() const
    {
        return m_enable;
    }

    /// What CLAIM gives when the queue is empty.
    static constexpr word kNoLine = 0xFFFFFFFF;

    /// The offsets of the registers.
    static constexpr word kRegClaim = 0x00;
    static constexpr word kRegCount = 0x04;
    static constexpr word kRegEnable = 0x08;
    static constexpr word kRegRaise = 0x0C;
    static constexpr word kRegPending = 0x10;

  protected:
    word read_register(word offset) override;
    void write_register(word offset, word value) override;

  private:
    static constexpr U8 kLines = 32; /// A power of two, the ring wraps with a mask.
    static_assert((kLines & (kLines - 1)) == 0);

    U8 m_ring[kLines] = {};          /// The lines in the queue, from m_head on, wrapping.
    U8 m_head = 0;
    U8 m_count = 0;
    word m_queued = 0;               /// Bit n: line n is in the queue.
    word m_enable = 0;
};

/// A timer that counts executed instructions, so a run is the same every time. Raises line 0.
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
    /// @param[in] intc The controller that gets the interrupt.
    explicit Timer(InterruptController &intc);

    /// One instruction completed. The machine moves the timer on with its DeviceClock, which calls
    /// advance; this is the same for one instruction.
    void tick()
    {
        m_count++;
        if (UNLIKELY(m_ctrl & kEnabled) && m_count == m_compare)
        {
            fire();
        }
    }

    /// For WFI and the clock: how long until the timer raises its interrupt.
    ///
    /// @return The number of instructions, or word(-1) if it is not going to.
    word cycles_until_event() const;

    /// For WFI and the clock: `count` instructions pass at once.
    ///
    /// @param[in] count The number of instructions, no more than cycles_until_event().
    void advance(word count);

    void reset() override;

    /// @return The COUNT register: the instructions since reset.
    word count() const
    {
        return m_count;
    }

    /// The bits of CTRL.
    static constexpr word kEnabled = 1;
    static constexpr word kPeriodic = 2;

    /// The offsets of the registers.
    static constexpr word kRegCount = 0x00;
    static constexpr word kRegCompare = 0x04;
    static constexpr word kRegCtrl = 0x08;
    static constexpr word kRegInterval = 0x0C;

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
    /// @param[in] intc The controller that gets the interrupt.
    explicit Console(InterruptController &intc);

    /// Where the bytes written go. The default is std::cout.
    ///
    /// @param[in] out The stream, which must outlive the console, or null to discard the bytes.
    void set_output(std::ostream *out);

    /// Queues bytes as received input.
    ///
    /// @param[in] bytes The bytes to receive.
    void push_input(const std::string &bytes);

    /// @return Whether a received byte is waiting to be read.
    bool input_waiting() const
    {
        return !m_input.empty();
    }

    /// Takes input that another thread supplies while the machine runs (a terminal, the window).
    /// The console moves it into its queue on the thread of the emulator: when a register is read,
    /// when the DeviceClock looks (at least every kHostInputPollInterval instructions) and when
    /// `WFI` waits.
    ///
    /// @param[in] input The input, which the console shares.
    void attach_host_input(std::shared_ptr<HostInput> input);

    /// @return The live input, or null.
    HostInput *host_input() const
    {
        return m_host.get();
    }

    /// Takes the bytes the host has supplied, and raises the interrupt if that is enabled.
    void poll_host_input();

    /// @return Whether a received byte raises the interrupt (CTRL bit 0).
    bool receive_interrupt_enabled() const
    {
        return (m_ctrl & kCtrlRxInterrupt) != 0;
    }

    void reset() override;

    /// The most instructions between two looks at the live input: a byte that is typed is
    /// received, and raises the interrupt, at most this many instructions later.
    static constexpr word kHostInputPollInterval = 10000;

    /// The bits of STATUS.
    static constexpr word kStatusRxWaiting = 1;
    static constexpr word kStatusTxReady = 2;

    /// The bits of CTRL.
    static constexpr word kCtrlRxInterrupt = 1;

    /// The offsets of the registers.
    static constexpr word kRegData = 0x00;
    static constexpr word kRegStatus = 0x04;
    static constexpr word kRegCtrl = 0x08;

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
/// DMA straight to and from physical RAM. A command takes `LATENCY` executed instructions (per
/// sector for DMA) to complete, then the status says so and line 2 is raised if that is enabled, so
/// the kernel can wait with `WFI`.
///
/// | Offset | Name     | Access | |
/// | 0x00   | COMMAND  | write  | 1 read the sector into the buffer, 2 write the buffer to the sector, 3 flush the storage to its file, 4 DMA read, 5 DMA write, 6 scatter/gather read, 7 scatter/gather write |
/// | 0x04   | STATUS   | r/w    | bit 0: busy, bit 1: done, bit 2: error. A write clears done and error |
/// | 0x08   | SECTOR   | r/w    | the sector number of the next command |
/// | 0x0C   | DATA     | r/w    | the word of the buffer at CURSOR, and CURSOR moves on by 4 (use word accesses) |
/// | 0x10   | CURSOR   | r/w    | byte offset in the buffer, a multiple of 4 below 512. Set to 0 when a read completes |
/// | 0x14   | CAPACITY | read   | the number of sectors, 0 when there is no disk |
/// | 0x18   | CTRL     | r/w    | bit 0: raise line 2 when a command completes |
/// | 0x1C   | LATENCY  | r/w    | instructions a command takes (at least 1). Reset: 100 |
/// | 0x20   | DMA_ADDR | r/w    | physical RAM address of a DMA transfer, a multiple of 4 |
/// | 0x24   | DMA_COUNT| r/w    | the number of sectors of a DMA transfer, from SECTOR on (at least 1); for 6 and 7 the number of descriptors |
/// | 0x28   | DMA_LIST | r/w    | physical RAM address of the descriptors of a command 6 or 7, a multiple of 4 |
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

    /// The instructions a command takes after a reset.
    static constexpr word kDefaultLatency = 100;

    /// The bits of STATUS.
    static constexpr word kBusy = 1;
    static constexpr word kDone = 2;
    static constexpr word kError = 4;

    /// The bits of CTRL.
    static constexpr word kCtrlDoneInterrupt = 1;

    /// The offsets of the registers.
    static constexpr word kRegCommand = 0x00;
    static constexpr word kRegStatus = 0x04;
    static constexpr word kRegSector = 0x08;
    static constexpr word kRegData = 0x0C;
    static constexpr word kRegCursor = 0x10;
    static constexpr word kRegCapacity = 0x14;
    static constexpr word kRegCtrl = 0x18;
    static constexpr word kRegLatency = 0x1C;
    static constexpr word kRegDmaAddr = 0x20;
    static constexpr word kRegDmaCount = 0x24;
    static constexpr word kRegDmaList = 0x28;

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

    /// @param[in] intc The controller that gets the interrupt.
    explicit BlockDevice(InterruptController &intc);

    /// The memory that DMA reads and writes (the RAM, by physical address). Without it, every DMA
    /// command fails.
    ///
    /// @param[in] memory The memory, which is not owned.
    void set_dma_memory(BaseMemory *memory)
    {
        m_dma_memory = memory;
    }

    /// Makes a disk in memory of zeros.
    ///
    /// @param[in] sectors The number of sectors.
    void set_capacity(word sectors);

    /// A disk kept in the file. A file that exists is read (its size, rounded up to sectors, is the
    /// capacity unless `sectors` is more); one that does not exist starts as zeros of `sectors`
    /// sectors, and is created when the disk is saved.
    ///
    /// @param[in] path The file.
    /// @param[in] sectors The minimum number of sectors.
    /// @return `false` if the file exists and cannot be read.
    bool open_file(const std::string &path, word sectors = 0);

    /// Writes the disk to its file, if it has one.
    ///
    /// @return `false` on an error.
    bool save();

    /// One instruction completed (the machine calls advance, through its DeviceClock).
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

    /// For WFI and the clock: how long until the command in progress completes.
    ///
    /// @return The number of instructions, or word(-1) if no command is in progress.
    word cycles_until_event() const;

    /// For WFI and the clock: `count` instructions pass at once.
    ///
    /// @param[in] count The number of instructions, no more than cycles_until_event().
    void advance(word count);

    /// A power cycle of the controller: the contents of the disk stay.
    void reset() override;

    /// For tests: the bytes of a sector.
    ///
    /// @param[in] number The sector number.
    /// @return The 512 bytes of the sector, or nothing if the sector is not on the disk.
    std::vector<byte> sector(word number) const;

  protected:
    word read_register(word offset) override;
    void write_register(word offset, word value) override;

  private:
    /// Takes a command: it is rejected with the error bit when one is in progress or the number
    /// is not a command, otherwise it becomes busy for LATENCY instructions (for each sector, for
    /// DMA).
    ///
    /// @param[in] command The number written to COMMAND.
    void start(word command);

    /// A piece of a DMA or scatter/gather command: `count` sectors from `sector` on, and the RAM
    /// at `address`.
    struct Segment
    {
        word sector;
        word count;
        word address;
    };

    /// @param[in] segment The segment of the descriptor.
    /// @return Whether the segment is inside the disk and inside the RAM, and aligned.
    bool segment_is_valid(const Segment &segment) const;

    /// Reads the descriptors of a scatter/gather command from RAM.
    ///
    /// @param[in] list The address of the list.
    /// @param[in] count The number of descriptors.
    /// @return `false` if there are none, too many, or the list is not all in RAM.
    bool read_descriptors(word list, word count);

    /// Moves the segments of the command between the disk and RAM. Nothing is moved unless all of
    /// them fit on both sides.
    ///
    /// @param[in] to_disk `true` to copy from RAM to the disk, false for the other way.
    /// @return `false` if the transfer is not valid.
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
    word m_latency = kDefaultLatency;
    word m_status = 0;

    bool m_busy = false;
    word m_command = 0;
    word m_command_sector = 0;
    word m_remaining = 0;
    BaseMemory *m_dma_memory = nullptr;
    word m_dma_addr = 0;
    word m_dma_count = 0;
    word m_dma_list = 0;
    word m_command_addr = 0;             /// DMA_ADDR and DMA_COUNT when the command was given.
    word m_command_count = 0;
    std::vector<Segment> m_segments;     /// What the command moves, from the registers or the list.
    bool m_segments_valid = false;       /// Whether they could be read.
    byte m_write_data[kSectorSize] = {}; /// The buffer when a write command was given.
};

/// A picture of the display: `width * height` pixels, row by row from the top left, each one
/// 0x00RRGGBB.
struct Frame
{
    word width = 0;
    word height = 0;
    std::vector<word> pixels;
};

/// A display (docs/display.md). The picture is in RAM, at a physical address the program chooses,
/// and the program draws with ordinary stores. Every INTERVAL executed instructions a frame ends: the
/// display counts it, says so in STATUS, raises line 3 if that is enabled and, when a window of the
/// host is attached, hands the picture over to it.
///
/// | Offset | Name     | Access | |
/// | 0x00   | CTRL     | r/w    | bit 0: on, bit 1: raise line 3 when a frame ends |
/// | 0x04   | STATUS   | r/w    | bit 0: a frame ended, bit 1: error (the picture was not in RAM, or its size not valid). A write clears both |
/// | 0x08   | ADDRESS  | r/w    | physical RAM address of the picture, a multiple of 4 |
/// | 0x0C   | WIDTH    | r/w    | pixels across, 1 to 1024. Reset: 320 |
/// | 0x10   | HEIGHT   | r/w    | pixels down, 1 to 1024. Reset: 240 |
/// | 0x14   | FORMAT   | r/w    | 0: a byte per pixel, an index into the palette. 1: a word per pixel, 0x00RRGGBB |
/// | 0x18   | INTERVAL | r/w    | instructions a frame takes, at least 1. Reset: 100000 |
/// | 0x1C   | FRAMES   | read   | the frames that ended since reset |
/// | 0x400  | PALETTE  | r/w    | 256 words, entry n at 0x400 + 4n, 0x00RRGGBB. Reset: 3-3-2 (index bits rrrgggbb) |
///
/// A frame starts when the display is turned on and when INTERVAL is written. The picture is read
/// when a frame ends, so a program that draws between two frames is seen whole.
class Display : public Device
{
  public:
    /// The most pixels across or down.
    static constexpr word kMaxSize = 1024;

    /// The entries of the palette.
    static constexpr word kPaletteSize = 256;

    /// The values after a reset.
    static constexpr word kDefaultWidth = 320;
    static constexpr word kDefaultHeight = 240;
    static constexpr word kDefaultInterval = 100000;

    /// The bits of CTRL.
    static constexpr word kCtrlOn = 1;
    static constexpr word kCtrlFrameInterrupt = 2;

    /// The bits of STATUS.
    static constexpr word kStatusFrame = 1;
    static constexpr word kStatusError = 2;

    /// The formats.
    static constexpr word kFormatIndexed = 0;
    static constexpr word kFormatRgb = 1;

    /// The offsets of the registers.
    static constexpr word kRegCtrl = 0x00;
    static constexpr word kRegStatus = 0x04;
    static constexpr word kRegAddress = 0x08;
    static constexpr word kRegWidth = 0x0C;
    static constexpr word kRegHeight = 0x10;
    static constexpr word kRegFormat = 0x14;
    static constexpr word kRegInterval = 0x18;
    static constexpr word kRegFrames = 0x1C;
    static constexpr word kRegPalette = 0x400;

    /// @param[in] intc The controller that gets the interrupt.
    explicit Display(InterruptController &intc);

    /// The memory that the picture is in (the RAM, by physical address). Without it, every frame
    /// is an error.
    ///
    /// @param[in] ram The RAM, which is not owned.
    void set_ram(RAM *ram)
    {
        m_ram = ram;
    }

    /// Hands each frame to a window of the host, which runs on another thread.
    ///
    /// @param[in] host The window's side, which the display shares.
    void attach_host_display(std::shared_ptr<HostDisplay> host);

    /// One instruction completed (the machine calls advance, through its DeviceClock).
    void tick()
    {
        if (UNLIKELY(m_ctrl & kCtrlOn))
        {
            if (--m_remaining == 0)
            {
                end_frames(1);
            }
        }
    }

    /// For the clock: how long until the frame ends.
    ///
    /// @return The number of instructions, or word(-1) if the display is off.
    word cycles_until_frame() const
    {
        return (m_ctrl & kCtrlOn) ? m_remaining : word(-1);
    }

    /// For WFI: how long until the display raises its interrupt.
    ///
    /// @return The number of instructions, or word(-1) if it is not going to (it is off, or the
    ///         interrupt is not enabled).
    word cycles_until_event() const;

    /// For WFI and the clock: `count` instructions pass at once. Any number of frames may end.
    ///
    /// @param[in] count The number of instructions.
    void advance(word count);

    /// The picture as it is in RAM now.
    ///
    /// @param[out] out The picture. Its pixels are reused.
    /// @return `false` if the picture is not all in RAM or its size is not valid (`out` is
    ///         unchanged).
    bool capture(Frame &out) const;

    void reset() override;

  protected:
    word read_register(word offset) override;
    void write_register(word offset, word value) override;

  private:
    /// @return The bytes of a pixel in the FORMAT.
    word bytes_per_pixel() const
    {
        return m_format == kFormatRgb ? 4 : 1;
    }

    /// @return Whether the picture is all in RAM, and its size is valid.
    bool picture_is_valid() const;

    /// Frames end: counts them, sets STATUS, raises the interrupt if it is enabled and hands the
    /// picture to the host's window. A new frame of INTERVAL instructions starts.
    ///
    /// @param[in] frames The number of frames that ended, at least 1.
    void end_frames(word frames);

    InterruptController &m_intc;
    RAM *m_ram = nullptr;
    std::shared_ptr<HostDisplay> m_host;
    Frame m_frame; /// The buffer handed to the host, swapped with the one it held.

    word m_ctrl = 0;
    word m_status = 0;
    word m_address = 0;
    word m_width = kDefaultWidth;
    word m_height = kDefaultHeight;
    word m_format = kFormatIndexed;
    word m_interval = kDefaultInterval;
    word m_frames = 0;
    word m_remaining = kDefaultInterval; /// Instructions until the frame ends, while it is on.
    word m_palette[kPaletteSize];
};

/// The system controller: what memory the machine has, and a way for a program to turn the machine
/// off (with an exit code, for a kernel's tests) or to reset it.
///
/// | Offset | Name      | Access | |
/// | 0x00   | POWEROFF  | write  | turns the machine off after this instruction, the value is the exit code |
/// | 0x04   | RESET     | write  | resets the machine after this instruction, which starts again at its boot address |
/// | 0x08   | RAM_START | read   | the physical address of the RAM |
/// | 0x0C   | RAM_SIZE  | read   | the bytes of RAM |
/// | 0x10   | ROM_START | read   | the physical address of the ROM, 0 when there is none |
/// | 0x14   | ROM_SIZE  | read   | the bytes of ROM |
/// | 0x18   | DEVICE_COUNT | read | the number of devices, in the pages from kDeviceBase on |
///
/// The device only records the request; the DeviceClock sees it after the instruction that made it,
/// and `Emulator32bit::run` serves it.
class SystemController : public Device
{
  public:
    /// What a program asked the machine to do.
    enum class Request : U8
    {
        NONE,
        POWER_OFF,
        RESET,
    };

    /// The offsets of the registers.
    static constexpr word kRegPowerOff = 0x00;
    static constexpr word kRegReset = 0x04;
    static constexpr word kRegRamStart = 0x08;
    static constexpr word kRegRamSize = 0x0C;
    static constexpr word kRegRomStart = 0x10;
    static constexpr word kRegRomSize = 0x14;
    static constexpr word kRegDeviceCount = 0x18;

    /// @param[in] ram The RAM, which is not owned.
    /// @param[in] rom The ROM, which is not owned.
    SystemController(const BaseMemory &ram, const BaseMemory &rom);

    /// @return What the program asked for, NONE until it asks and after a reset.
    Request request() const
    {
        return m_request;
    }

    /// @return The value written to POWEROFF.
    word exit_code() const
    {
        return m_exit_code;
    }

    /// Forgets the request: the machine that was turned off is on again.
    void reset() override;

  protected:
    word read_register(word offset) override;
    void write_register(word offset, word value) override;

  private:
    const BaseMemory &m_ram;
    const BaseMemory &m_rom;
    Request m_request = Request::NONE;
    word m_exit_code = 0;
};

/// The speed that the machine is taken to run at, by the devices that turn executed instructions
/// into time: 100 million instructions a second, about what the emulator does on a desktop.
inline constexpr U64 kInstructionsPerSecond = 100'000'000;

/// A real time clock: the time of day, for `time ()` and the dates of files.
///
/// | Offset | Name    | Access | |
/// | 0x00   | SECONDS | r/w    | seconds since 1970-01-01 00:00 UTC. A read keeps the microseconds for MICROS, a write sets the time to that second |
/// | 0x04   | MICROS  | read   | the microseconds within the second that the last read of SECONDS gave |
///
/// The time is the machine's own unless the host says otherwise: it moves on by a second every
/// kInstructionsPerSecond executed instructions, from the time the host set (0 by default), so a
/// run is the same every time. The time of the host (use_host_time) is the host's clock instead. A
/// reset of the machine keeps the time, like the battery of a real clock.
class RealTimeClock : public Device
{
  public:
    /// The offsets of the registers.
    static constexpr word kRegSeconds = 0x00;
    static constexpr word kRegMicros = 0x04;

    RealTimeClock();

    /// Sets the time, as a write of SECONDS does.
    ///
    /// @param[in] seconds The time now, in seconds since 1970.
    void set_time(U64 seconds);

    /// Makes the time that of the host's clock (`true`), or of the machine, from the time it is
    /// now (`false`).
    ///
    /// @param[in] host Whether the time is the host's.
    void use_host_time(bool host);

    /// @return The time now, in microseconds since 1970.
    U64 microseconds() const;

    /// For the clock: `count` instructions pass at once.
    ///
    /// @param[in] count The number of instructions.
    void advance(word count)
    {
        m_instructions += count;
    }

    /// Keeps the time.
    void reset() override;

  protected:
    word read_register(word offset) override;
    void write_register(word offset, word value) override;

  private:
    /// @return The microseconds that the source of the time counts: the executed instructions, or
    ///         the host's clock.
    U64 source_microseconds() const;

    U64 m_instructions = 0; /// Executed since the machine was made.
    bool m_host = false;
    S64 m_offset = 0;       /// The time is the microseconds of the source plus this.
    word m_latched_micros = 0;
};

/// Random numbers, for stack canaries, the seed of `rand ()` and addresses that differ from run to
/// run.
///
/// | Offset | Name | Access | |
/// | 0x00   | DATA | read   | the next random word |
/// | 0x04   | SEED | write  | the numbers start again from this seed |
///
/// The numbers come from a generator (SplitMix64) whose seed the host sets (0 by default), so a
/// run is the same every time. With use_host_entropy they come from the host (std::random_device)
/// instead, until a program writes SEED. A reset goes back to what the host set.
class RandomNumbers : public Device
{
  public:
    /// The offsets of the registers.
    static constexpr word kRegData = 0x00;
    static constexpr word kRegSeed = 0x04;

    RandomNumbers();

    /// Sets the seed that the numbers start from, now and after a reset, and uses the generator.
    ///
    /// @param[in] seed The seed.
    void set_seed(word seed);

    /// Takes the numbers from the host, now and after a reset, which a run cannot repeat.
    void use_host_entropy();

    /// @return The next number, as a read of DATA does.
    word next();

    /// Starts the numbers again from what the host set.
    void reset() override;

  protected:
    word read_register(word offset) override;
    void write_register(word offset, word value) override;

  private:
    U64 m_state = 0;          /// Of the generator.
    word m_seed = 0;          /// The host's.
    bool m_entropy = false;   /// Whether the host wants its own numbers.
    bool m_from_host = false; /// Whether they come from the host now, not the generator.
};

/// An event of the keyboard or the mouse, which the host gives the input device.
struct InputEvent
{
    enum class Kind : U8
    {
        KEY,           /// A key went down, repeated or went up.
        MOUSE_MOVE,    /// The mouse is at a new position.
        MOUSE_BUTTONS, /// A button of the mouse went down or up.
        MOUSE_WHEEL,   /// The wheel turned.
    };

    Kind kind;
    word value; /// KEY: the word that KEY reads (InputDevice::key_word). MOUSE_BUTTONS: the buttons
                /// that are down (InputDevice::kButton*). MOUSE_WHEEL: the steps, up positive.
    word x = 0; /// MOUSE_MOVE: the position, in pixels of the display from the top left.
    word y = 0;
};

/// The keyboard and the mouse of the window (docs/devices.md). Key events wait in a queue; the
/// mouse is the state it is in now. Raises line 4 when a key event arrives or the mouse changes,
/// as CTRL says.
///
/// | Offset | Name    | Access | |
/// | 0x00   | KEY     | read   | take the next key event, 0 if there is none (InputDevice::key_word) |
/// | 0x04   | STATUS  | r/w    | bit 0: a key event is waiting, bit 1: the mouse changed, bit 2: key events were lost. A write clears bits 1 and 2 |
/// | 0x08   | CTRL    | r/w    | bit 0: raise line 4 for a key event, bit 1: raise it when the mouse changes |
/// | 0x0C   | MOUSE_X | read   | the position across, in pixels of the display |
/// | 0x10   | MOUSE_Y | read   | the position down |
/// | 0x14   | BUTTONS | read   | bit 0: left, bit 1: right, bit 2: middle |
/// | 0x18   | WHEEL   | read   | the steps the wheel turned since the last read (up positive), and set it to 0 |
class InputDevice : public Device
{
  public:
    /// The offsets of the registers.
    static constexpr word kRegKey = 0x00;
    static constexpr word kRegStatus = 0x04;
    static constexpr word kRegCtrl = 0x08;
    static constexpr word kRegMouseX = 0x0C;
    static constexpr word kRegMouseY = 0x10;
    static constexpr word kRegButtons = 0x14;
    static constexpr word kRegWheel = 0x18;

    /// The bits of STATUS.
    static constexpr word kStatusKeyWaiting = 1;
    static constexpr word kStatusMouseChanged = 2;
    static constexpr word kStatusLost = 4;

    /// The bits of CTRL.
    static constexpr word kCtrlKeyInterrupt = 1;
    static constexpr word kCtrlMouseInterrupt = 2;

    /// The bits of a key event, above the key code in bits 15-0 (a USB HID usage, the code of the
    /// key's position and not of what it prints: 4 is A, 40 Enter, 82 up).
    static constexpr word kKeyCodeMask = 0xFFFF;
    static constexpr word kKeyPressed = word(1) << 16; /// 0: the key went up.
    static constexpr word kKeyRepeat = word(1) << 17;  /// The key is held, a press again.
    static constexpr word kModShift = word(1) << 24;   /// The modifiers that were down.
    static constexpr word kModCtrl = word(1) << 25;
    static constexpr word kModAlt = word(1) << 26;
    static constexpr word kModGui = word(1) << 27;

    /// The bits of BUTTONS.
    static constexpr word kButtonLeft = 1;
    static constexpr word kButtonRight = 2;
    static constexpr word kButtonMiddle = 4;

    /// The key events that wait at most; one more is lost.
    static constexpr word kQueueSize = 64;

    /// @param[in] code The key code, a USB HID usage (1 to 0xFFFF).
    /// @param[in] flags kKeyPressed, kKeyRepeat and the kMod* bits.
    /// @return The word that KEY reads for the event.
    static constexpr word key_word(const word code, const word flags)
    {
        return (code & kKeyCodeMask) | flags;
    }

    /// @param[in] intc The controller that gets the interrupt.
    explicit InputDevice(InterruptController &intc);

    /// Takes an event, as if it came from the host now.
    ///
    /// @param[in] event The event.
    void push(const InputEvent &event);

    /// Takes the events that another thread supplies while the machine runs (the window). The
    /// device moves them in on the thread of the emulator, like the console its bytes: when a
    /// register is read, when the DeviceClock looks and when `WFI` waits.
    ///
    /// @param[in] input The input, which the device shares.
    void attach_host_input(std::shared_ptr<HostInput> input);

    /// @return The live input, or null.
    HostInput *host_input() const
    {
        return m_host.get();
    }

    /// Takes the events the host has supplied.
    void poll_host_input();

    /// @return Whether any event raises the interrupt (CTRL is not 0).
    bool interrupt_enabled() const
    {
        return m_ctrl != 0;
    }

    void reset() override;

  protected:
    word read_register(word offset) override;
    void write_register(word offset, word value) override;

  private:
    /// Raises the interrupt for a key event (`key`) or a change of the mouse, if CTRL says so.
    ///
    /// @param[in] key Whether it is for a key event.
    void interrupt(bool key);

    InterruptController &m_intc;
    std::shared_ptr<HostInput> m_host;
    std::vector<InputEvent> m_arrived; /// What was taken from the host, reused.
    word m_ctrl = 0;
    word m_flags = 0;                  /// kStatusMouseChanged and kStatusLost.
    word m_x = 0;
    word m_y = 0;
    word m_buttons = 0;
    word m_wheel = 0;
    word m_keys[kQueueSize] = {}; /// The key events, from m_head on, wrapping.
    word m_head = 0;
    word m_count = 0;
};

/// The time of the devices that count executed instructions: the timer, the block device and the
/// display. Rather than each of them looking at every instruction, the clock counts down to the
/// first moment that one of them has something to do, and only then gives them the instructions
/// that passed (`advance`). A device is also brought up to date before its registers are read or
/// written (Device::set_clock), and the countdown starts again after a write, which can change when
/// the next event is. So a device sees the same instructions, and raises its interrupt after the
/// same one, as if it counted them one at a time.
///
/// The clock also takes the live input from the host of the console and the input device, at least
/// every Console::kHostInputPollInterval instructions while there is one. When a byte arrives is up to
/// the host, so the machine does not have to look before every instruction. And it tells `run`,
/// after the instruction that made it, of a request of the system controller.
class DeviceClock
{
  public:
    /// @param[in] timer The timer.
    /// @param[in] block The block device.
    /// @param[in] display The display.
    /// @param[in] console The console, whose live input the clock takes.
    /// @param[in] sysctl The system controller, whose request the clock passes on.
    /// @param[in] rtc The real time clock.
    /// @param[in] input The input device, whose live input the clock takes.
    DeviceClock(Timer &timer, BlockDevice &block, Display &display, Console &console,
                const SystemController &sysctl, RealTimeClock &rtc, InputDevice &input);

    /// Called after each instruction that completes.
    ///
    /// @return Whether the system controller has a request, which the machine serves now.
    bool tick()
    {
        if (UNLIKELY(--m_until_event == 0))
        {
            return sync();
        }
        return false;
    }

    /// Gives the devices the instructions that completed since the last time, then counts down
    /// to the next moment one of them has something to do. Code that runs inside
    /// `Emulator32bit::run` calls it before looking at a device directly (not through its
    /// registers) and after changing one directly; `run` calls it when it starts and ends.
    ///
    /// @return Whether the system controller has a request. While it has, the next tick syncs.
    bool sync();

    /// Gives the devices the instructions that completed since the last time, without looking at
    /// when the next event is: it is the same moment, since the instructions that passed are fewer
    /// than any device waits for. For a read of a register, which does not change when a device
    /// has something to do.
    void catch_up();

    /// Moves all the devices on, for WFI right after a sync (which syncs again after it).
    ///
    /// @param[in] passed The instructions, no more than the first device waits for.
    void advance(word passed);

  private:
    /// The longest countdown, so a timer that waits for its count to wrap around is not passed.
    static constexpr word kMaxCountdown = word(1) << 30;

    Timer &m_timer;
    BlockDevice &m_block;
    Display &m_display;
    Console &m_console;
    const SystemController &m_sysctl;
    RealTimeClock &m_rtc;
    InputDevice &m_input;
    word m_countdown = 1;   /// What the count down started from.
    word m_until_event = 1; /// Instructions left of it.
};
