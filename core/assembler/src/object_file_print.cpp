// The objdump-style listing of an object file (basm -dump).

#include "assembler/object_file.h"
#include "emulator32bit/emulator32bit.h"
#include "util/types.h"

#include <algorithm>
#include <bit>
#include <format>
#include <iostream>
#include <map>
#include <unordered_map>

namespace
{

const char *relocation_name(ObjectFile::RelocationEntry::Type type)
{
    using Type = ObjectFile::RelocationEntry::Type;
    switch (type)
    {
    case Type::R_EMU32_O_LO12:
        return "R_EMU32_O_LO12";
    case Type::R_EMU32_ADRP_HI20:
        return "R_EMU32_ADRP_HI20";
    case Type::R_EMU32_MOV_LO19:
        return "R_EMU32_MOV_LO19";
    case Type::R_EMU32_MOV_HI13:
        return "R_EMU32_MOV_HI13";
    case Type::R_EMU32_B_OFFSET22:
        return "R_EMU32_B_OFFSET22";
    case Type::R_EMU32_ABS32:
        return "R_EMU32_ABS32";
    case Type::UNDEFINED:
        break;
    }
    return "<ERROR>";
}

/// An address the way it is shown at the start of a line.
std::string address_text(U64 address)
{
    return color_val_str(to_hex_str(dword(address)));
}

/// At least four digits, and room for the highest number of the section.
int digits_for(size_t count)
{
    return std::max(4, int(std::bit_width(count)));
}

} // namespace

void ObjectFile::print()
{
    print(std::cout);
}

void ObjectFile::print(std::ostream &out)
{
    /* An object that was neither read nor assembled has nothing to list. */
    if (sections.empty())
    {
        out << "ERROR: Cannot print object file. It has no sections.\n";
        return;
    }

    out << std::format("{}.{}:\tfile format belf32-littleemu32\n\n", m_obj_file.get_name(),
                       m_obj_file.get_extension());
    out << "SYMBOL TABLE:\n";
    for (const auto &[key, symbol] : symbol_table)
    {
        char visibility = ' ';
        if (symbol.binding_info == SymbolTableEntry::BindingInfo::GLOBAL)
        {
            visibility = 'g';
        }
        else if (symbol.binding_info == SymbolTableEntry::BindingInfo::LOCAL)
        {
            visibility = 'l';
        }

        std::string section_name = "*UND*";
        if (symbol.section != U32(-1))
        {
            section_name = strings[sections[symbol.section].section_name];
        }

        out << std::format("{} {}\t {}\t\t {} {}\n", address_text(symbol.symbol_value), visibility,
                           section_name, address_text(0), strings[symbol.symbol_name]);
    }

    out << "\nContents of section .data:";
    const int data_address_width = digits_for(data_section.size() / 16);
    for (size_t i = 0; i < data_section.size(); i++)
    {
        if (i % 16 == 0)
        {
            out << std::format("\n{:0{}x} ", i, data_address_width);
        }
        if (i % 4 == 0 && i % 16 != 0)
        {
            out << ' ';
        }
        out << std::format("{:02x}", data_section[i]);
    }

    if (!rel_data.empty())
    {
        out << "\n\nRelocations of section .data:";
        for (const RelocationEntry &rel : rel_data)
        {
            out << std::format("\n{:0{}x}: {:<20}{}", rel.offset, data_address_width,
                               relocation_name(rel.type),
                               strings[symbol_table.at(rel.symbol).symbol_name]);
        }
    }

    out << "\n\nDisassembly of section .text:\n";

    // The labels of the code, by address. The labels of a scope are not shown.
    std::map<sword, U32> labels;
    for (const auto &[key, symbol] : symbol_table)
    {
        if (symbol.section == U32(-1) || sections[symbol.section].type != SectionHeader::Type::TEXT
            || strings[symbol.symbol_name].find("::SCOPE") != std::string::npos)
        {
            continue;
        }

        labels[sword(symbol.symbol_value)] = symbol.symbol_name;
    }

    std::unordered_map<word, RelocationEntry> rel_text_map;
    for (const RelocationEntry &rel : rel_text)
    {
        rel_text_map[rel.offset] = rel;
    }

    if (labels.find(0) == labels.end())
    {
        out << address_text(0) << ":";
    }

    const int text_address_width = digits_for(text_section.size() / 4);
    for (size_t i = 0; i < text_section.size(); i++)
    {
        const word address = word(i * 4);
        const auto label_here = labels.find(sword(address));
        if (label_here != labels.end())
        {
            if (i != 0)
            {
                out << "\n\n";
            }
            out << address_text(address) << " <" << strings[label_here->second] << ">:";
        }

        const std::string disassembly = Emulator32bit::disassemble_instr(text_section[i]);
        out << std::format("\n{:{}x}", address, text_address_width);

        const size_t space = disassembly.find_first_of(' ');
        if (space != std::string::npos)
        {
            const std::string op = disassembly.substr(0, space).substr(0, 12);
            const std::string operands = disassembly.substr(space + 1);
            out << std::format(":\t{:08x}\t{}\t\t{}", text_section[i], op, operands);

            const U8 opcode = bitfield_unsigned(text_section[i], 26, 6);
            if (opcode == Emulator32bit::_op_b || opcode == Emulator32bit::_op_bl)
            {
                /* branch offsets are in words, relative to the branch instruction itself */
                const sword offset_words = bitfield_signed(text_section[i], 0, 22);
                const sword target = sword(address) + offset_words * 4;
                auto label = labels.upper_bound(target);
                if (target >= 0 && label != labels.begin())
                {
                    --label;
                    const sword delta = target - label->first;
                    out << " <" << strings[label->second];
                    if (delta != 0)
                    {
                        out << std::format("+{:#x}", delta);
                    }
                    out << ">";
                }
            }
        }
        else
        {
            out << std::format(":\t{:08x}\t{}", text_section[i], disassembly.substr(0, 12));
        }

        /* Check if there is a relocation record here */
        const auto rel = rel_text_map.find(address);
        if (rel != rel_text_map.end())
        {
            out << "\n" << std::string(text_address_width, ' ');
            out << std::format(" \t{:x}: {:<{}}{}", address, relocation_name(rel->second.type),
                               std::max(1, 29 - int(std::bit_width(i / 4))),
                               strings[symbol_table.at(rel->second.symbol).symbol_name]);
        }
    }
    out << "\n";
}
