// Tests for the load/store instructions: ldr ldrh ldrb str strh strb, and the unaligned forms ldur
// ldurh stur sturh.
//
// RAM covers addresses [0, 0x2000). The instruction under test lives at address 0, and
// [kPatternBegin, kPatternEnd) is filled with a position-dependent byte pattern. Loads are checked
// against that pattern, and stores are checked by comparing a window of bytes around the target
// with the pattern plus the stored bytes. Every op goes through the same addressing-mode sweep.

#include "emulator32bit_test/emulator32bit_test.h"

#include <vector>

namespace
{
using AddrType = Emulator32bit::AddrType;

struct MemOp
{
    const char *name;
    U8 opcode;
    unsigned width;    // bytes
    bool is_load;
    bool has_sign_bit; // ldrb/ldrh/strb/strh read bit 25 ("sign"); ldr/str do not
};

constexpr MemOp kMemOps[] = {
    {"ldr", Emulator32bit::_op_ldr, 4, true, false},
    {"ldrh", Emulator32bit::_op_ldrh, 2, true, true},
    {"ldrb", Emulator32bit::_op_ldrb, 1, true, true},
    {"str", Emulator32bit::_op_str, 4, false, false},
    {"strh", Emulator32bit::_op_strh, 2, false, true},
    {"strb", Emulator32bit::_op_strb, 1, false, true},
};

constexpr AddrType kModes[] = {AddrType::ADDR_OFFSET, AddrType::ADDR_PRE_INC,
                               AddrType::ADDR_POST_INC};

const char *mode_name(const AddrType m)
{
    switch (m)
    {
    case AddrType::ADDR_OFFSET:
        return "offset";
    case AddrType::ADDR_PRE_INC:
        return "pre-index";
    case AddrType::ADDR_POST_INC:
        return "post-index";
    case AddrType::ADDR_UNALIGNED:
        return "unaligned";
    }
    return "?";
}

constexpr word kPatternBegin = 0x400;
constexpr word kPatternEnd = 0x1C00;
constexpr word kBase = 0x1000; // value of the base register; every access lands inside the pattern

byte pattern(const word addr)
{
    return byte(addr * 131u + (addr >> 8) * 17u + 7u);
}

/// What a load of `width` bytes at `addr` must return, from the pattern, little endian.
word pattern_word(const word addr, const unsigned width)
{
    word v = 0;
    for (unsigned i = 0; i < width; ++i) v |= word(pattern(addr + i)) << (8 * i);
    return v;
}

/// One way of forming the address: base register + (simm12 | xm <shift> #amount), with a mode.
struct Access
{
    AddrType mode;
    bool reg_offset;
    int simm;
    word xm;
    ShiftType shift;
    unsigned amount;

    word offset() const
    {
        return reg_offset ? ref_shift(xm, shift, amount) : word(simm);
    }

    std::string str() const
    {
        std::string s = std::string("[") + mode_name(mode) + ", ";
        if (reg_offset)
            s += "xm=" + hex32(xm) + " " + shift_name(shift) + " #" + std::to_string(amount);
        else s += "#" + std::to_string(simm);
        return s + "]";
    }
};

std::vector<Access> accesses_for_width(const unsigned w)
{
    // Immediate offsets keep the address aligned to the access width. The extremes of the signed
    // 12-bit field are the most negative offset and the largest one that stays aligned.
    const int offsets[] = {0, int(w), -int(w), int(3 * w), -int(3 * w), int(2048 - w), -2048};

    struct RegOffset
    {
        word xm;
        ShiftType shift;
        unsigned amount;
    };

    const RegOffset reg_offsets[] = {
        {0, ShiftType::SHIFT_LSL, 0},          {3 * w, ShiftType::SHIFT_LSL, 0},
        {3, ShiftType::SHIFT_LSL, 2},          // +12
        {0x40, ShiftType::SHIFT_LSR, 2},       // +16
        {0xFFFFFFC0, ShiftType::SHIFT_ASR, 3}, // -8, the sign bit is replicated
        {0x40, ShiftType::SHIFT_ROR, 4},       // +4
        {0xFFFFFFF0, ShiftType::SHIFT_LSL, 0}, // -16 as a wrapped unsigned register value
        {0xFFFFFFFC, ShiftType::SHIFT_LSL, 1}, // -8
    };

    std::vector<Access> accesses;
    for (const AddrType mode : kModes)
    {
        for (const int off : offsets)
            accesses.push_back({mode, false, off, 0, ShiftType::SHIFT_LSL, 0});
        for (const RegOffset &r : reg_offsets)
            accesses.push_back({mode, true, 0, r.xm, r.shift, r.amount});
    }
    return accesses;
}

class MemoryTest : public EmulatorFixture
{
  protected:
    static constexpr U8 kXn = 1;
    static constexpr U8 kXm = 2;
    static constexpr U8 kXt = 3;
    static constexpr U8 kXzr = U8(Register::XZR);
    static constexpr word kStoreValue = 0xCAFEF00D;

