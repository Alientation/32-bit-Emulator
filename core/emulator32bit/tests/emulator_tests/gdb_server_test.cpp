// The server of the gdb remote protocol (docs/debugging.md): the packets, without a socket, and one
// client on the loopback address.

#include "emulator32bit/gdb_server.h"
#include "emulator32bit_test/emulator32bit_test.h"

#include <atomic>
#include <format>
#include <thread>

#if defined(__unix__) || defined(__APPLE__)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#define AEMU_TEST_SOCKETS 1
#endif

namespace
{

using Status = Emulator32bit::RunResult::Status;

word add_imm(const int xd, const int xn, const int imm)
{
    return Emulator32bit::asm_format_o(Emulator32bit::_op_add, false, xd, xn, imm);
}

/// x0 += 1 three times, a store of x2 to 0x100, then hlt: at 0, 4, 8, the store at 12, hlt at 16.
class GdbTest : public EmulatorFixture
{
  protected:
    GdbServer server{cpu};

    void SetUp() override
    {
        const word program[] = {
            add_imm(0, 0, 1), add_imm(0, 0, 1), add_imm(0, 0, 1),
            Emulator32bit::asm_format_m(Emulator32bit::_op_str, false, 2, 1, 0,
                                        Emulator32bit::AddrType::ADDR_OFFSET),
            Emulator32bit::asm_hlt()};
        for (size_t i = 0; i < std::size(program); i++)
        {
            cpu.memory.write_word(word(i * 4), program[i]);
        }
        cpu.set_pc(0);
        cpu.write_reg(1, 0x100);
        cpu.write_reg(2, 0xCAFE);
    }

