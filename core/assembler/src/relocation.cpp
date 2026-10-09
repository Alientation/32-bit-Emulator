#include "assembler/relocation.h"

#include "emulator32bit/emulator32bit.h"
#include "util/logger.h"

word apply_relocation(ObjectFile::RelocationEntry::Type type, word instr, word instr_address,
                      word target)
{
    using Type = ObjectFile::RelocationEntry::Type;

    switch (type)
    {
    case Type::R_EMU32_O_LO12:
        return mask_0(instr, 0, 14) + bitfield_unsigned(target, 0, 12);
    case Type::R_EMU32_ADRP_HI20:
    {
        /* adrp is relative to the page of the instruction itself */
        const int pages = int(target >> 12) - int(instr_address >> 12);
        word patched = mask_0(instr, 0, 20) + bitfield_unsigned(pages, 0, 20);
        if ((pages >> 20) & 1)
        {
            patched = set_bit(patched, kInstructionUpdateFlagBit, 1);
        }
        return patched;
    }
    case Type::R_EMU32_ADR_PCREL21:
    {
        /* the distance in bytes from the adr itself */
        const int64_t distance = int64_t(sword(target)) - int64_t(sword(instr_address));
        AEMU_CHECK(distance >= -(1 << 20) && distance < (1 << 20),
                   "apply_relocation() - adr at {:#x} cannot reach {:#x}, the distance does not "
                   "fit in 21 bits.",
                   instr_address, target);
        word patched = mask_0(instr, 0, 20) + bitfield_unsigned(sword(distance), 0, 20);
        if (distance < 0)
        {
            patched = set_bit(patched, kInstructionUpdateFlagBit, 1);
        }
        return patched;
    }
    case Type::R_EMU32_MOV_LO19:
        return mask_0(instr, 0, 19) + bitfield_unsigned(target, 0, 19);
    case Type::R_EMU32_MOV_HI13:
        return mask_0(instr, 0, 19) + bitfield_unsigned(target, 19, 13);
    case Type::R_EMU32_B_OFFSET22:
    {
        AEMU_CHECK((target & 0b11) == 0,
                   "apply_relocation() - Expected the branch target to be 4 byte aligned. Got {}",
                   target);

        /* offset in words, relative to the branch instruction itself */
        const sword delta_words = (sword(target) - sword(instr_address)) / 4;
        AEMU_CHECK(delta_words >= -(1 << 21) && delta_words < (1 << 21),
                   "apply_relocation() - Branch at {:#x} cannot reach {:#x}, the offset does not "
                   "fit in 22 bits.",
                   instr_address, target);
        return mask_0(instr, 0, 22) + bitfield_unsigned(delta_words, 0, 22);
    }
    case Type::R_EMU32_ABS32:
        /* The word is the address, plus what was in the word. */
        return instr + target;
    case Type::UNDEFINED:
        break;
    }
    AEMU_FATAL("apply_relocation() - Unknown relocation entry type ({}).", int(type));
}
