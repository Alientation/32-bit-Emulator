#pragma once

#include "emulator32bit/emulator32bit.h"

#include <cstdint>
#include <string>

/// A server for the GDB remote serial protocol (docs/debugging.md), so that gdb, or an IDE that
/// drives it, can debug a program on the emulator: registers, memory, breakpoints, watchpoints,
/// single steps and continue.
///
/// The packets are handled by `handle`, which knows nothing of sockets, so it is tested without
/// one. `serve` is the TCP side: it listens on the loopback address, serves one client and returns
/// when it detaches, kills the program or hangs up.
///
/// The registers are those of the target description (`target_description ()`): x0-x29, sp, pc and
/// the processor state, each 32 bits. gdb has no architecture for this processor, so it shows the
/// registers and memory but does not disassemble: use `x/i` of the debugger of `emu32 --debug`
/// for that.
class GdbServer
{
  public:
    /// Asks whether the client sent an interrupt (Ctrl-C) while the program runs.
    class InterruptSource
    {
      public:
        virtual ~InterruptSource() = default;

        /// @return whether the program should stop now
        virtual bool interrupted() = 0;
    };

    /// @param emu the machine, with the program loaded
    explicit GdbServer(Emulator32bit &emu);

    /// @param source where to look for an interrupt while the program runs, or null for none
    void set_interrupt_source(InterruptSource *source)
    {
        m_interrupt = source;
    }

    /// Handles one packet.
    ///
    /// @param packet the payload, between `$` and `#`
    /// @return the payload of the reply; empty for a packet that is not supported, which is what
    ///         the protocol asks for. For a packet that has no reply (`k`) see `finished`.
    std::string handle(const std::string &packet);

    /// @return whether the client detached or killed the program, so the session is over
    bool finished() const
    {
        return m_finished;
    }

    /// @return whether the last reply was to a packet that has no reply at all
    bool silent() const
    {
        return m_silent;
    }

    /// The XML that tells gdb what registers there are.
    static std::string target_description();

    /// Serves one client.
    ///
    /// @param port the TCP port on 127.0.0.1
    /// @param ready called with the port once it is listening (0 asks for any free port), or null
    /// @param log where to say what happens (a connection, an error), or null
    /// @return false if the port could not be listened on
    static bool serve(Emulator32bit &emu, std::uint16_t port, void (*ready)(std::uint16_t port),
                      std::ostream *log);

    /// Encodes bytes as the hexadecimal text of the protocol.
    static std::string to_hex(const std::string &bytes);

    /// @return the bytes of the hexadecimal text, or nothing if it is not valid
    static bool from_hex(const std::string &text, std::string &bytes);

  private:
    /// The stop reply after the program ran.
    std::string stop_reply(const Emulator32bit::RunResult &result);

    /// Runs until something stops it, or the client interrupts.
    ///
    /// @param steps 1 for a single step, 0 to go on
    std::string resume(U64 steps);

    std::string read_registers();
    bool write_registers(const std::string &hex);
    bool write_register(unsigned number, word value);
    bool read_register(unsigned number, word &value);
    std::string read_memory(word address, word length);
    bool write_memory(word address, const std::string &bytes);
    std::string breakpoint_packet(const std::string &packet, bool insert);
    std::string query(const std::string &packet);

    /// `monitor <command>` of gdb.
    ///
    /// @return the text to show
    std::string monitor(const std::string &command);

    Emulator32bit &m_emu;
    InterruptSource *m_interrupt = nullptr;
    bool m_finished = false;
    bool m_silent = false;
    std::string m_last_stop = "S05";

    /// A watchpoint hit was reported and the client has not gone on yet. gdb takes it that this
    /// processor reports the hit *before* the instruction that accessed the memory (as ARM
    /// does), removes the watchpoint and single-steps that instruction. Here the instruction has
    /// finished already, so that one step is not done.
    bool m_after_watch_hit = false;
};