    void SetUp() override
    {
        reset(2, 0, nullptr, 0, 2);
        for (word a = kPatternBegin; a < kPatternEnd; ++a) cpu.memory.write_byte(a, pattern(a));
    }

    word encode(const MemOp &op, const bool sign, const U8 xt, const U8 xn, const Access &a)
    {
        return a.reg_offset ? Emulator32bit::asm_format_m(op.opcode, sign, xt, xn, kXm, a.shift,
                                                          a.amount, a.mode)
                            : Emulator32bit::asm_format_m(op.opcode, sign, xt, xn, a.simm, a.mode);
    }

    /// Runs one access from base register kBase and checks the data, the base register
    /// write-back, every other register, the flags and the memory around the target.
    void check(const MemOp &op, const Access &a, const bool sign, const unsigned case_id)
    {
        fill_registers();
        cpu.write_reg(kXn, kBase);
        cpu.write_reg(kXm, a.xm);
        if (!op.is_load) cpu.write_reg(kXt, kStoreValue);
        const NZCVFlags in = flags_from_bits(case_id % 16);
        set_flags(in);

        const word offset = a.offset();
        const word addr = a.mode == AddrType::ADDR_POST_INC ? kBase : kBase + offset;
        const word base_after = a.mode == AddrType::ADDR_OFFSET ? kBase : kBase + offset;
        const auto before = snapshot_registers();
        const std::string ctx =
            std::string(op.name) + (sign ? " (sign)" : "") + " " + a.str() + " addr=" + hex32(addr);

        execute(encode(op, sign, kXt, kXn, a));

        EXPECT_EQ(cpu.read_reg(kXn), base_after) << ctx << ": base register";
        EXPECT_EQ(flags(), in) << ctx << ": memory instructions must not touch the flags";

        if (op.is_load)
        {
            word expected = pattern_word(addr, op.width);
            if (sign && op.width == 1) expected = word(sword(S8(expected)));
            if (sign && op.width == 2) expected = word(sword(S16(expected)));
            EXPECT_EQ(cpu.read_reg(kXt), expected) << ctx << ": loaded value";
            expect_registers_unchanged_except(before, {kXt, kXn}, ctx);
        }
        else
        {
            EXPECT_EQ(cpu.read_reg(kXt), kStoreValue) << ctx << ": source register";
            expect_registers_unchanged_except(before, {kXn}, ctx);
        }

        // Compare a window around the target against pattern + stored bytes, then restore it.
        for (word i = addr - 8; i < addr + 16; ++i)
        {
            byte expected = pattern(i);
            if (!op.is_load && i >= addr && i < addr + op.width)
                expected = byte(kStoreValue >> (8 * (i - addr)));
            EXPECT_EQ(cpu.memory.read_byte(i), expected) << ctx << ": memory at " << hex32(i);
            cpu.memory.write_byte(i, pattern(i));
        }
    }
};
} // namespace

