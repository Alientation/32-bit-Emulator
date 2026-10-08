#pragma once

#include "emulator32bit/emulator32bit_util.h"
#include "util/file.h"

#include <iosfwd>
#include <map>
#include <unordered_map>
#include <vector>

class ObjectFile
{
    friend class Linker;

  public:
    ObjectFile();
    ObjectFile(File obj_file);

    void read_object_file(File object_file);
    void read_object_file(std::vector<byte> &bytes);
    void write_object_file(File object_file);

    /// @brief              Symbols defined in this unit.
    struct SymbolTableEntry
    {
        /// @brief          Index into the string table.
        U32 symbol_name;

        /// @brief          Value of the symbol.
        word symbol_value;

        /// @brief          Binding type of the symbol. Determines how to resolve symbol references
        ///                 during the linking process.
        enum class BindingInfo
        {
            /// @brief      TODO:
            LOCAL = 0,

            /// @brief      TODO:
            GLOBAL = 1,

            /// @brief      TODO:
            WEAK = 2
        } binding_info;

        /// @brief          Index into the section table that this symbol is defined in. U32(-1)
        ///                 indicates no section.
        U32 section;
    };

    /// @brief              Description of a section stored in the object file binary.
    struct SectionHeader
    {
        /// @brief          Index into the string table of the section name.
        U32 section_name;

        /// @brief          Type of section.
        enum class Type
        {
            UNDEFINED,
            TEXT,
            DATA,
            BSS,
            SYMTAB,
            REL_TEXT,
            REL_DATA,
            REL_BSS,
            DEBUG,
            STRTAB,
        } type;

        /// @brief          Offset this section starts at in bytes.
        word section_start;

        /// @brief          Size of the section in bytes.
        word section_size;

        /// @brief          Size of an entry in the section.
        /// @todo           TODO: Why is this necessary? ELF seems to have this but why.
        word entry_size;

        /// @brief          Whether the section is loaded at a physical address, as opposed to a
        ///                 virtual one. Set by the linker.
        bool load_at_physical_address = false;

        /// @brief          Address the section is loaded at. Set by the linker.
        word address = 0;

        /// @brief          What the address of the section, and of what it holds from an object file,
        ///                 is a multiple of in bytes. The largest .align of the section.
        word alignment = 1;
    };

    /// @brief              A place in a section that holds an address, which is not known until the
    ///                     file is linked.
    struct RelocationEntry
    {
        /// @brief          Offset from the beginning of the section to symbol.
        word offset;

        /// @brief          Index into symbol table.
        U32 symbol;

        /// @brief          Type of relocation.
        enum class Type
        {
            /// @brief      Undefined.
            UNDEFINED,

            /// @brief      TODO:
            R_EMU32_O_LO12,

            /// @brief      Format O instructions and ADRP.
            R_EMU32_ADRP_HI20,

            /// @brief      TODO:
            R_EMU32_MOV_LO19,

            /// @brief      MOV/MVN instructions.
            R_EMU32_MOV_HI13,

            /// @brief      Branch offset, +/- 24 bit value (last 2 bits are 0).
            R_EMU32_B_OFFSET22,

            /// @brief      The 32 bit address of the symbol, in .data (`.word symbol`).
            R_EMU32_ABS32,
        } type;

        /// @brief          Signed constant added to the address of the symbol, so the relocation
        ///                 refers to `symbol + addend` (`label + 4`). Stored in 8 bytes, the
        ///                 reader keeps the low 32 bits. The 32 bit arithmetic wraps.
        sword addend;

        /// @brief          Token index that the relocation entry is used on. Use to fill local symbols.
        size_t token;
    };

    // TODO: Figure out how these sizes are calculated again.
    /// @brief              TODO:
    static constexpr U32 kBELFHeaderSize = 24;

    /// @brief              Name 8, type 4, start 8, size 8, entry size 8, physical 1, address 8,
    ///                     alignment 8.
    static constexpr U32 kSectionHeaderSize = 53;

    /// @brief              TODO:
    static constexpr U32 kBSSSectionSize = 8;

    /// @brief              TODO:
    static constexpr U32 kTextEntrySize = 4;

