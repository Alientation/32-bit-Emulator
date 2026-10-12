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

        /// @return Whether the program should stop now.
        virtual bool interrupted() = 0;
    };

    /// @param[in,out] emu The machine, with the program loaded.
    explicit GdbServer(Emulator32bit &emu);

    /// @param[in] source Where to look for an interrupt while the program runs, or null for none.
    void set_interrupt_source(InterruptSource *source)
    {
        m_interrupt = source;
    }

    /// Handles one packet.
    ///
    /// @param[in] packet The payload, between `$` and `#`.
    /// @return The payload of the reply; empty for a packet that is not supported, which is what
    ///         the protocol asks for. For a packet that has no reply (`k`) see `finished`.
    std::string handle(const std::string &packet);

    /// @return Whether the client detached or killed the program, so the session is over.
    bool finished() const
    {
        return m_finished;
    }

    /// @return Whether the last reply was to a packet that has no reply at all.
    bool silent() const
    {
        return m_silent;
    }

    /// @return The XML that tells gdb what registers there are.
    static std::string target_description();

    /// Serves one client.
    ///
    /// @param[in,out] emu The machine, with the program loaded.
    /// @param[in] port The TCP port on 127.0.0.1.
    /// @param[in] ready Called with the port once it is listening (0 asks for any free port), or
    ///     null.
    /// @param[out] log Where to say what happens (a connection, an error), or null.
    /// @return `false` if the port could not be listened on.
    static bool serve(Emulator32bit &emu, U16 port, void (*ready)(U16 port), std::ostream *log);

    /// Encodes bytes as the hexadecimal text of the protocol.
    ///
    /// @param[in] bytes The bytes.
    /// @return Two lower case digits for each byte.
    static std::string to_hex(const std::string &bytes);

    /// Decodes the hexadecimal text of the protocol.
    ///
    /// @param[out] bytes Receives the bytes.
    /// @param[in] text Two digits for each byte.
    /// @return `false` if the text is not valid (an odd number of digits, or a character that is
    ///     not one).
    static bool from_hex(std::string &bytes, const std::string &text);

  private:
    /// @param[in] result How the run ended.
    /// @return The stop reply after the program ran.
    std::string stop_reply(const Emulator32bit::RunResult &result);

    /// Runs until something stops it, or the client interrupts.
    ///
    /// @param[in] steps 1 for a single step, 0 to go on.
    /// @return The stop reply.
    std::string resume(U64 steps);

    /// Handles `g`.
    ///
    /// @return The values of all the registers, as hex.
    std::string read_registers();

    /// Handles `G`.
    ///
    /// @param[in] hex The values of all the registers, as hex.
    /// @return Whether it was accepted.
    bool write_registers(const std::string &hex);

    /// Handles `P`. Only the flags of cpsr change.
    ///
    /// @param[in] number The register number of the protocol: x0-x29, sp, xzr, pc, cpsr.
    /// @param[in] value The value.
    /// @return Whether there is such a register and the value is allowed.
    bool write_register(U32 number, word value);

    /// Handles `p`.
    ///
    /// @param[out] value Receives the value.
    /// @param[in] number The register number of the protocol.
    /// @return Whether there is such a register.
    bool read_register(word &value, U32 number);

    /// Handles `m`.
    ///
    /// @param[in] address The virtual address.
    /// @param[in] length The number of bytes.
    /// @return The bytes as hex, shorter if an address failed after the first, or an error reply.
    std::string read_memory(word address, word length);

    /// Handles `M`.
    ///
    /// @param[in] address The virtual address.
    /// @param[in] bytes The bytes.
    /// @return Whether all of them were written.
    bool write_memory(word address, const std::string &bytes);

    /// Handles `Z` and `z`, the breakpoints and watchpoints.
    ///
    /// @param[in] packet The `Z` or `z` packet.
    /// @param[in] insert Whether it is `Z`, which inserts, as opposed to `z`.
    /// @return The reply: `OK`, an error, or empty if the type is not supported.
    std::string breakpoint_packet(const std::string &packet, bool insert);

    /// Handles `q`.
    ///
    /// @param[in] packet The `q` packet.
    /// @return The reply, empty if the query is not supported.
    std::string query(const std::string &packet);

    /// `monitor <command>` of gdb.
    ///
    /// @param[in] command The command line.
    /// @return The text to show.
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
