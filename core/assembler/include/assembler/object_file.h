#pragma once

#include "emulator32bit/emulator32bit_util.h"
#include "util/file.h"

#include <array>
#include <iosfwd>
#include <map>
#include <unordered_map>
#include <vector>

/// A relocatable object file (.bo) or an executable (.bexe), in memory: the sections, the symbols
/// and the relocations, with the code to read and write the BELF format (docs/belf-format.md).
class ObjectFile
{
    friend class Linker;

  public:
    /// Makes an empty object file, which the assembler fills.
    ObjectFile();

    /// Reads an object file.
    ///
    /// @param obj_file the file to read
    ObjectFile(File obj_file);

    /// Replaces the contents with those of a file. A file that is not valid is a fatal error.
    ///
    /// @param object_file the file to read
    void read_object_file(File object_file);

    /// Replaces the contents with those of an object file that is in memory.
    ///
    /// @param bytes the bytes of the file
    void read_object_file(std::vector<byte> &bytes);

    /// Writes the object file.
    ///
    /// @param object_file the file to write
    void write_object_file(File object_file);

    /// Symbols defined in this unit.
    struct SymbolTableEntry
    {
        /// Index into the string table.
        U32 symbol_name;

        /// Value of the symbol.
        word symbol_value;

        /// Binding type of the symbol. Determines how to resolve symbol references
        /// during the linking process.
        enum class BindingInfo
        {
            /// Defined in this file and not visible to the others.
            LOCAL = 0,

            /// Defined in this file and visible to the others (`.global`).
            GLOBAL = 1,

            /// A reference that nothing defined in this file yet (what `.extern` and
            /// the use of a name give), which the linker resolves.
            WEAK = 2,

            /// Declared with `.weak`. A definition of it is used only if no other
            /// file has a (strong) definition, and if there is none at all the
            /// symbol is 0 instead of an undefined reference.
            WEAK_DECLARED = 3
        } binding_info;

        /// Index into the section table that this symbol is defined in. U32(-1)
        /// indicates no section.
        U32 section;
    };

    /// Description of a section stored in the object file binary.
    struct SectionHeader
    {
        /// Index into the string table of the section name.
        U32 section_name;

        /// Type of section.
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
            RODATA,
            INIT_ARRAY,
            FINI_ARRAY,
            REL_RODATA,
            REL_INIT_ARRAY,
            REL_FINI_ARRAY,

            /// A section the program named itself (`.section "name"`), read only, writable or
            /// executable. See @ref UserSection.
            USER_R,
            USER_RW,
            USER_RX,

            /// The relocations of the user section whose header is just before this one.
            REL_USER,

            /// A user section that is zero filled and writable (`.section "name", "rw",
            /// "nobits"`). Like .bss, the file has its size and no bytes.
            USER_BSS,
        } type;

        /// Offset this section starts at in bytes.
        word section_start;

        /// Size of the section in bytes.
        word section_size;

        /// Size of an entry in the section.
        /// @todo Why is this necessary? ELF seems to have this but why.
        word entry_size;

        /// Whether the section is loaded at a physical address, as opposed to a
        /// virtual one. Set by the linker.
        bool load_at_physical_address = false;

        /// Address the section is loaded at. Set by the linker.
        word address = 0;