TEST_F(MemoryTest, addressing_modes)
{
    for (const MemOp &op : kMemOps)
    {
        unsigned case_id = 0;
        for (const Access &a : accesses_for_width(op.width))
            for (const bool sign :
                 op.has_sign_bit ? std::vector<bool>{false, true} : std::vector<bool>{false})
                check(op, a, sign, case_id++);
    }
}

TEST_F(MemoryTest, loads_are_little_endian_and_zero_extended)
{
    const word addr = 0x800;
    const byte bytes[] = {0x78, 0x56, 0x34, 0x12, 0xF0, 0xDE, 0xBC, 0x9A};
    for (unsigned i = 0; i < sizeof(bytes); ++i) cpu.memory.write_byte(addr + i, bytes[i]);

    struct Case
    {
        const char *name;
        U8 opcode;
        int offset;
        word expected;
    };

    const Case kCases[] = {
        {"ldr", Emulator32bit::_op_ldr, 0, 0x12345678},
        {"ldr", Emulator32bit::_op_ldr, 4, 0x9ABCDEF0},
        {"ldrh", Emulator32bit::_op_ldrh, 0, 0x5678},
        {"ldrh", Emulator32bit::_op_ldrh, 2, 0x1234},
        {"ldrh", Emulator32bit::_op_ldrh, 4, 0xDEF0}, // bit 15 set, not sign extended
        {"ldrb", Emulator32bit::_op_ldrb, 0, 0x78},
        {"ldrb", Emulator32bit::_op_ldrb, 3, 0x12},
        {"ldrb", Emulator32bit::_op_ldrb, 4, 0xF0},   // bit 7 set, not sign extended
    };
    for (const Case &c : kCases)
    {
        fill_registers();
        cpu.write_reg(kXn, addr);
        execute(Emulator32bit::asm_format_m(c.opcode, false, kXt, kXn, c.offset,
                                            AddrType::ADDR_OFFSET));
        EXPECT_EQ(cpu.read_reg(kXt), c.expected) << c.name << " #" << c.offset;
    }
}

TEST_F(MemoryTest, signed_loads_sign_extend)
{
    const word addr = 0x800;
    const byte bytes[] = {0x7F, 0x80, 0xFF, 0x00, 0x00, 0x80, 0xFF, 0x7F};
    for (unsigned i = 0; i < sizeof(bytes); ++i) cpu.memory.write_byte(addr + i, bytes[i]);

    struct Case
    {
        U8 opcode;
        int offset;
        word expected;
    };

    const Case kCases[] = {
        {Emulator32bit::_op_ldrb, 0, 0x0000007F}, // largest positive byte
        {Emulator32bit::_op_ldrb, 1, 0xFFFFFF80}, // smallest negative byte
        {Emulator32bit::_op_ldrb, 2, 0xFFFFFFFF}, {Emulator32bit::_op_ldrb, 3, 0x00000000},
        {Emulator32bit::_op_ldrh, 4, 0xFFFF8000}, // smallest negative halfword
        {Emulator32bit::_op_ldrh, 6, 0x00007FFF}, // largest positive halfword
        {Emulator32bit::_op_ldrh, 2, 0x000000FF}, // 0x00FF: bit 15 clear
    };
    for (const Case &c : kCases)
    {
        fill_registers();
        cpu.write_reg(kXn, addr);
        execute(
            Emulator32bit::asm_format_m(c.opcode, true, kXt, kXn, c.offset, AddrType::ADDR_OFFSET));
        EXPECT_EQ(cpu.read_reg(kXt), c.expected) << "opcode " << int(c.opcode) << " #" << c.offset;
    }
}

