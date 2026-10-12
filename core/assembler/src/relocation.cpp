#include "assembler/relocation.h"

#include "emulator32bit/emulator32bit.h"
#include "util/logger.h"

word apply_relocation(const ObjectFile::RelocationEntry::Type type, const word instr,
                      const word instr_address, const word target)
{
    using Type = ObjectFile::RelocationEntry::Type;

    switch (type)
    {
    case Type::R_EMU32_O_LO12:
        return zero_bits<0, 14>(instr) + bitfield_unsigned<0, 12>(target);
    case Type::R_EMU32_ADRP_HI20:
    {
        // An adrp is relative to the page of the instruction itself.
        const S32 pages =
            S32(target >> kNumPageOffsetBits) - S32(instr_address >> kNumPageOffsetBits);
        word patched = zero_bits<0, 20>(instr) + bitfield_unsigned<0, 20>(pages);
        if ((pages >> 20) & 1)
        {
            patched = set_bit<kInstructionUpdateFlagBit>(patched, 1);
        }
        return patched;
    }
    case Type::R_EMU32_ADR_PCREL21:
    {
        // The distance in bytes from the adr itself.
        const S64 distance = S64(sword(target)) - S64(sword(instr_address));
        AEMU_CHECK(distance >= -(1 << 20) && distance < (1 << 20),
                   "Adr at {:#x} cannot reach {:#x}, the distance does not "
                   "fit in 21 bits.",
                   instr_address, target);
        word patched = zero_bits<0, 20>(instr) + bitfield_unsigned<0, 20>(sword(distance));
        if (distance < 0)
        {
            patched = set_bit<kInstructionUpdateFlagBit>(patched, 1);
        }
        return patched;
    }
    case Type::R_EMU32_MOV_LO19:
        return zero_bits<0, 19>(instr) + bitfield_unsigned<0, 19>(target);
    case Type::R_EMU32_MOV_HI13:
        return zero_bits<0, 19>(instr) + bitfield_unsigned<19, 13>(target);
    case Type::R_EMU32_B_OFFSET22:
    {
        AEMU_CHECK(target % kInstructionSize == 0,
                   "Expected the branch target to be 4 byte aligned. Got {}", target);

        // The offset in words, relative to the branch instruction itself.
        const sword delta_words = (sword(target) - sword(instr_address)) / sword(kInstructionSize);
        AEMU_CHECK(delta_words >= -(1 << 21) && delta_words < (1 << 21),
                   "Branch at {:#x} cannot reach {:#x}, the offset does not "
                   "fit in 22 bits.",
                   instr_address, target);
        return zero_bits<0, 22>(instr) + bitfield_unsigned<0, 22>(delta_words);
    }
    case Type::R_EMU32_ABS32:
        // The word is the address, plus what was in the word.
        return instr + target;
    case Type::UNDEFINED:
        break;
    }
    AEMU_FATAL("Unknown relocation entry type ({}).", S32(type));
}