        /// What the address of the section, and of what it holds from an object file,
        /// is a multiple of in bytes. The largest .align of the section.
        word alignment = 1;
    };

    /// A place in a section that holds an address, which is not known until the
    /// file is linked.
    struct RelocationEntry
    {
        /// Offset from the beginning of the section to symbol.
        word offset;

        /// Index into symbol table.
        U32 symbol;

        /// Type of relocation.
        enum class Type
        {
            /// Undefined.
            UNDEFINED,

            /// The low 12 bits of the address, in the 14 bit immediate of an ALU instruction
            /// (`:lo12:`).
            R_EMU32_O_LO12,

            /// The distance in 4 KiB pages from the instruction to the address, in `adrp`.
            R_EMU32_ADRP_HI20,

            /// Bits 0-18 of the address, in `mov` or `mvn` (`:lo19:`).
            R_EMU32_MOV_LO19,

            /// Bits 19-31 of the address, in `mov` or `mvn` (`:hi13:`).
            R_EMU32_MOV_HI13,

            /// Branch offset, a signed 22 bit number of instructions (a +/- 24 bit byte distance,
            /// the last 2 bits are 0).
            R_EMU32_B_OFFSET22,

            /// The 32 bit address of the symbol, in .data (`.word symbol`).
            R_EMU32_ABS32,

            /// `adr`: the distance in bytes from the instruction to the target, a
            /// signed 21 bit number (the 20 bits of the immediate and the sign bit).
            R_EMU32_ADR_PCREL21,
        } type;

        /// Signed constant added to the address of the symbol, so the relocation
        /// refers to `symbol + addend` (`label + 4`). Stored in 8 bytes, the
        /// reader keeps the low 32 bits. The 32 bit arithmetic wraps.
        sword addend;

        /// Token index that the relocation entry is used on. Use to fill local symbols.
        size_t token;
    };

    // The sizes in bytes of what the file holds, as in docs/belf-format.md. Numbers are stored in
    // more bytes than they need.

    /// Magic 4, unused 12, file type 2, target machine 2, flags 2, number of sections 2.
    static constexpr U32 kBELFHeaderSize = 24;

    /// Name 8, type 4, start 8, size 8, entry size 8, physical 1, address 8,
    /// alignment 8.
    static constexpr U32 kSectionHeaderSize = 53;

    /// What .bss takes in the file: its size.
    static constexpr U32 kBSSSectionSize = 8;

    /// One instruction.
    static constexpr U32 kTextEntrySize = 4;

    /// Offset 8, symbol 8, type 4, addend 8.
    static constexpr U32 kRelocationEntrySize = 28;

    /// Name 8, value 8, binding 2, section 8.
    static constexpr U32 kSymbolTableEntrySize = 26;

    /// The file type of an object file (.bo).
    static constexpr hword kRelocatableFileType = 1;

    /// The file type of an executable (.bexe).
    static constexpr hword kExecutableFileType = 2;

    /// The file type of a shared object, which is not made yet.
    static constexpr hword kSharedObjectFileType = 3;

    /// The target machine of the emulator.
    static constexpr hword kEMU32MachineId = 1;

    /// What binary file type this object file represents.
    hword file_type = 0;

    /// Target machine that this object file is built for.
    hword target_machine = 0;

    /// Flags of the file, none are defined.
    hword flags = 0;

    /// Number of sections added so far.
    hword n_sections = 0;

    /// Instructions stored in .text section.
    std::vector<word> text_section;

    /// Data stored in .data section.
    std::vector<byte> data_section;

    /// Read only data, `.rodata`.
    std::vector<byte> rodata_section;

    /// The addresses of functions to call before `main`, `.init_array`. Words.
    std::vector<byte> init_array_section;

    /// The addresses of functions to call after `main`, `.fini_array`. Words.
    std::vector<byte> fini_array_section;

    /// Size of .bss section. Zero initialized on program load.
    word bss_section = 0;

    /// Maps string index to symbol.
    /// In the order of the string table, which is the order the symbols were
    /// added in, so a file is the same bytes whatever the standard library is.
    std::map<U32, SymbolTableEntry> symbol_table;

    /// References to symbols that need to be relocated.
    std::vector<RelocationEntry> rel_text;

    /// For now, no purpose.
    std::vector<RelocationEntry> rel_data;

    /// For now, no purpose.
    /// @todo Will this ever be used?
    std::vector<RelocationEntry> rel_bss;

    /// `.word symbol` in .rodata, .init_array and .fini_array.
    std::vector<RelocationEntry> rel_rodata;
    std::vector<RelocationEntry> rel_init_array;
    std::vector<RelocationEntry> rel_fini_array;

    /// The sections that hold bytes (and the relocations for words in them)
    /// as opposed to code or a size: .data, .rodata, .init_array and
    /// .fini_array. They are handled the same everywhere; this lets the
    /// assembler, linker, loader and file reader/writer loop over them.
    struct ByteSection
    {
        const char *name;
        const char *rel_name;
        SectionHeader::Type type;
        SectionHeader::Type rel_type;
        std::vector<byte> ObjectFile::*bytes;
        std::vector<RelocationEntry> ObjectFile::*relocations;

        /// Whether the loaded section may be written by the program.
        bool writable;

        /// What the section is aligned to by default (the entries are words).
        word alignment;
    };

    /// A section with a name of the program's choosing (`.section "name"`),
    /// holding bytes. Unlike the sections above there can be any number of
    /// them. Its header is followed by the header of its relocations
    /// (@ref SectionHeader::Type::REL_USER). An executable one holds
    /// instructions (words, little endian) as well as data. A `nobits` one is
    /// like .bss: writable, zeroed when the program is loaded, and the file has
    /// its size and no bytes.
    struct UserSection
    {
        std::string name;

        /// Whether the loaded section may be written by the program.
        bool writable = false;

        /// Whether the program runs code from it. Never together with writable.
        bool executable = false;

        /// Whether it is zero filled instead of holding `bytes`. Then it is
        /// writable, not executable, and has no relocations.
        bool nobits = false;

        std::vector<byte> bytes;
        std::vector<RelocationEntry> relocations;

        /// The size of a nobits section.
        word zero_size = 0;

        /// Size in bytes, `zero_size` for a nobits section.
        word size() const
        {
            return nobits ? zero_size : word(bytes.size());
        }

        /// Index into `sections` of its header. The header of its relocations is
        /// the next one.
        U32 header_index = U32(-1);
    };

    /// The user sections, in the order they were added.
    std::vector<UserSection> user_sections;

    /// The type of the header of a user section with the permissions.
    static SectionHeader::Type user_section_type(bool writable, bool executable,
                                                 bool nobits = false);

    /// Whether the type is the type of a header of a user section.
    static bool is_user_section_type(SectionHeader::Type type);

    /// The user section with the name, or null.
    UserSection *find_user_section(const std::string &name);
    const UserSection *find_user_section(const std::string &name) const;

    /// The user section whose header is the one at the index, or null if that
    /// is not the header of one.
    UserSection *user_section_at(U32 header_index);

    /// Adds a user section and its two headers. The name is not one that is
    /// taken (a section of the file already, or one of the reserved names).
    ///
    /// @return Index into `sections` of its header.
    U32 add_user_section(const std::string &name, bool writable, bool executable,
                         bool nobits = false);

    /// The four byte sections, in the order .data, .rodata, .init_array,
    /// .fini_array.
    static const std::array<ByteSection, 4> &byte_sections();

    /// The byte section with the section type or relocation section type, or
    /// null if the type is none of them.
    static const ByteSection *byte_section_of(SectionHeader::Type type);

    // TODO: possibly add a separate string table for the section headers, like ELF files.
    // TODO: make the string table store the offset of the first character of a string, not the
    // position of the string in the array.

    /// Stores all the strings in a compact table.
    std::vector<std::string> strings;

    /// Maps strings to index in the table.
    std::unordered_map<std::string, U32> string_table;

    /// Section headers.
    std::vector<SectionHeader> sections;

    /// Map section name to index in sections.
    std::unordered_map<std::string, U32> section_table;

    /// Adds a string to the string table. The string is not in it yet.
    ///
    /// @param string the string to add
    /// @return the index of the string
    U32 add_string(const std::string &string);

    /// Adds a symbol to the symbol table.
    ///
    /// @param symbol the name of the symbol
    /// @param value the value of the symbol if it is defined
    /// @param binding_info the visibility of the symbol
    /// @param section the section it is defined in, -1 if it is not defined in a section
    void add_symbol(const std::string &symbol, word value,
                    SymbolTableEntry::BindingInfo binding_info, U32 section = U32(-1));

    /// Adds a section header. The name is not the name of a section yet.
    ///
    /// @param section_name the name of the section
    /// @param type the type of the section
    /// @return the index of the header in `sections`
    U32 add_section(const std::string &section_name, SectionHeader::Type type);

    /// @param symbol the index of a symbol, the key of `symbol_table`
    /// @return the name of the symbol
    std::string get_symbol_name(U32 symbol);

    /// Get the size of the .text section.
    ///
    /// @return Size of .text section in bytes.
    word get_text_section_size();

    /// Get size of .data section.
    ///
    /// @return Size of .data section in bytes.
    word get_data_section_size();

    /// Get size of .bss section.
    ///
    /// @return Size of .bss section in bytes.
    word get_bss_section_size();

    /// Get the current size of a .text, .data or .bss section, which is the
    /// offset that the next item added to it will have.
    ///
    /// @param section Index into the section table.
    /// @return Size of the section in bytes.
    word get_section_size(U32 section);

    /// Prints an objdump-style listing (symbols, .data, disassembled .text with
    /// relocations) to the stream, stdout by default. Prints an error line
    /// instead if the object has no sections (it was never read or assembled).
    void print();

    /// Same, to the stream.
    ///
    /// @param out where the listing goes
    void print(std::ostream &out);

  private:
    /// The file that was read, for the error messages.
    File m_obj_file;

    /// Reads the sections, symbols and relocations from the bytes of a file.
    ///
    /// @param bytes the bytes of the file
    void disassemble(std::vector<byte> &bytes);

    /// disassemble() that reports a truncated or corrupt file as a fatal error.
    void disassemble_checked(std::vector<byte> &bytes);

    /// Fatal if what was read is not a file that the linker and the loader can
    /// use: sections that are missing, and symbols, sections and relocations
    /// that refer to something that is not there.
    void validate() const;
};