    /// @brief              TODO:
    static constexpr U32 kRelocationEntrySize = 28;

    /// @brief              TODO:
    static constexpr U32 kSymbolTableEntrySize = 26;

    /// @brief              TODO:
    static constexpr hword kRelocatableFileType = 1;

    /// @brief              TODO:
    static constexpr hword kExecutableFileType = 2;

    /// @brief              TODO:
    static constexpr hword kSharedObjectFileType = 3;

    /// @brief              TODO:
    static constexpr hword kEMU32MachineId = 1;

    /// @brief              What binary file type this object file represents.
    hword file_type = 0;

    /// @brief              Target machine that this object file is built for.
    hword target_machine = 0;

    /// @brief              TODO:
    hword flags = 0;

    /// @brief              Number of sections added so far.
    hword n_sections = 0;

    /// @brief              Instructions stored in .text section.
    std::vector<word> text_section;

    /// @brief              Data stored in .data section.
    std::vector<byte> data_section;

    /// @brief              Size of .bss section. Zero initialized on program load.
    word bss_section = 0;

    /// @brief              Maps string index to symbol.
    ///                     In the order of the string table, which is the order the symbols were
    ///                     added in, so a file is the same bytes whatever the standard library is.
    std::map<U32, SymbolTableEntry> symbol_table;

    /// @brief              References to symbols that need to be relocated.
    std::vector<RelocationEntry> rel_text;

    /// @brief              For now, no purpose.
    std::vector<RelocationEntry> rel_data;

    /// @brief              For now, no purpose.
    /// @todo               TODO: Will this ever be used?
    std::vector<RelocationEntry> rel_bss;

    // TODO: Possbly in future add separate string table for section headers like ELF files.
    // TODO: Refactor string table so that it stores the offset of the first character of a
    // string in the string table, not the position of it in the array.
    // TODO: what did i mean by the above?

    /// @brief              Stores all the strings in a compact table.
    std::vector<std::string> strings;

    /// @brief              Maps strings to index in the table.
    std::unordered_map<std::string, U32> string_table;

    /// @brief              Section headers.
    std::vector<SectionHeader> sections;

    /// @brief              Map section name to index in sections.
    std::unordered_map<std::string, U32> section_table;

    /// @brief              TODO:
    /// @param string
    /// @return
    U32 add_string(const std::string &string);

    /// @brief              TODO:
    /// @param symbol
    /// @param value
    /// @param binding_info
    /// @param section
    void add_symbol(const std::string &symbol, word value,
                    SymbolTableEntry::BindingInfo binding_info, U32 section = U32(-1));

    /// @brief              TODO:
    /// @param section_name
    /// @param type
    /// @return
    U32 add_section(const std::string &section_name, SectionHeader::Type type);

    /// @brief              TODO:
    /// @param symbol
    /// @return
    std::string get_symbol_name(U32 symbol);

    /// @brief              Get the size of the .text section.
    /// @return             Size of .text section in bytes.
    word get_text_section_size();

    /// @brief              Get size of .data section.
    /// @return             Size of .data section in bytes.
    word get_data_section_size();

    /// @brief              Get size of .bss section.
    /// @return             Size of .bss section in bytes.
    word get_bss_section_size();

    /// @brief              Get the current size of a .text, .data or .bss section, which is the
    ///                     offset that the next item added to it will have.
    /// @param section      Index into the section table.
    /// @return             Size of the section in bytes.
    word get_section_size(U32 section);

    /// @brief              Prints an objdump-style listing (symbols, .data, disassembled .text with
    ///                     relocations) to the stream, stdout by default. Prints an error line
    ///                     instead if the object has no sections (it was never read or assembled).
    void print();
    void print(std::ostream &out);

  private:
    /// @brief              TODO:
    File m_obj_file;

    /// @brief              TODO:
    /// @param bytes
    void disassemble(std::vector<byte> &bytes);

    /// @brief              disassemble() that reports a truncated or corrupt file as a fatal error.
    void disassemble_checked(std::vector<byte> &bytes);

    /// @brief              Fatal if what was read is not a file that the linker and the loader can
    ///                     use: sections that are missing, and symbols, sections and relocations
    ///                     that refer to something that is not there.
    void validate() const;
};