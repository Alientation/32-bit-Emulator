#include "emulator32bit/gdb_server.h"

#include "util/logger.h"

#include <algorithm>
#include <cstdlib>
#include <format>
#include <ostream>

#if defined(__unix__) || defined(__APPLE__)
#define AEMU_GDB_SOCKETS 1
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace
{

// The numbers of the registers in the packets. gdb has no architecture for this processor, so the
// description is the ARM core one, which it accepts with any more registers: r0-r12 are x0-x12, sp
// is sp, lr is x29 (the link register), pc is pc, cpsr is the processor state in the bits of an ARM
// one, and x13-x28 follow as registers of their own.
constexpr unsigned kRegisterSp = 13;
constexpr unsigned kRegisterLr = 14;
constexpr unsigned kRegisterPc = 15;
constexpr unsigned kRegisterCpsr = 16;
constexpr unsigned kRegisterFirstExtra = 17; // x13
constexpr unsigned kRegisterCount = 33;      // up to x28

/// @return the number of the emulator register that gdb's register is, or -1 for pc and cpsr
int emulator_register(const unsigned number)
{
    if (number <= 12) return int(number);
    if (number == kRegisterSp) return 30;
    if (number == kRegisterLr) return 29;
    if (number >= kRegisterFirstExtra && number < kRegisterCount)
    {
        return int(number - kRegisterFirstExtra + 13);
    }
    return -1;
}

/// The instructions run between two looks at the client, which can interrupt.
constexpr U64 kSlice = 20000;

/// The most bytes a read or write of memory moves in one packet.
constexpr word kMaxTransfer = 0x4000;

int hex_digit(const char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/// A number in hexadecimal, up to 32 bits.
bool parse_hex(const std::string &text, word &out)
{
    if (text.empty() || text.size() > 8) return false;
    word value = 0;
    for (const char c : text)
    {
        const int digit = hex_digit(c);
        if (digit < 0) return false;
        value = (value << 4) | word(digit);
    }
    out = value;
    return true;
}

/// The bytes of a word, least significant first, as hexadecimal text.
std::string word_to_hex(const word value)
{
    return std::format("{:02x}{:02x}{:02x}{:02x}", value & 0xFF, (value >> 8) & 0xFF,
                       (value >> 16) & 0xFF, value >> 24);
}

bool hex_to_word(const std::string &text, word &out)
{
    std::string bytes;
    if (text.size() != 8 || !GdbServer::from_hex(text, bytes)) return false;
    out = word(U8(bytes[0])) | (word(U8(bytes[1])) << 8) | (word(U8(bytes[2])) << 16)
          | (word(U8(bytes[3])) << 24);
    return true;
}

/// Splits "a,b" at the first comma.
bool split_comma(const std::string &text, std::string &first, std::string &second)
{
    const size_t comma = text.find(',');
    if (comma == std::string::npos) return false;
    first = text.substr(0, comma);
    second = text.substr(comma + 1);
    return true;
}

} // namespace

GdbServer::GdbServer(Emulator32bit &emu) :
    m_emu(emu)
{
    // A `brk` in the program stops it, like a breakpoint.
    m_emu.set_brk_stops(true);
}

std::string GdbServer::to_hex(const std::string &bytes)
{
    std::string text;
    for (const char c : bytes)
    {
        text += std::format("{:02x}", U8(c));
    }
    return text;
}

bool GdbServer::from_hex(const std::string &text, std::string &bytes)
{
    if (text.size() % 2 != 0) return false;
    bytes.clear();
    for (size_t i = 0; i < text.size(); i += 2)
    {
        const int high = hex_digit(text[i]);
        const int low = hex_digit(text[i + 1]);
        if (high < 0 || low < 0) return false;
        bytes += char(high * 16 + low);
    }
    return true;
}

std::string GdbServer::target_description()
{
    std::string xml =
        "<?xml version=\"1.0\"?>\n"
        "<!DOCTYPE target SYSTEM \"gdb-target.dtd\">\n"
        "<target version=\"1.0\">\n"
        "  <architecture>arm</architecture>\n"
        "  <feature name=\"org.gnu.gdb.arm.core\">\n";
    for (unsigned i = 0; i <= 12; i++)
    {
        xml += std::format("    <reg name=\"r{}\" bitsize=\"32\" regnum=\"{}\"/>\n", i, i);
    }
    xml += "    <reg name=\"sp\" bitsize=\"32\" type=\"data_ptr\" regnum=\"13\"/>\n"
           "    <reg name=\"lr\" bitsize=\"32\" type=\"code_ptr\" regnum=\"14\"/>\n"
           "    <reg name=\"pc\" bitsize=\"32\" type=\"code_ptr\" regnum=\"15\"/>\n"
           "    <reg name=\"cpsr\" bitsize=\"32\" regnum=\"16\"/>\n"
           "  </feature>\n"
           "  <feature name=\"org.aemu.emu32.extra\">\n";
    for (unsigned i = kRegisterFirstExtra; i < kRegisterCount; i++)
    {
        xml += std::format("    <reg name=\"x{}\" bitsize=\"32\" regnum=\"{}\"/>\n",
                           i - kRegisterFirstExtra + 13, i);
    }
    xml += "  </feature>\n"
           "</target>\n";
    return xml;
}

bool GdbServer::read_register(const unsigned number, word &value)
{
    if (const int reg = emulator_register(number); reg >= 0)
    {
        value = m_emu.read_reg(U8(reg));
    }
    else if (number == kRegisterPc)
    {
        value = m_emu.get_pc();
    }
    else if (number == kRegisterCpsr)
    {
        // N Z C V in bits 31-28 like ARM, and the mode in bits 4-0 (user 0b10000, supervisor
        // 0b10011), so that gdb shows the flags; the mask of the IRQs is bit 7 (I).
        const NZCVFlags flags = m_emu.get_NZCV();
        value = (word(flags.n) << 31) | (word(flags.z) << 30) | (word(flags.c) << 29)
                | (word(flags.v) << 28) | (m_emu.user_mode() ? 0x10 : 0x13)
                | (test_bit<kIrqMaskBit>(m_emu.get_pstate()) ? 0x80 : 0);
    }
    else
    {
        return false;
    }
    return true;
}

bool GdbServer::write_register(const unsigned number, const word value)
{
    if (const int reg = emulator_register(number); reg >= 0)
    {
        m_emu.write_reg(U8(reg), value);
    }
    else if (number == kRegisterPc)
    {
        if (value % 4 != 0) return false; // the pc of the machine is always a multiple of 4
        m_emu.set_pc(value);
    }
    else if (number == kRegisterCpsr)
    {
        // Only the flags: the mode and the mask are not changed from outside.
        m_emu.set_NZCV(value >> 31 & 1, value >> 30 & 1, value >> 29 & 1, value >> 28 & 1);
    }
    else
    {
        return false;
    }
    return true;
}

std::string GdbServer::read_registers()
{
    std::string text;
    for (unsigned i = 0; i < kRegisterCount; i++)
    {
        word value = 0;
        read_register(i, value);
        text += word_to_hex(value);
    }
    return text;
}

bool GdbServer::write_registers(const std::string &hex)
{
    if (hex.size() != kRegisterCount * 8) return false;
    for (unsigned i = 0; i < kRegisterCount; i++)
    {
        word value = 0;
        if (!hex_to_word(hex.substr(i * 8, 8), value) || !write_register(i, value)) return false;
    }
    return true;
}

std::string GdbServer::read_memory(const word address, const word length)
{
    if (length > kMaxTransfer) return "E22";
    std::string bytes;
    try
    {
        for (word i = 0; i < length; i++)
        {
            bytes += char(m_emu.memory.read_byte(address + i));
        }
    }
    catch (const std::exception &)
    {
        // What was read before the address that failed is still the answer, if there is any.
        if (bytes.empty()) return "E01";
    }
    return to_hex(bytes);
}

bool GdbServer::write_memory(const word address, const std::string &bytes)
{
    try
    {
        for (size_t i = 0; i < bytes.size(); i++)
        {
            m_emu.memory.write_byte(address + word(i), U8(bytes[i]));
        }
    }
    catch (const std::exception &)
    {
        return false;
    }
    return true;
}

std::string GdbServer::breakpoint_packet(const std::string &packet, const bool insert)
{
    // Z<type>,<address>,<kind>
    const std::string rest = packet.substr(1);
    std::string type_text, tail, address_text, kind_text;
    if (!split_comma(rest, type_text, tail) || !split_comma(tail, address_text, kind_text))
    {
        return "E01";
    }
    // The kind may be followed by conditions or commands (";X..."): not supported, ignored.
    kind_text = kind_text.substr(0, kind_text.find(';'));

    word address = 0, kind = 0;
    if (type_text.size() != 1 || !parse_hex(address_text, address) || !parse_hex(kind_text, kind))
    {
        return "E01";
    }

    switch (type_text[0])
    {
    case '0': // software breakpoint
    case '1': // hardware breakpoint
        if (insert)
        {
            m_emu.add_breakpoint(address);
        }
        else
        {
            m_emu.remove_breakpoint(address);
        }
        return "OK";
    case '2':
    case '3':
    case '4':
        if (kind == 0) return "E01"; // the length of the watched range
        if (insert)
        {
            // gdb takes an error as: no more hardware watchpoints
            if (!m_emu.add_watchpoint(address, kind,
                                      type_text[0] == '2'   ? Emulator32bit::WatchKind::WRITE
                                      : type_text[0] == '3' ? Emulator32bit::WatchKind::READ
                                                            : Emulator32bit::WatchKind::ACCESS))
            {
                return "E02";
            }
        }
        else
        {
            m_emu.remove_watchpoint(address);
        }
        return "OK";
    default:
        return ""; // not supported
    }
}

std::string GdbServer::stop_reply(const Emulator32bit::RunResult &result)
{
    using Status = Emulator32bit::RunResult::Status;
    switch (result.status)
    {
    case Status::HALTED:
        m_finished = false; // the client says when it is over, with a detach or a kill
        return "W00";
    case Status::FAULT:
        return "T0bthread:1;"; // SIGSEGV
    case Status::BREAKPOINT:
    {
        if (result.message.rfind("Watchpoint", 0) == 0)
        {
            // gdb needs the kind of the watchpoint and the address that was accessed to know
            // which of its watchpoints it is.
            const Emulator32bit::WatchHit &hit = m_emu.last_watch_hit();
            const char *kind = hit.kind == Emulator32bit::WatchKind::WRITE  ? "watch"
                               : hit.kind == Emulator32bit::WatchKind::READ ? "rwatch"
                                                                            : "awatch";
            m_after_watch_hit = true;
            return std::format("T05thread:1;{}:{:x};", kind, hit.address);
        }
        const bool is_breakpoint = result.message.rfind("Breakpoint", 0) == 0;
        return std::string("T05thread:1;") + (is_breakpoint ? "swbreak:;" : "");
    }
    case Status::LIMIT_REACHED:
    default:
        return "T05thread:1;";
    }
}

std::string GdbServer::resume(const U64 steps)
{
    const bool absorb = m_after_watch_hit && steps == 1;
    m_after_watch_hit = false;
    if (absorb)
    {
        return m_last_stop = "T05thread:1;";
    }

    if (steps != 0)
    {
        m_last_stop = stop_reply(m_emu.run(steps));
        return m_last_stop;
    }

    for (;;)
    {
        const Emulator32bit::RunResult result = m_emu.run(kSlice);
        if (result.status != Emulator32bit::RunResult::Status::LIMIT_REACHED)
        {
            m_last_stop = stop_reply(result);
            return m_last_stop;
        }
        // run () does not stop at a breakpoint on the first instruction of a call, which is right
        // for the first slice (the program is at the breakpoint it was stopped at) and wrong for
        // the others.
        if (m_emu.breakpoints().contains(m_emu.get_pc()))
        {
            m_last_stop = "T05thread:1;swbreak:;";
            return m_last_stop;
        }
        if (m_interrupt != nullptr && m_interrupt->interrupted())
        {
            m_last_stop = "T02thread:1;"; // SIGINT
            return m_last_stop;
        }
    }
}

std::string GdbServer::monitor(const std::string &command)
{
    // `monitor disasm [address [count]]`: gdb disassembles with the architecture it believes in,
    // which is not this one.
    if (command.rfind("disasm", 0) == 0)
    {
        word address = m_emu.get_pc();
        word count = 8;
        const size_t first = command.find(' ');
        if (first != std::string::npos)
        {
            char *end = nullptr;
            const std::string rest = command.substr(first + 1);
            address = word(std::strtoul(rest.c_str(), &end, 0));
            if (end != nullptr && *end == ' ')
            {
                count = std::clamp<word>(word(std::strtoul(end + 1, nullptr, 0)), 1, 256);
            }
        }
        std::string text;
        for (word i = 0; i < count; i++, address += 4)
        {
            try
            {
                text += std::format("{:#010x}: {}\n", address,
                                    Emulator32bit::disassemble_instr(m_emu.memory.read_word(address)));
            }
            catch (const std::exception &)
            {
                text += std::format("{:#010x}: <unreadable>\n", address);
                break;
            }
        }
        return text;
    }
    return "monitor commands: disasm [address [count]]\n";
}

std::string GdbServer::query(const std::string &packet)
{
    if (packet.rfind("qSupported", 0) == 0)
    {
        return "PacketSize=4000;qXfer:features:read+;swbreak+;hwbreak+;vContSupported+";
    }
    if (packet.rfind("qXfer:features:read:target.xml:", 0) == 0)
    {
        std::string offset_text, length_text;
        word offset = 0, length = 0;
        if (!split_comma(packet.substr(std::string("qXfer:features:read:target.xml:").size()),
                         offset_text, length_text)
            || !parse_hex(offset_text, offset) || !parse_hex(length_text, length))
        {
            return "E01";
        }
        const std::string xml = target_description();
        if (offset >= xml.size()) return "l";
        const std::string chunk = xml.substr(offset, length);
        return (offset + chunk.size() >= xml.size() ? "l" : "m") + chunk;
    }
    if (packet.rfind("qRcmd,", 0) == 0)
    {
        std::string command;
        if (!from_hex(packet.substr(6), command)) return "E01";
        return to_hex(monitor(command));
    }
    if (packet == "qAttached") return "1";
    if (packet == "qC") return "QC1";
    if (packet == "qfThreadInfo") return "m1";
    if (packet == "qsThreadInfo") return "l";
    if (packet == "qOffsets") return "Text=0;Data=0;Bss=0";
    return "";
}

std::string GdbServer::handle(const std::string &packet)
{
    m_silent = false;
    if (packet.empty())
    {
        return "";
    }

    switch (packet[0])
    {
    case '?':
        return m_last_stop;
    case 'g':
        return read_registers();
    case 'G':
        return write_registers(packet.substr(1)) ? "OK" : "E01";
    case 'p':
    {
        word number = 0, value = 0;
        if (!parse_hex(packet.substr(1), number) || !read_register(number, value)) return "E01";
        return word_to_hex(value);
    }
    case 'P':
    {
        // P<register>=<value>
        const size_t equals = packet.find('=');
        if (equals == std::string::npos) return "E01";
        const std::string number_text = packet.substr(1, equals - 1);
        const std::string value_text = packet.substr(equals + 1);
        word number = 0, value = 0;
        if (!parse_hex(number_text, number) || !hex_to_word(value_text, value)
            || !write_register(number, value))
        {
            return "E01";
        }
        return "OK";
    }
    case 'm':
    {
        std::string address_text, length_text;
        word address = 0, length = 0;
        if (!split_comma(packet.substr(1), address_text, length_text)
            || !parse_hex(address_text, address) || !parse_hex(length_text, length))
        {
            return "E01";
        }
        return read_memory(address, length);
    }
    case 'M':
    {
        const size_t colon = packet.find(':');
        std::string address_text, length_text, bytes;
        word address = 0, length = 0;
        if (colon == std::string::npos
            || !split_comma(packet.substr(1, colon - 1), address_text, length_text)
            || !parse_hex(address_text, address) || !parse_hex(length_text, length)
            || !from_hex(packet.substr(colon + 1), bytes) || bytes.size() != length)
        {
            return "E01";
        }
        return write_memory(address, bytes) ? "OK" : "E01";
    }
    case 'c':
    {
        word address = 0;
        if (packet.size() > 1 && parse_hex(packet.substr(1), address) && address % 4 == 0)
        {
            m_emu.set_pc(address);
        }
        return resume(0);
    }
    case 's':
    {
        word address = 0;
        if (packet.size() > 1 && parse_hex(packet.substr(1), address) && address % 4 == 0)
        {
            m_emu.set_pc(address);
        }
        return resume(1);
    }
    case 'v':
        if (packet == "vCont?") return "vCont;c;C;s;S";
        if (packet.rfind("vCont;", 0) == 0)
        {
            const char action = packet.size() > 6 ? packet[6] : 0;
            if (action == 'c' || action == 'C') return resume(0);
            if (action == 's' || action == 'S') return resume(1);
            return "E01";
        }
        if (packet.rfind("vKill", 0) == 0)
        {
            m_finished = true;
            return "OK";
        }
        return ""; // vMustReplyEmpty and the others
    case 'H':
        return "OK"; // one thread
    case 'T':
        return "OK";
    case 'Z':
        return breakpoint_packet(packet, true);
    case 'z':
        return breakpoint_packet(packet, false);
    case 'q':
        return query(packet);
    case 'D':
        m_finished = true;
        return "OK";
    case 'k':
        m_finished = true;
        m_silent = true;
        return "";
    default:
        return ""; // not supported
    }
}

#ifdef AEMU_GDB_SOCKETS

namespace
{

/// A client on a socket: reads the packets and says whether it sent Ctrl-C.
class Client : public GdbServer::InterruptSource
{
  public:
    explicit Client(const int fd) :
        m_fd(fd)
    {
    }

    ~Client() override
    {
        close(m_fd);
    }

    Client(const Client &) = delete;
    Client &operator=(const Client &) = delete;

    bool interrupted() override
    {
        pollfd descriptor{.fd = m_fd, .events = POLLIN, .revents = 0};
        if (poll(&descriptor, 1, 0) <= 0 || !(descriptor.revents & POLLIN))
        {
            return false;
        }
        char c = 0;
        if (recv(m_fd, &c, 1, MSG_PEEK) != 1) return false;
        if (c != 0x03) return false; // a packet of the next command: left for the loop to read
        recv(m_fd, &c, 1, 0);
        return true;
    }

    /// @return false when the client hung up
    bool read_byte(char &c)
    {
        return recv(m_fd, &c, 1, 0) == 1;
    }

    void send(const std::string &text)
    {
        size_t sent = 0;
        while (sent < text.size())
        {
            const ssize_t n = ::send(m_fd, text.data() + sent, text.size() - sent, MSG_NOSIGNAL);
            if (n <= 0) return;
            sent += size_t(n);
        }
    }

  private:
    int m_fd;
};

unsigned checksum(const std::string &payload)
{
    unsigned sum = 0;
    for (const char c : payload)
    {
        sum += U8(c);
    }
    return sum & 0xFF;
}

} // namespace

bool GdbServer::serve(Emulator32bit &emu, const std::uint16_t port,
                      void (*ready)(std::uint16_t port), std::ostream *log)
{
    const int listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) return false;
    const int yes = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port);
    socklen_t length = sizeof(address);
    if (bind(listener, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0
        || listen(listener, 1) != 0
        || getsockname(listener, reinterpret_cast<sockaddr *>(&address), &length) != 0)
    {
        close(listener);
        return false;
    }
    if (ready != nullptr) ready(ntohs(address.sin_port));

    const int fd = accept(listener, nullptr, nullptr);
    close(listener);
    if (fd < 0) return false;
    if (log != nullptr) *log << "gdb connected\n";

    Client client(fd);
    GdbServer server(emu);
    server.set_interrupt_source(&client);

    // $payload#checksum, acknowledged with + or -; a byte of 0x03 outside of a packet is Ctrl-C.
    while (!server.finished())
    {
        char c = 0;
        if (!client.read_byte(c)) break;
        if (c == 0x03)
        {
            client.send("+");
            client.send(std::format("$T02thread:1;#{:02x}", checksum("T02thread:1;")));
            continue;
        }
        if (c != '$') continue; // an acknowledgement, or noise

        std::string payload;
        while (client.read_byte(c) && c != '#')
        {
            payload += c;
        }
        char high = 0, low = 0;
        if (!client.read_byte(high) || !client.read_byte(low)) break;
        if (hex_digit(high) < 0 || hex_digit(low) < 0
            || unsigned(hex_digit(high) * 16 + hex_digit(low)) != checksum(payload))
        {
            client.send("-");
            continue;
        }
        client.send("+");

        const std::string reply = server.handle(payload);
        if (!server.silent())
        {
            client.send(std::format("${}#{:02x}", reply, checksum(reply)));
        }
    }
    if (log != nullptr) *log << "gdb disconnected\n";
    return true;
}

#else

bool GdbServer::serve(Emulator32bit &, std::uint16_t, void (*)(std::uint16_t), std::ostream *log)
{
    if (log != nullptr) *log << "The gdb server needs sockets, which this build does not have\n";
    return false;
}

#endif