TEST_F(MemoryTest, narrow_stores_write_only_the_low_bytes)
{
    const word addr = 0x800;
    for (const bool sign : {false, true})
    {
        // Pre-fill with a sentinel so that extra bytes written would be seen.
        const auto fill = [&]
        {
            for (word i = 0; i < 8; ++i) cpu.memory.write_byte(addr + i, 0xEE);
        };
        const auto bytes_at = [&](word offset, unsigned n)
        {
            word v = 0;
            for (unsigned i = 0; i < n; ++i)
                v |= word(cpu.memory.read_byte(addr + offset + i)) << (8 * i);
            return v;
        };

        // The sign bit makes no difference to what is stored.
        fill_registers();
        cpu.write_reg(kXn, addr);
        cpu.write_reg(kXt, 0x8899AABB);

        fill();
        execute(Emulator32bit::asm_format_m(Emulator32bit::_op_strb, sign, kXt, kXn, 2,
                                            AddrType::ADDR_OFFSET));
        EXPECT_EQ(bytes_at(0, 4), 0xEEBBEEEEu) << "strb, sign=" << sign;
        EXPECT_EQ(bytes_at(4, 4), 0xEEEEEEEEu) << "strb, sign=" << sign;

        fill();
        execute(Emulator32bit::asm_format_m(Emulator32bit::_op_strh, sign, kXt, kXn, 2,
                                            AddrType::ADDR_OFFSET));
        EXPECT_EQ(bytes_at(0, 4), 0xAABBEEEEu) << "strh, sign=" << sign;
        EXPECT_EQ(bytes_at(4, 4), 0xEEEEEEEEu) << "strh, sign=" << sign;

        fill();
        execute(Emulator32bit::asm_format_m(Emulator32bit::_op_str, sign, kXt, kXn, 4,
                                            AddrType::ADDR_OFFSET));
        EXPECT_EQ(bytes_at(0, 4), 0xEEEEEEEEu) << "str, sign=" << sign;
        EXPECT_EQ(bytes_at(4, 4), 0x8899AABBu) << "str, sign=" << sign;
    }
}

TEST_F(MemoryTest, pre_index_writes_back_before_the_access_and_post_index_after)
{
    const word addr = kBase;
    cpu.memory.write_word(addr, 0x11111111);
    cpu.memory.write_word(addr + 8, 0x22222222);

    fill_registers();
    cpu.write_reg(kXn, addr);
    execute(Emulator32bit::asm_format_m(Emulator32bit::_op_ldr, false, kXt, kXn, 8,
                                        AddrType::ADDR_PRE_INC));
    EXPECT_EQ(cpu.read_reg(kXt), 0x22222222u) << "pre-index reads from base + offset";
    EXPECT_EQ(cpu.read_reg(kXn), addr + 8);

    fill_registers();
    cpu.write_reg(kXn, addr);
    execute(Emulator32bit::asm_format_m(Emulator32bit::_op_ldr, false, kXt, kXn, 8,
                                        AddrType::ADDR_POST_INC));
    EXPECT_EQ(cpu.read_reg(kXt), 0x11111111u) << "post-index reads from the original base";
    EXPECT_EQ(cpu.read_reg(kXn), addr + 8);

    // A negative post-index offset: `ldr x3, [x1], #-8`.
    fill_registers();
    cpu.write_reg(kXn, addr + 8);
    execute(Emulator32bit::asm_format_m(Emulator32bit::_op_ldr, false, kXt, kXn, -8,
                                        AddrType::ADDR_POST_INC));
    EXPECT_EQ(cpu.read_reg(kXt), 0x22222222u);
    EXPECT_EQ(cpu.read_reg(kXn), addr);
}

TEST_F(MemoryTest, transfer_register_may_be_the_base_register)
{
    // Stores read xt before the writeback, so they store the original base. Loads write xt last,
    // so the loaded value replaces the writeback.
    for (const MemOp &op : kMemOps)
        for (const AddrType mode : {AddrType::ADDR_PRE_INC, AddrType::ADDR_POST_INC})
        {
            const std::string ctx = std::string(op.name) + " " + mode_name(mode);
            const word target = mode == AddrType::ADDR_PRE_INC ? kBase + 8 : kBase;

            fill_registers();
            cpu.write_reg(kXn, kBase);
            cpu.memory.write_word(target, 0xCAFEBABE);
            execute(Emulator32bit::asm_format_m(op.opcode, false, kXn, kXn, 8, mode));

            if (op.is_load)
            {
                const word mask = op.width == 4 ? 0xFFFFFFFFu : (1u << (8 * op.width)) - 1;
                EXPECT_EQ(cpu.read_reg(kXn), 0xCAFEBABEu & mask) << ctx;
            }
            else
            {
                const word mask = op.width == 4 ? 0xFFFFFFFFu : (1u << (8 * op.width)) - 1;
                EXPECT_EQ(cpu.read_reg(kXn), kBase + 8) << ctx << ": base still written back";
                EXPECT_EQ(cpu.memory.read_word(target) & mask, kBase & mask)
                    << ctx << ": stores the original base";
            }
        }
}

