
#include "emulator32bit/emulator32bit.h"

#include "util/common.h"
#include "util/logger.h"

#include <format>
#include <iostream>

namespace
{

/// The longest string that a program can ask the emulator to print. A string that does not end
/// before that is a program that lost track of its memory.
constexpr word kMaxStringLength = 1 << 16;

} // namespace

void Emulator32bit::set_output(std::ostream &out)
{
    m_out = &out;
}

void Emulator32bit::set_error_output(std::ostream &err)
{
    m_err = &err;
}

// The value of the size bytes at the address, the most significant byte being the last one in memory
// for a little endian value and the first one for a big endian value.
word Emulator32bit::_emu_read_value(word mem_addr, U8 size, bool little_endian)
{
    if (size == 0 || size > sizeof(word))
    {
        throw Exception(InterruptType::BAD_INSTR,
                        "A value in memory is 1 to 4 bytes, not " + std::to_string(size));
    }

    word val = 0;
    for (U8 i = 0; i < size; i++)
    {
        const U8 index = little_endian ? size - 1 - i : i;
        val = (val << 8) + system_bus->read_byte(mem_addr + index);
    }
    return val;
}

U8 Emulator32bit::_emu_register_arg(word reg_id)
{
    if (reg_id >= kNumReg)
    {
        throw Exception(InterruptType::BAD_REG, "There is no register " + std::to_string(reg_id));
    }
    return U8(reg_id);
}

std::string Emulator32bit::_emu_read_string(word address)
{
    std::string text;
    for (word length = 0; length < kMaxStringLength; length++)
    {
        const byte c = system_bus->read_byte(address + length);
        if (c == '\0')
        {
            return text;
        }
        text += char(c);
    }

    throw Exception(InterruptType::BAD_INSTR,
                    "The string at address " + std::to_string(address) + " is not terminated");
}

void Emulator32bit::_emu_print()
{
    print();
}

void Emulator32bit::_emu_printr(U8 reg_id)
{
    *m_out << std::format("REG: {} = {:x}\n", reg_id, read_reg(reg_id));
}

void Emulator32bit::_emu_printm(word mem_addr, U8 size, bool little_endian)
{
    const word val = _emu_read_value(mem_addr, size, little_endian);
    *m_out << std::format("MEM: {:x} = {:0{}x}\n", mem_addr, val, size * 2);
}

void Emulator32bit::_emu_printp()
{
    *m_out << std::format("PSTATE: N={:d},Z={:d},C={:d},V={:d}\n", test_bit(m_pstate, kNFlagBit),
                          test_bit(m_pstate, kZFlagBit), test_bit(m_pstate, kCFlagBit),
                          test_bit(m_pstate, kVFlagBit));
}

void Emulator32bit::_emu_assertr(U8 reg_id, word min_value, word max_value)
{
    word val = read_reg(reg_id);

    if (val < min_value || val > max_value)
    {
        throw Exception(InterruptType::FAILED_ASSERT,
                        "Failed system call assertion. Expected register " + std::to_string(reg_id)
                            + " to contain a value between " + std::to_string(min_value) + " and "
                            + std::to_string(max_value) + " but it contains " + std::to_string(val)
                            + ".");
    }
}

void Emulator32bit::_emu_assertm(word mem_addr, U8 size, bool little_endian, word min_value,
                                 word max_value)
{
    const word val = _emu_read_value(mem_addr, size, little_endian);

    if (val < min_value || val > max_value)
    {
        throw Exception(InterruptType::FAILED_ASSERT,
                        "Expected value at memory address " + std::to_string(mem_addr)
                            + " to be between " + std::to_string(min_value) + " and "
                            + std::to_string(max_value) + ". Got " + std::to_string(val) + ".");
    }
}

void Emulator32bit::_emu_assertp(U8 p_state_id, bool expected_value)
{
    bool val = test_bit(m_pstate, p_state_id);

    if (val != expected_value)
    {
        throw Exception(InterruptType::FAILED_ASSERT,
                        "Failed system call assertion. Expected PSTATE "
                            + std::to_string(p_state_id) + " to be "
                            + std::to_string(expected_value) + ". Got " + std::to_string(val)
                            + ".");
    }
}

void Emulator32bit::_emu_log(word str)
{
    *m_out << _emu_read_string(str) << "\n";
}

// TODO: raise interrupt so kernel can handle
void Emulator32bit::_emu_err(word err)
{
    const std::string message = _emu_read_string(err);
    *m_err << message << "\n";
    throw Exception(InterruptType::PROGRAM_ERROR, "The program reported an error: " + message);
}

