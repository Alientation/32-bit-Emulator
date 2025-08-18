#pragma once

#include "emulator32bit/emulator32bit_util.h"
#include "util/file.h"

#include <unordered_map>
#include <vector>

class ObjectFile
{
    friend class Linker;

  public:
    ObjectFile ();
    ObjectFile (File obj_file);

    void read_object_file (File object_file);
    void read_object_file (std::vector<byte> &bytes);
    void write_object_file (File object_file);

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

        /// @brief          TODO:
        bool load_at_physical_address = false;

        /// @brief          TODO:
        word address = 0;
    };

    /// @brief              TODO:
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
        } type;

        /// @brief          Constant to be added to the value of symbol.
        word shift;

        /// @brief          Token index that the relocation entry is used on. Use to fill local symbols.
        size_t token;
    };

    // TODO: Figure out how these sizes are calculated again.
    /// @brief              TODO:
    static constexpr U32 kBELFHeaderSize = 24;

    /// @brief              TODO:
    static constexpr U32 kSectionHeaderSize = 45;

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
    hword file_type;

    /// @brief              Target machine that this object file is built for.
    hword target_machine;

    /// @brief              TODO:
    hword flags;

    /// @brief              TODO:
    hword n_sections;

    /// @brief              Instructions stored in .text section.
    std::vector<word> text_section;

    /// @brief              Data stored in .data section.
    std::vector<byte> data_section;

    /// @brief              Size of .bss section. Zero initialized on program load.
    word bss_section = 0;

    /// @brief              Maps string index to symbol.
    std::unordered_map<U32, SymbolTableEntry> symbol_table;

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
    U32 add_string (const std::string &string);

    /// @brief              TODO:
    /// @param symbol
    /// @param value
    /// @param binding_info
    /// @param section
    void add_symbol (const std::string &symbol, word value,
                     SymbolTableEntry::BindingInfo binding_info, U32 section = U32 (-1));

    /// @brief              TODO:
    /// @param section_name
    /// @param type
    /// @return
    U32 add_section (const std::string &section_name, SectionHeader::Type type);

    /// @brief              TODO:
    /// @param symbol
    /// @return
    std::string get_symbol_name (U32 symbol);

    /// @brief              Get the size of the .text section.
    /// @return             Size of .text section in bytes.
    word get_text_section_size ();

    /// @brief              Get size of .data section.
    /// @return             Size of .data section in bytes.
    word get_data_section_size ();

    /// @brief              Get size of .bss section.
    /// @return             Size of .bss section in bytes.
    word get_bss_section_size ();

  private:
    /// @brief              State of the disassembly.
    enum class State
    {
        /// @brief          TODO:
        NO_STATE,

        /// @brief          TODO:
        DISASSEMBLING,

        /// @brief          TODO:
        DISASSEMBLED_SUCCESS,

        /// @brief          TODO:
        DISASSEMBLED_ERROR,

        /// @brief          TODO:
        WRITING,

        /// @brief          TODO:
        WRITING_SUCCESS,

        /// @brief          TODO:
        WRITING_ERROR,
    } m_state;

    /// @brief              TODO:
    File m_obj_file;

    /// @brief              TODO:
    /// @param bytes
    void disassemble (std::vector<byte> &bytes);

    /// @brief              TODO:
    void print ();
};