    std::string ask(const std::string &packet)
    {
        return server.handle(packet);
    }
};

TEST_F(GdbTest, the_target_description_names_the_registers_and_is_sent_in_pieces)
{
    const std::string xml = GdbServer::target_description();
    EXPECT_NE(xml.find("org.gnu.gdb.arm.core"), std::string::npos);
    EXPECT_NE(xml.find("name=\"r12\""), std::string::npos);
    EXPECT_NE(xml.find("name=\"cpsr\""), std::string::npos);
    EXPECT_NE(xml.find("name=\"x28\""), std::string::npos);

    // 'm' says that there is more, 'l' that this is the end.
    const std::string first = ask("qXfer:features:read:target.xml:0,40");
    EXPECT_EQ(first.substr(0, 1), "m");
    EXPECT_EQ(first.size(), 0x40u + 1);
    EXPECT_EQ(ask(std::format("qXfer:features:read:target.xml:{:x},ffff", xml.size() - 4)),
              "l" + xml.substr(xml.size() - 4));
    EXPECT_EQ(ask(std::format("qXfer:features:read:target.xml:{:x},10", xml.size() + 10)), "l");
    EXPECT_EQ(ask("qXfer:features:read:target.xml:zz,10"), "E01");
}

TEST_F(GdbTest, the_queries_that_gdb_starts_with)
{
    EXPECT_NE(ask("qSupported:multiprocess+;swbreak+").find("qXfer:features:read+"),
              std::string::npos);
    EXPECT_EQ(ask("qAttached"), "1");
    EXPECT_EQ(ask("qfThreadInfo"), "m1");
    EXPECT_EQ(ask("qsThreadInfo"), "l");
    EXPECT_EQ(ask("Hg0"), "OK");
    EXPECT_EQ(ask("?"), "S05");
    EXPECT_EQ(ask("vMustReplyEmpty"), "");
    EXPECT_EQ(ask("vCont?"), "vCont;c;C;s;S");
    EXPECT_EQ(ask("QStartNoAckMode"), "") << "what is not supported is an empty reply";
    EXPECT_EQ(ask(""), "");
}

TEST_F(GdbTest, registers_are_read_and_written_in_the_order_of_the_description)
{
    cpu.write_reg(5, 0x11223344);
    cpu.write_reg(Register::SP, 0xAAAA0000);
    cpu.write_reg(Register::LR, 0xBBBB0000);
    cpu.write_reg(20, 0x55);

    const std::string all = ask("g");
    ASSERT_EQ(all.size(), 33u * 8);
    EXPECT_EQ(all.substr(5 * 8, 8), "44332211") << "r5, least significant byte first";
    EXPECT_EQ(all.substr(13 * 8, 8), "0000aaaa") << "sp";
    EXPECT_EQ(all.substr(14 * 8, 8), "0000bbbb") << "lr is x29";
    EXPECT_EQ(all.substr(17 * 8 + 7 * 8, 8), "55000000") << "x20 is the eighth of the extra ones";

    EXPECT_EQ(ask("p5"), "44332211");
    EXPECT_EQ(ask("p18"), "55000000") << "x20, register 24";
    EXPECT_EQ(ask("p0f"), "00000000") << "pc";
    EXPECT_EQ(ask("p99"), "E01");

    EXPECT_EQ(ask("P1=78563412"), "OK");
    EXPECT_EQ(cpu.read_reg(1), 0x12345678u);
    EXPECT_EQ(ask("P11=01000000"), "OK") << "register 17 is x13";
    EXPECT_EQ(cpu.read_reg(13), 1u);
    EXPECT_EQ(ask("Pf=10000000"), "OK");
    EXPECT_EQ(cpu.get_pc(), 0x10u);
    EXPECT_EQ(ask("Pf=11000000"), "E01") << "the pc is a multiple of 4";
    EXPECT_EQ(ask("P1=zz"), "E01");

    EXPECT_EQ(ask("G" + all), "OK");
    EXPECT_EQ(ask("G" + all.substr(8)), "E01") << "too short";
    EXPECT_EQ(ask("g"), all);
}

TEST_F(GdbTest, cpsr_shows_the_flags_and_the_mode_in_the_bits_of_an_arm_one)
{
    cpu.set_NZCV(true, false, true, false);
    std::string bytes;
    ASSERT_TRUE(GdbServer::from_hex(ask("p10"), bytes));
    const word cpsr = word(U8(bytes[0])) | (word(U8(bytes[1])) << 8) | (word(U8(bytes[2])) << 16)
           | (word(U8(bytes[3])) << 24);
    EXPECT_EQ(cpsr >> 28, 0b1010u) << "N and C";
    EXPECT_EQ(cpsr & 0x1F, 0x13u) << "supervisor: the machine is in kernel mode";
    EXPECT_NE(cpsr & 0x80, 0u) << "IRQs are masked after a reset";

    EXPECT_EQ(ask("P10=0000004"), "E01") << "seven digits";
    EXPECT_EQ(ask("P10=00000050"), "OK");
    EXPECT_TRUE(cpu.get_NZCV().z);
    EXPECT_TRUE(cpu.get_NZCV().v);
    EXPECT_FALSE(cpu.get_NZCV().n);
}

TEST_F(GdbTest, memory_is_read_and_written)
{
    cpu.memory.write_word(0x100, 0x04030201);
    EXPECT_EQ(ask("m100,4"), "01020304");
    EXPECT_EQ(ask("m101,2"), "0203");
    EXPECT_EQ(ask("m100,0"), "");
    EXPECT_EQ(ask("M200,3:aabbcc"), "OK");
    EXPECT_EQ(cpu.memory.read_byte(0x201), 0xBB);
    EXPECT_EQ(ask("M200,3:aabb"), "E01") << "the length is not the data";
    EXPECT_EQ(ask("m10000000,4"), "E01") << "nothing there";
    EXPECT_EQ(ask("m0,ffffff"), "E22") << "too much";
    EXPECT_EQ(ask("mzz,4"), "E01");
    EXPECT_EQ(ask("M10000000,1:00"), "E01");
}

TEST_F(GdbTest, a_breakpoint_stops_the_program_and_continue_goes_past_it)
{
    EXPECT_EQ(ask("Z0,8,4"), "OK");
    EXPECT_EQ(ask("c"), "T05thread:1;swbreak:;");
    EXPECT_EQ(cpu.get_pc(), 8u);
    EXPECT_EQ(cpu.read_reg(0), 2u);
    EXPECT_EQ(ask("?"), "T05thread:1;swbreak:;");

    EXPECT_EQ(ask("c"), "W00") << "it was stopped at the breakpoint, so it goes on to the hlt";
    EXPECT_EQ(cpu.read_reg(0), 3u);
    EXPECT_EQ(cpu.memory.read_word(0x100), 0xCAFEu);

    EXPECT_EQ(ask("z0,8,4"), "OK");
    EXPECT_TRUE(cpu.breakpoints().empty());
    EXPECT_EQ(ask("Z0,zz,4"), "E01");
    EXPECT_EQ(ask("Z9,8,4"), "") << "not a kind of breakpoint";
}

TEST_F(GdbTest, a_step_runs_one_instruction)
{
    EXPECT_EQ(ask("s"), "T05thread:1;");
    EXPECT_EQ(cpu.get_pc(), 4u);
    EXPECT_EQ(ask("vCont;s:1"), "T05thread:1;");
    EXPECT_EQ(cpu.get_pc(), 8u);
    EXPECT_EQ(ask("sc"), "T05thread:1;") << "an address to go on from: the store at 0xc";
    EXPECT_EQ(cpu.get_pc(), 0x10u);
    EXPECT_EQ(cpu.memory.read_word(0x100), 0xCAFEu);
}

TEST_F(GdbTest, a_watchpoint_stops_after_the_access)
{
    EXPECT_EQ(ask("Z2,100,4"), "OK");
    EXPECT_EQ(ask("vCont;c"), "T05thread:1;watch:100;") << "gdb is told which watchpoint";
    EXPECT_EQ(cpu.get_pc(), 0x10u) << "after the store";
    EXPECT_EQ(cpu.memory.read_word(0x100), 0xCAFEu);

    // gdb takes it that the hit comes before the instruction and steps over it: the instruction
    // has run, so that step does nothing, and the next one is a step like any other.
    EXPECT_EQ(ask("z2,100,4"), "OK");
    EXPECT_EQ(ask("s"), "T05thread:1;");
    EXPECT_EQ(cpu.get_pc(), 0x10u);
    EXPECT_EQ(ask("s"), "W00") << "the hlt";

    cpu.set_pc(0);
    EXPECT_TRUE(cpu.watchpoints().empty());
    EXPECT_EQ(ask("Z3,100,4"), "OK");
    EXPECT_EQ(cpu.watchpoints()[0].kind, Emulator32bit::WatchKind::READ);
    EXPECT_EQ(ask("Z4,100,4"), "OK");
    EXPECT_EQ(cpu.watchpoints()[0].kind, Emulator32bit::WatchKind::ACCESS);
    EXPECT_EQ(ask("Z2,100,0"), "E01") << "no length";
}

TEST_F(GdbTest, a_halt_is_an_exit_and_a_fault_is_a_signal)
{
    EXPECT_EQ(ask("c"), "W00");

    cpu.memory.write_word(0, 0xFC000000); // an opcode that is not assigned
    cpu.set_pc(0);
    EXPECT_EQ(ask("c"), "T0bthread:1;");
}

class Interrupts : public GdbServer::InterruptSource
{
  public:
    int polls = 0;