TEST_F(MemoryTest, zero_register)
{
    // Base register xzr addresses absolute memory: `ldr x3, [xzr, #0x400]`.
    fill_registers();
    execute(Emulator32bit::asm_format_m(Emulator32bit::_op_ldr, false, kXt, kXzr, 0x400,
                                        AddrType::ADDR_OFFSET));
    EXPECT_EQ(cpu.read_reg(kXt), pattern_word(0x400, 4));

    // Storing xzr stores zeros.
    fill_registers();
    cpu.write_reg(kXn, kBase);
    execute(Emulator32bit::asm_format_m(Emulator32bit::_op_str, false, kXzr, kXn, 0,
                                        AddrType::ADDR_OFFSET));
    EXPECT_EQ(cpu.memory.read_word(kBase), 0u);

    // Loading into xzr discards the value.
    fill_registers();
    cpu.write_reg(kXn, kBase + 0x100);
    const auto before = snapshot_registers();
    execute(Emulator32bit::asm_format_m(Emulator32bit::_op_ldr, false, kXzr, kXn, 0,
                                        AddrType::ADDR_OFFSET));
    EXPECT_EQ(cpu.read_reg(kXzr), 0u);
    expect_registers_unchanged_except(before, {}, "ldr xzr");
}

TEST_F(MemoryTest, load_store_round_trip)
{
    for (const MemOp &op : kMemOps)
    {
        if (op.is_load) continue;
        fill_registers();
        cpu.write_reg(kXn, kBase);
        cpu.write_reg(kXt, 0x80F0C3A5);
        execute(Emulator32bit::asm_format_m(op.opcode, false, kXt, kXn, 16, AddrType::ADDR_OFFSET));

        // The matching load, unsigned, returns exactly the stored low bytes.
        const U8 load = op.width == 4   ? Emulator32bit::_op_ldr
                        : op.width == 2 ? Emulator32bit::_op_ldrh
                                        : Emulator32bit::_op_ldrb;
        cpu.write_reg(kXt, 0);
        execute(Emulator32bit::asm_format_m(load, false, kXt, kXn, 16, AddrType::ADDR_OFFSET));
        const word mask = op.width == 4 ? 0xFFFFFFFFu : (1u << (8 * op.width)) - 1;
        EXPECT_EQ(cpu.read_reg(kXt), 0x80F0C3A5u & mask) << op.name;
    }
}

TEST_F(MemoryTest, loads_from_rom)
{
    static const byte rom[kPageSize] = {9, 1, 2, 3};
    reset(1, 0, rom, 1, 1);

    fill_registers();
    cpu.write_reg(kXn, kPageSize - 4);
    execute(Emulator32bit::asm_format_m(Emulator32bit::_op_ldr, false, kXt, kXn, 4,
                                        AddrType::ADDR_OFFSET));
    EXPECT_EQ(cpu.read_reg(kXt), 0x03020109u);

    execute(Emulator32bit::asm_format_m(Emulator32bit::_op_ldrb, false, kXt, kXn, 5,
                                        AddrType::ADDR_OFFSET));
    EXPECT_EQ(cpu.read_reg(kXt), 1u);
}

