// Fuzz target of the server of the gdb remote protocol: the input is packets, one a line (the
// payload, between `$` and `#`), which GdbServer::handle answers on a machine with a small program
// at 0. Besides a crash, a sanitizer report, a failed assertion, an exception or a hang, these are
// bugs: a `G` or `P` that answers an error but changed a register, an `M` that answers OK but whose
// bytes do not read back with `m` (outside the devices), and registers that `G` does not take back
// as `g` gave them.
//
// A continue runs one slice of the server (the client always interrupts), so that a program that
// loops ends. fuzz/gdb.dict has the packets.

#include "emulator32bit/devices.h"
#include "emulator32bit/emulator32bit.h"
#include "emulator32bit/gdb_server.h"
#include "util/logger.h"

#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

namespace
{

/// The machine of the tests (assembler_test.h): 16 pages of RAM, then 16 of ROM.
constexpr word kRamPages = 16;
constexpr word kRomPages = 16;

/// The most packets of an input that are handled, so that an input of continues stays fast.
constexpr size_t kMaxPackets = 256;

/// Where x1 points: the program stores to it and loads from it.
constexpr word kData = 0x100;

/// Interrupts the program at the end of each slice of a continue.
class AlwaysInterrupt : public GdbServer::InterruptSource
{
  public:
    bool interrupted() override
    {
        return true;
    }
};

/// x0 += 1, a store of x0 and a load of it at kData, a brk, then back to the start; a hlt after
/// the loop.
void load_program(Emulator32bit &emu)
{
    const word program[] = {
        Emulator32bit::asm_format_o(encoding::_op_add, false, 0, 0, 1),
        Emulator32bit::asm_format_m(encoding::_op_str, false, 0, 1, 0,
                                    encoding::AddrType::ADDR_OFFSET),
        Emulator32bit::asm_format_m(encoding::_op_ldr, false, 2, 1, 0,
                                    encoding::AddrType::ADDR_OFFSET),
        Emulator32bit::asm_brk(0),
        Emulator32bit::asm_format_b1(encoding::_op_b, ConditionCode::AL, -4),
        Emulator32bit::asm_hlt(),
    };
    for (size_t i = 0; i < std::size(program); i++)
    {
        emu.memory.write_word(word(i * sizeof(word)), program[i]);
    }
    emu.write_reg(1, kData);
    emu.set_pc(0);
}

void fail(const std::string &what, const std::string &packet)
{
    std::cerr << "gdb: " << what << " (packet '" << packet << "')\n";
    std::abort();
}

/// Whether the bytes from the address overlap the registers of the devices.
bool touches_devices(const word address, const word length)
{
    const U64 end = U64(address) + length;
    return address < kDeviceEnd && end > kDeviceBase;
}

/// Checks what a packet that writes promised.
void check_write(GdbServer &server, const std::string &packet, const std::string &reply,
                 const std::string &registers_before)
{
    if ((packet[0] == 'G' || packet[0] == 'P') && reply != "OK"
        && server.handle("g") != registers_before)
    {
        fail("a refused register write changed the registers", packet);
    }

    if (packet[0] == 'M' && reply == "OK")
    {
        // M<address>,<length>:<bytes>, which the server parsed already.
        const size_t comma = packet.find(',');
        const size_t colon = packet.find(':');
        const word address = word(std::stoul(packet.substr(1, comma - 1), nullptr, 16));
        const word length = word(std::stoul(packet.substr(comma + 1, colon - comma - 1), nullptr, 16));
        if (length == 0 || touches_devices(address, length)) return;

        std::string written = packet.substr(colon + 1);
        for (char &c : written)
        {
            c = char(std::tolower(static_cast<unsigned char>(c)));
        }
        std::ostringstream read;
        read << 'm' << std::hex << address << ',' << length;
        if (server.handle(read.str()) != written)
        {
            fail("the bytes of an M do not read back", packet);
        }
    }
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    aemu::log::set_level(aemu::log::Level::Off);
    aemu::log::set_fatal_action(aemu::log::FatalAction::Throw);

    Emulator32bit emu(std::make_unique<RAM>(kRamPages, 0),
                      std::make_unique<ROM>(kRomPages, kRamPages), std::make_unique<MockDisk>());
    std::ostream discard(nullptr);
    emu.set_output(discard);
    emu.set_error_output(discard);
    emu.system_bus->console.set_output(nullptr);
    load_program(emu);

    GdbServer server(emu);
    AlwaysInterrupt interrupt;
    server.set_interrupt_source(&interrupt);

    std::istringstream packets(std::string(reinterpret_cast<const char *>(data), size));
    std::string packet;
    for (size_t n = 0; n < kMaxPackets && !server.finished() && std::getline(packets, packet); n++)
    {
        if (packet.empty()) continue;
        const std::string registers_before = server.handle("g");
        const std::string reply = server.handle(packet);
        check_write(server, packet, reply, registers_before);
    }

    const std::string registers = server.handle("g");
    if (server.handle("G" + registers) != "OK" || server.handle("g") != registers)
    {
        fail("the registers of g are not taken back by G", "G" + registers);
    }
    return 0;
}