    bool interrupted() override
    {
        return ++polls == 3;
    }
};

TEST_F(GdbTest, the_client_can_interrupt_a_program_that_does_not_stop)
{
    cpu.memory.write_word(0, Emulator32bit::asm_format_b1(Emulator32bit::_op_b, ConditionCode::AL, 0));
    cpu.set_pc(0);
    Interrupts interrupts;
    server.set_interrupt_source(&interrupts);

    EXPECT_EQ(ask("c"), "T02thread:1;") << "the third look at the client";
    EXPECT_EQ(interrupts.polls, 3);
}

TEST_F(GdbTest, a_breakpoint_between_two_slices_of_a_run_is_not_missed)
{
    // 30000 instructions of a loop, with the breakpoint on the one that a slice ends before.
    cpu.memory.write_word(0, Emulator32bit::asm_format_b1(Emulator32bit::_op_b, ConditionCode::AL, 1));
    cpu.memory.write_word(4, Emulator32bit::asm_format_b1(Emulator32bit::_op_b, ConditionCode::AL, -1));
    cpu.set_pc(0);
    EXPECT_EQ(ask("Z0,4,4"), "OK");
    EXPECT_EQ(ask("c"), "T05thread:1;swbreak:;");
    EXPECT_EQ(cpu.get_pc(), 4u);
}

TEST_F(GdbTest, detach_and_kill_end_the_session)
{
    EXPECT_FALSE(server.finished());
    EXPECT_EQ(ask("D"), "OK");
    EXPECT_TRUE(server.finished());
    EXPECT_FALSE(server.silent());
}

TEST_F(GdbTest, vkill_is_answered)
{
    EXPECT_EQ(ask("vKill;1"), "OK");
    EXPECT_TRUE(server.finished());
}

TEST_F(GdbTest, kill_has_no_reply)
{
    EXPECT_EQ(ask("k"), "");
    EXPECT_TRUE(server.finished());
    EXPECT_TRUE(server.silent());
}

TEST_F(GdbTest, monitor_disasm_shows_the_instructions_in_the_syntax_of_the_assembler)
{
    const auto monitor = [&](const std::string &command)
    {
        std::string text;
        EXPECT_TRUE(GdbServer::from_hex(ask("qRcmd," + GdbServer::to_hex(command)), text));
        return text;
    };

    const std::string two = monitor("disasm 0 2");
    EXPECT_NE(two.find("0x00000000: add x0, x0, 1\n"), std::string::npos) << two;
    EXPECT_NE(two.find("0x00000004: add x0, x0, 1\n"), std::string::npos) << two;
    EXPECT_EQ(two.find("0x00000008"), std::string::npos);
    EXPECT_NE(monitor("disasm").find("0x00000000:"), std::string::npos) << "from the pc";
    EXPECT_NE(monitor("disasm 0x10000000 1").find("<unreadable>"), std::string::npos);
    EXPECT_NE(monitor("what").find("disasm"), std::string::npos) << "the list of commands";
    EXPECT_EQ(ask("qRcmd,zz"), "E01");
}

TEST(GdbHex, bytes_make_hex_and_back)
{
    EXPECT_EQ(GdbServer::to_hex(std::string("\x01\xAB\xFF", 3)), "01abff");
    std::string bytes;
    EXPECT_TRUE(GdbServer::from_hex("01aBFf", bytes));
    EXPECT_EQ(bytes, std::string("\x01\xAB\xFF", 3));
    EXPECT_FALSE(GdbServer::from_hex("0", bytes));
    EXPECT_FALSE(GdbServer::from_hex("0g", bytes));
}

#ifdef AEMU_TEST_SOCKETS

std::atomic<std::uint16_t> g_port{0};

/// A client of the loopback address.
class TestClient
{
  public:
    explicit TestClient(const std::uint16_t port)
    {
        m_fd = socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(port);
        connected = connect(m_fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0;
    }

    ~TestClient()
    {
        close(m_fd);
    }

    bool connected = false;

    /// Sends a packet and returns the payload of the reply ("" if the reply did not come).
    std::string transact(const std::string &payload)
    {
        unsigned sum = 0;
        for (const char c : payload) sum += static_cast<unsigned char>(c);
        const std::string packet = std::format("${}#{:02x}", payload, sum & 0xFF);
        send(m_fd, packet.data(), packet.size(), 0);

        std::string reply;
        char c = 0;
        while (recv(m_fd, &c, 1, 0) == 1 && c != '$') {} // the "+" first
        while (recv(m_fd, &c, 1, 0) == 1 && c != '#') reply += c;
        char checksum[2];
        recv(m_fd, checksum, 2, MSG_WAITALL);
        send(m_fd, "+", 1, 0);
        return reply;
    }

  private:
    int m_fd;
};

TEST_F(GdbTest, a_client_on_the_loopback_address_runs_the_program)
{
    g_port = 0;
    std::atomic<bool> failed{false};
    std::thread serving(
        [this, &failed]
        {
            if (!GdbServer::serve(cpu, 0, [](const std::uint16_t port) { g_port = port; }, nullptr))
            {
                failed = true;
            }
        });
    while (g_port == 0 && !failed) std::this_thread::yield();
    if (failed)
    {
        serving.join();
        FAIL() << "the server could not listen";
    }

    {
        TestClient client(g_port);
        ASSERT_TRUE(client.connected);
        EXPECT_NE(client.transact("qSupported").find("PacketSize"), std::string::npos);
        EXPECT_EQ(client.transact("Z0,8,4"), "OK");
        EXPECT_EQ(client.transact("c"), "T05thread:1;swbreak:;");
        EXPECT_EQ(client.transact("p0"), "02000000");
        EXPECT_EQ(client.transact("c"), "W00");
        EXPECT_EQ(client.transact("m100,4"), "feca0000");
        EXPECT_EQ(client.transact("D"), "OK");
    }
    serving.join();
}

#endif

} // namespace