TEST_F(MemoryTest, access_outside_memory_faults)
{
    constexpr word kUnmapped = 0x100000;

    fill_registers();
    cpu.write_reg(kXn, kUnmapped);
    auto result = step(0, Emulator32bit::asm_format_m(Emulator32bit::_op_ldr, false, kXt, kXn, 0,
                                                      AddrType::ADDR_OFFSET));
    EXPECT_EQ(result.status, Emulator32bit::RunResult::Status::FAULT);
    EXPECT_EQ(cpu.read_reg(kXt), 0xA5000000u | (kXt * 0x01010101u))
        << "a faulting load must not write xt";

    fill_registers();
    cpu.write_reg(kXn, kUnmapped);
    result = step(0, Emulator32bit::asm_format_m(Emulator32bit::_op_str, false, kXt, kXn, 0,
                                                 AddrType::ADDR_OFFSET));
    EXPECT_EQ(result.status, Emulator32bit::RunResult::Status::FAULT);
}

// ldr, ldrh, str and strh need an address that is a multiple of the size of the access. The
// instruction that faults does nothing: not a register (the base is not written back), not a
// byte of memory.
TEST_F(MemoryTest, an_access_that_is_not_aligned_to_its_size_faults_and_does_nothing)
{
    for (const MemOp &op : kMemOps)
    {
        if (op.width == 1) continue;

        for (const AddrType mode : kModes)
            for (unsigned skew = 1; skew < op.width; ++skew)
            {
                const std::string ctx =
                    std::string(op.name) + " " + mode_name(mode) + " skew=" + std::to_string(skew);

                // The address is the base for a post-index access, else the base plus the offset.
                fill_registers();
                cpu.write_reg(kXn, mode == AddrType::ADDR_POST_INC ? kBase + skew : kBase);
                cpu.write_reg(kXt, kStoreValue);
                const int offset = mode == AddrType::ADDR_POST_INC ? 8 : int(8 + skew);
                const auto before = snapshot_registers();

                const auto result = step(
                    0, Emulator32bit::asm_format_m(op.opcode, false, kXt, kXn, offset, mode));

                EXPECT_EQ(result.status, Emulator32bit::RunResult::Status::FAULT) << ctx;
                EXPECT_NE(result.message.find("Misaligned"), std::string::npos)
                    << ctx << ": " << result.message;
                expect_registers_unchanged_except(before, {}, ctx);
                for (word a = kBase; a < kBase + 32; ++a)
                {
                    ASSERT_EQ(cpu.memory.read_byte(a), pattern(a)) << ctx << " at " << hex32(a);
                }
            }
    }
}

// A byte is always aligned, and so is an access at a multiple of its size, which the sweep of the
// addressing modes covers for every op.
TEST_F(MemoryTest, the_message_of_a_misaligned_access_names_the_instruction_to_use)
{
    fill_registers();
    cpu.write_reg(kXn, kBase + 2);
    auto result = step(0, Emulator32bit::asm_format_m(Emulator32bit::_op_ldr, false, kXt, kXn, 0,
                                                      AddrType::ADDR_OFFSET));
    EXPECT_NE(result.message.find("ldur"), std::string::npos) << result.message;
    EXPECT_NE(result.message.find("0x00001002"), std::string::npos) << result.message;

    result = step(0, Emulator32bit::asm_format_m(Emulator32bit::_op_strh, false, kXt, kXn, 1,
                                                 AddrType::ADDR_OFFSET));
    EXPECT_NE(result.message.find("sturh"), std::string::npos) << result.message;
}

