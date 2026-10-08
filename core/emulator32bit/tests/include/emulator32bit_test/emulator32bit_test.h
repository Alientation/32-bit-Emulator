#pragma once

#include <emulator32bit/alu.h>
#include <emulator32bit/emulator32bit.h>
#include <emulator32bit/emulator32bit_util.h>
#include <gtest/gtest.h>

#include <array>
#include <iomanip>
#include <new>
#include <ostream>
#include <sstream>
#include <string>

/// Prints flags as "NzCv", where an upper case letter means the flag is set.
inline std::string flags_to_string(const NZCVFlags f)
{
    std::string s;
    s += f.n ? 'N' : 'n';
    s += f.z ? 'Z' : 'z';
    s += f.c ? 'C' : 'c';
    s += f.v ? 'V' : 'v';
    return s;
}

inline std::string hex32(const word w)
{
    std::ostringstream os;
    os << "0x" << std::hex << std::setw(8) << std::setfill('0') << w;
    return os.str();
}

/// Decodes the low four bits as N, Z, C, V (bit 3 = N). Iterate 0..15 to cover every combination.
inline NZCVFlags flags_from_bits(const unsigned bits)
{
    return {.n = bool(bits & 8), .z = bool(bits & 4), .c = bool(bits & 2), .v = bool(bits & 1)};
}

inline bool operator==(const NZCVFlags &a, const NZCVFlags &b)
{
    return a.n == b.n && a.z == b.z && a.c == b.c && a.v == b.v;
}

inline std::ostream &operator<<(std::ostream &os, const NZCVFlags &f)
{
    return os << flags_to_string(f);
}

/// Values that sit on sign, carry and overflow boundaries.
inline constexpr std::array<word, 10> kBoundaryValues = {
    0x00000000, 0x00000001, 0x00000002, 0x7FFFFFFF, 0x80000000,
    0x80000001, 0xFFFFFFFE, 0xFFFFFFFF, 0x55555555, 0xAAAAAAAA,
};

/// Immediates that sit on the edges of the unsigned imm14 field.
inline constexpr std::array<word, 6> kImm14Values = {0, 1, 2, 0x1FFF, 0x2000, 0x3FFF};

constexpr std::array<ShiftType, 4> kShiftTypes = {ShiftType::SHIFT_LSL, ShiftType::SHIFT_LSR,
                                                  ShiftType::SHIFT_ASR, ShiftType::SHIFT_ROR};

inline const char *shift_name(const ShiftType t)
{
    switch (t)
    {
    case ShiftType::SHIFT_LSL:
        return "lsl";
    case ShiftType::SHIFT_LSR:
        return "lsr";
    case ShiftType::SHIFT_ASR:
        return "asr";
    case ShiftType::SHIFT_ROR:
        return "ror";
    }
    return "?";
}

/// Reference shifter for amounts in [0, 31]. Written independently of `alu_shift`.
inline word ref_shift(const word v, const ShiftType t, const unsigned n)
{
    if (n == 0) return v;
    switch (t)
    {
    case ShiftType::SHIFT_LSL:
        return v << n;
    case ShiftType::SHIFT_LSR:
        return v >> n;
    case ShiftType::SHIFT_ASR:
        return word(S32(v) >> n);
    case ShiftType::SHIFT_ROR:
        return (v >> n) | (v << (32 - n));
    }
    return v;
}

/// Reference carry-out of a shift: the last bit shifted out, or `old_carry` for a zero amount.
inline bool ref_shift_carry(const word v, const ShiftType t, const unsigned n, const bool old_carry)
{
    if (n == 0) return old_carry;
    switch (t)
    {
    case ShiftType::SHIFT_LSL:
        return ((U64(v) << n) >> 32) & 1;
    case ShiftType::SHIFT_LSR:
        return (v >> (n - 1)) & 1;
    case ShiftType::SHIFT_ASR:
        return (S32(v) >> (n - 1)) & 1;
    case ShiftType::SHIFT_ROR:
        return ref_shift(v, t, n) >> 31;
    }
    return old_carry;
}

class EmulatorFixture : public ::testing::Test
{
  protected:
    Emulator32bit cpu{1, 0, {}, 0, 1};

    void reset(word ram_npages, word ram_start_page, const byte rom_data[], word rom_npages,
               word rom_start_page)
    {
        cpu.~Emulator32bit();
        new (&cpu) Emulator32bit{ram_npages, ram_start_page, rom_data, rom_npages, rom_start_page};
    }

    /// Writes `instr` at `addr`, points the PC at it and executes exactly one instruction.
    Emulator32bit::RunResult step(word addr, word instr)
    {
        cpu.memory.write_word(addr, instr);
        cpu.set_pc(addr);
        return cpu.run(1);
    }

    /// Executes `instr` at address 0 and expects it to retire normally.
    void execute(word instr)
    {
        const auto result = step(0, instr);
        ASSERT_EQ(result.status, Emulator32bit::RunResult::Status::LIMIT_REACHED)
            << "instruction " << hex32(instr) << " faulted: " << result.message;
    }

    void set_flags(const NZCVFlags f)
    {
        cpu.set_NZCV(f);
    }

    NZCVFlags flags()
    {
        return cpu.get_NZCV();
    }

    /// Writes every register (including xzr, which ignores it) to a recognizable value derived
    /// from its index, so that stray writes to any register are detectable.
    void fill_registers()
    {
        for (U8 r = 0; r < kNumReg; ++r) cpu.write_reg(r, 0xA5000000u | (word(r) * 0x01010101u));
    }

    std::array<word, kNumReg> snapshot_registers()
    {
        std::array<word, kNumReg> regs{};
        for (U8 r = 0; r < kNumReg; ++r) regs[r] = cpu.read_reg(r);
        return regs;
    }

    /// Expects every register other than those in `except` to equal its value in `before`.
    void expect_registers_unchanged_except(const std::array<word, kNumReg> &before,
                                           std::initializer_list<U8> except,
                                           const std::string &ctx = "")
    {
        for (U8 r = 0; r < kNumReg; ++r)
        {
            bool skip = false;
            for (const U8 e : except) skip |= (e == r);
            if (skip) continue;
            EXPECT_EQ(cpu.read_reg(r), before[r]) << "x" << int(r) << " was modified; " << ctx;
        }
    }
};