/**
 * @brief                   `swi <imm22>`. With a vector table (VBAR != 0) it raises the supervisor
 *                          call exception, except `swi 1`, which is an emulator call. Without one,
 *                          `swi` and `swi 1` are emulator calls (see docs/exceptions.md).
 *
 *                          The emulator calls: the number of the call is in the register
 *                          SYSCALL (x8), the arguments in x0-x4.
 *
 *                          In future, we need a vector table that contains various jump instructions
 *                          to handle various exceptions, and the system calls of an operating
 *                          system (https://chromium.googlesource.com/chromiumos/docs/+/master/constants/syscalls.md#arm64-64_bit)
 *                          would be implemented in the kernel, not here. That needs a linker script
 *                          that places the kernel at a fixed address, which exists now.
 *
 * | ID   | NAME         | x0               | x1            | x2              | x3        | x4        |
 * |------|--------------|------------------|---------------|-----------------|-----------|-----------|
 * | 1000 | emu_print    |                  |               |                 |           |           |
 * | 1001 | emu_printr   | byte reg_id      |               |                 |           |           |
 * | 1002 | emu_printm   | word mem_addr    | byte size     | bool little_end |           |           |
 * | 1003 | emu_printp   |                  |               |                 |           |           |
 * | 1010 | emu_assertr  | byte reg_id      | word min      | word max        |           |           |
 * | 1011 | emu_assertm  | word mem_addr    | byte size     | bool little_end | word min  | word max  |
 * | 1012 | emu_assertp  | byte p_state_id  | bool expected |                 |           |           |
 * | 1020 | emu_log      | char *str        |               |                 |           |           |
 * | 1021 | emu_error    | char *str        |               |                 |           |           |
 *
 *  - emu_print prints the registers, emu_printr one register, emu_printm a value of 1 to 4 bytes in
 *    memory and emu_printp the flags.
 *  - The assertions stop the program with a fault when the value is not within the bounds (or the
 *    flag is not the expected one).
 *  - emu_log prints a message, and emu_error prints it to the error output and stops the program
 *    with a fault.
 *  - Output goes to the streams set with set_output () and set_error_output (), by default
 *    std::cout and std::cerr.
 */
void Emulator32bit::_swi(word instr)
{
    byte cond = bitfield_unsigned(instr, 22, 4);
    AEMU_DEBUG("swi {}", cond);

    if (!check_cond(m_pstate, cond))
    {
        return;
    }

    // With a vector table installed every swi is a system call of the operating system (a
    // supervisor call exception) except `swi 1`. Without one, `swi` and `swi 1` are both the
    // emulator calls below, which is what programs without an operating system use.
    const word imm = bitfield_unsigned(instr, 0, 22);
    if (m_vbar != 0 && imm != kSwiSemihosting)
    {
        enter_exception(ExceptionClass::SUPERVISOR_CALL, imm, 0, m_pc + 4);
        return;
    }
    if (imm > kSwiSemihosting || !m_semihosting)
    {
        throw Exception(InterruptType::BAD_INSTR,
                        std::format("swi {} is not available ({})", imm,
                                    m_semihosting ? "there is no vector table to take it"
                                                  : "the emulator calls are off"),
                        kUndefinedIss_unimplemented);
    }

    // software interrupts.. perfect to add functionality to this like console print,
    // file operations, ports, etc
    word id = read_reg(Register::SYSCALL);
    word arg0 = read_reg(Register::X0);
    word arg1 = read_reg(Register::X1);
    word arg2 = read_reg(Register::X2);
    word arg3 = read_reg(Register::X3);
    word arg4 = read_reg(Register::X4);
    switch (id)
    {
    case 1000:
        _emu_print();
        break;
    case 1001:
        _emu_printr(_emu_register_arg(arg0));
        break;
    case 1002:
        _emu_printm(arg0, arg1, arg2);
        break;
    case 1003:
        _emu_printp();
        break;

    case 1010:
        _emu_assertr(_emu_register_arg(arg0), arg1, arg2);
        break;
    case 1011:
        _emu_assertm(arg0, arg1, arg2, arg3, arg4);
        break;
    case 1012:
        _emu_assertp(arg0, arg1);
        break;

    case 1020:
        _emu_log(arg0);
        break;
    case 1021:
        _emu_err(arg0);
        break;
    default:
        throw Exception(InterruptType::BAD_INSTR, "Invalid syscall number " + std::to_string(id));
    }
}