// ldur, ldurh, stur and sturh are ldr, ldrh, str and strh with the address mode ADDR_UNALIGNED:
// the same access, at any address, with a plain offset (immediate or register).
TEST_F(MemoryTest, the_unaligned_forms_access_any_address)
{
    for (const MemOp &op : kMemOps)
    {
        if (op.width == 1) continue;

        // Around a page boundary (0x1000) too: the pattern covers it.
        for (const word addr : {kBase + 1, kBase + 2, kBase + 3, kBase - 1, kBase - 2, kBase - 3,
                                kBase + 0x101})
            for (const bool reg_offset : {false, true})
                for (const bool sign : {false, true})
                {
                    if (sign && !op.has_sign_bit) continue;

                    const std::string ctx = std::string(op.name) + (sign ? " (sign)" : "")
                                            + (reg_offset ? " [xn, xm]" : " [xn, imm]")
                                            + " unaligned, addr=" + hex32(addr);
                    fill_registers();
                    cpu.write_reg(kXn, kBase);
                    cpu.write_reg(kXm, addr - kBase);
                    cpu.write_reg(kXt, kStoreValue);
                    const auto before = snapshot_registers();

                    const word instr =
                        reg_offset ? Emulator32bit::asm_format_m(op.opcode, sign, kXt, kXn, kXm,
                                                                 ShiftType::SHIFT_LSL, 0,
                                                                 AddrType::ADDR_UNALIGNED)
                                   : Emulator32bit::asm_format_m(op.opcode, sign, kXt, kXn,
                                                                 int(addr - kBase),
                                                                 AddrType::ADDR_UNALIGNED);
                    execute(instr);

                    if (op.is_load)
                    {
                        word expected = pattern_word(addr, op.width);
                        if (sign && op.width == 2) expected = word(sword(S16(expected)));
                        EXPECT_EQ(cpu.read_reg(kXt), expected) << ctx;
                        expect_registers_unchanged_except(before, {kXt}, ctx);
                    }
                    else
                    {
                        expect_registers_unchanged_except(before, {}, ctx);
                    }
                    EXPECT_EQ(cpu.read_reg(kXn), kBase) << ctx << ": no write back";

                    for (word a = addr - 8; a < addr + 16; ++a)
                    {
                        byte expected = pattern(a);
                        if (!op.is_load && a >= addr && a < addr + op.width)
                            expected = byte(kStoreValue >> (8 * (a - addr)));
                        EXPECT_EQ(cpu.memory.read_byte(a), expected) << ctx << " at " << hex32(a);
                        cpu.memory.write_byte(a, pattern(a));
                    }
                }
    }
}

// The unaligned form is a word or half-word access: a byte is never unaligned, and the mode is
// not assigned for ldrb and strb.
TEST_F(MemoryTest, the_unaligned_mode_of_a_byte_access_is_an_undefined_instruction)
{
    for (const U8 opcode : {Emulator32bit::_op_ldrb, Emulator32bit::_op_strb})
    {
        fill_registers();
        cpu.write_reg(kXn, kBase);
        const auto result = step(0, Emulator32bit::asm_format_m(opcode, false, kXt, kXn, 0,
                                                                AddrType::ADDR_UNALIGNED));
        EXPECT_EQ(result.status, Emulator32bit::RunResult::Status::FAULT);
        EXPECT_NE(result.message.find("Bad memory address mode"), std::string::npos)
            << result.message;
    }
}

// The disassembler names the unaligned forms the way the assembler writes them.
TEST_F(MemoryTest, the_unaligned_forms_disassemble_as_ldur_stur)
{
    const auto name = [](const U8 opcode, const bool sign)
    {
        return Emulator32bit::disassemble_instr(
            Emulator32bit::asm_format_m(opcode, sign, 1, 2, 4, AddrType::ADDR_UNALIGNED));
    };
    EXPECT_EQ(name(Emulator32bit::_op_ldr, false), "ldur x1, [x2, 4]");
    EXPECT_EQ(name(Emulator32bit::_op_str, false), "stur x1, [x2, 4]");
    EXPECT_EQ(name(Emulator32bit::_op_ldrh, false), "ldurh x1, [x2, 4]");
    EXPECT_EQ(name(Emulator32bit::_op_ldrh, true), "ldursh x1, [x2, 4]");
    EXPECT_EQ(name(Emulator32bit::_op_strh, false), "sturh x1, [x2, 4]");

    EXPECT_EQ(Emulator32bit::disassemble_instr(Emulator32bit::asm_format_m(
                  Emulator32bit::_op_ldr, false, 1, 2, 4, AddrType::ADDR_OFFSET)),
              "ldr x1, [x2, 4]");
}
