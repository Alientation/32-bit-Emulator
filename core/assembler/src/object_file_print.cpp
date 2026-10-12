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

/// The width that the name of a relocation is padded to in the listing of the code, less one for
/// each bit of the number of the instruction (so it gets narrower further down).
constexpr S32 kRelocationNameWidth = 29;

/// The bytes of a section on a line of the listing, and the bytes in a group on that line.
constexpr size_t kBytesPerLine = 16;
constexpr size_t kBytesPerGroup = 4;

/// The least number of digits of the addresses that start the lines of the contents.
constexpr S32 kMinAddressDigits = 4;

/// The characters of the name of an instruction that are listed.
constexpr size_t kMnemonicWidth = 12;

static const char *relocation_name(const ObjectFile::RelocationEntry::Type type)
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
    case Type::R_EMU32_ADR_PCREL21:
        return "R_EMU32_ADR_PCREL21";
    case Type::UNDEFINED:
        break;
    }
    return "<ERROR>";
}

/// The symbol of a relocation with its addend, `table+0x8`.
static std::string target_text(const ObjectFile &obj, const ObjectFile::RelocationEntry &rel)
{
    const std::string &name = obj.strings[obj.symbol_table.at(rel.symbol).symbol_name];
    if (rel.addend == 0)
    {
        return name;
    }
    const S64 addend = rel.addend;
    return std::format("{}{}{:#x}", name, addend < 0 ? '-' : '+', addend < 0 ? -addend : addend);
}

/// An address the way it is shown at the start of a line.
static std::string address_text(const U64 address)
{
    return color_val_str(to_hex_str(dword(address)));
}

/// At least four digits, and room for the highest number of the section.
static S32 digits_for(const size_t count)
{
    const S32 digits = S32(std::bit_width(count));
    return digits > kMinAddressDigits ? digits : kMinAddressDigits;
}

/// Lists the bytes of a section, 16 to a line, and then its relocations. `separator` is what
/// comes before the section.
static void print_bytes(std::ostream &out, const ObjectFile &obj, const char *separator,
                        const std::string &name, const std::vector<byte> &bytes_of_section,
                        const std::vector<ObjectFile::RelocationEntry> &relocations)
{
    out << std::format("{}Contents of section {}:", separator, name);
    const S32 width = digits_for(bytes_of_section.size() / kBytesPerLine);
    for (size_t i = 0; i < bytes_of_section.size(); i++)
    {
        if (i % kBytesPerLine == 0)
        {
            out << std::format("\n{:0{}x} ", i, width);
        }
        if (i % kBytesPerGroup == 0 && i % kBytesPerLine != 0)
        {
            out << ' ';
        }
        out << std::format("{:02x}", bytes_of_section[i]);
    }

    if (!relocations.empty())
    {
        out << std::format("\n\nRelocations of section {}:", name);
        for (const ObjectFile::RelocationEntry &rel : relocations)
        {
            out << std::format("\n{:0{}x}: {:<20}{}", rel.offset, width, relocation_name(rel.type),
                               target_text(obj, rel));
        }
    }
}

void ObjectFile::print() const
{
    print(std::cout);
}

void ObjectFile::print(std::ostream &out) const
{
    // An object that was neither read nor assembled has nothing to list.
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

    // .data is always listed, the other byte sections only when they have contents.
    for (const ByteSection &section : byte_sections())
    {
        if (section.type != SectionHeader::Type::DATA && (this->*section.bytes).empty())
        {
            continue;
        }
        print_bytes(out, *this, &section == &byte_sections()[0] ? "\n" : "\n\n", section.name,
                    this->*section.bytes, this->*section.relocations);
    }

    // The sections of the program's own, as bytes whatever they hold (code too).
    for (const UserSection &section : user_sections)
    {
        if (section.nobits)
        {
            out << std::format("\n\nContents of section {}: {} bytes, zero filled when loaded",
                               section.name, section.zero_size);
            continue;
        }
        print_bytes(out, *this, "\n\n", section.name, section.bytes, section.relocations);
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

    const S32 text_address_width = digits_for(text_section.size() / 4);
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
            const std::string op = disassembly.substr(0, space).substr(0, kMnemonicWidth);
            const std::string operands = disassembly.substr(space + 1);
            out << std::format(":\t{:08x}\t{}\t\t{}", text_section[i], op, operands);

            const U8 opcode = bitfield_unsigned<26, 6>(text_section[i]);
            if (opcode == encoding::_op_b || opcode == encoding::_op_bl)
            {
                // Branch offsets are in words, relative to the branch instruction itself.
                const sword offset_words = bitfield_signed<0, 22>(text_section[i]);
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
            out << std::format(":\t{:08x}\t{}", text_section[i],
                               disassembly.substr(0, kMnemonicWidth));
        }

        // Check whether there is a relocation record here.
        const auto rel = rel_text_map.find(address);
        if (rel != rel_text_map.end())
        {
            out << "\n" << std::string(text_address_width, ' ');
            const S32 name_width = kRelocationNameWidth - S32(std::bit_width(i / 4));
            out << std::format(" \t{:x}: {:<{}}{}", address, relocation_name(rel->second.type),
                               name_width > 1 ? name_width : 1, target_text(*this, rel->second));
        }
    }
    out << "\n";
}
