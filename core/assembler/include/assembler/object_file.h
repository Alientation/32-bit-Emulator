#pragma once

#include "emulator32bit/emulator32bit_util.h"
#include "util/file.h"

#include <iosfwd>
#include <map>
#include <unordered_map>
#include <vector>

/// @param[in] size A size or an address.
/// @param[in] alignment A power of two.
/// @return How much has to be added to the size to make it a multiple of the alignment.
inline word padding_to(const word size, const word alignment)
{
    return (alignment - size % alignment) % alignment;
}

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
    /// @param[in] obj_file The file to read.
    ObjectFile(const File &obj_file);

    /// Replaces the contents with those of a file. A file that is not valid is a fatal error.
    ///
    /// @param[in] object_file The file to read.
    void read_object_file(const File &object_file);

    /// Replaces the contents with those of an object file that is in memory (a member of a static
    /// library). A file that is not valid is a fatal error, which does not name a file.
    ///
    /// @param[in] bytes The bytes of the file.
    void read_object_file(const std::vector<byte> &bytes);

    /// Writes the object file.
    ///
    /// @param[in] object_file The file to write.
    void write_object_file(const File &object_file);

    /// A symbol of this file: one it defines, or one it refers to.
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

            /// A reference to a symbol that this file does not define (what `.extern` and the
            /// use of a name give). The linker resolves it.
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

        /// Size in bytes of an entry of a section that is a table: 4 for the instructions of .text,
        /// kSymbolTableEntrySize and kRelocationEntrySize for the symbols and the relocations. 0
        /// for the others. Nothing reads it back, the sizes are known from the type of the section.
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
        /// Offset from the beginning of the section to the place that holds the address.
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

            /// The 32 bit address of the symbol, in a section of bytes (`.word symbol`).
            R_EMU32_ABS32,

            /// `adr`: the distance in bytes from the instruction to the target, a
            /// signed 21 bit number (the 20 bits of the immediate and the sign bit).
            R_EMU32_ADR_PCREL21,
        } type;

        /// Signed constant added to the address of the symbol, so the relocation
        /// refers to `symbol + addend` (`label + 4`). Stored in 8 bytes, the
        /// reader keeps the low 32 bits. The 32 bit arithmetic wraps.
        sword addend;

        /// The position in the tokens of the source where the relocation was made, which tells
        /// the assembler which scopes were open when it fills in local symbols.
        size_t token;
    };

    // The sizes in bytes of what the file holds, as in docs/belf-format.md. Numbers are stored in
    // more bytes than they need.

    /// The unused bytes after the magic number in the header.
    static constexpr U32 kBELFHeaderPadding = 12;

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

    /// `.word symbol` in .data.
    std::vector<RelocationEntry> rel_data;

    /// Always empty, since .bss has no contents. The file format has the section for symmetry.
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
        /// The name of the section, and of the section of its relocations.
        const char *name;
        const char *rel_name;

        /// The types of the headers of the two.
        SectionHeader::Type type;
        SectionHeader::Type rel_type;

        /// The members of ObjectFile that hold the bytes and the relocations.
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

        /// @return The size in bytes, `zero_size` for a nobits section.
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

    /// @param[in] writable Whether the section is writable.
    /// @param[in] executable Whether the section is executable.
    /// @param[in] nobits Whether the section is zero filled.
    /// @return The type of the header of a user section with the permissions.
    static SectionHeader::Type user_section_type(bool writable, bool executable,
                                                 bool nobits = false);

    /// @param[in] type The type of a section header.
    /// @return Whether the type is the type of a header of a user section.
    static bool is_user_section_type(SectionHeader::Type type);

    /// @param[in] name The name of the section.
    /// @return The user section with the name, or null.
    UserSection *find_user_section(const std::string &name);

    /// @copydoc find_user_section(const std::string &)
    const UserSection *find_user_section(const std::string &name) const;

    /// @param[in] header_index The index into `sections`.
    /// @return The user section whose header is the one at the index, or null if that is not the
    ///     header of one.
    UserSection *user_section_at(U32 header_index);

    /// @copydoc user_section_at(U32)
    const UserSection *user_section_at(U32 header_index) const;

    /// Adds a user section and its two headers. The name is not one that is
    /// taken (a section of the file already, or one of the reserved names).
    ///
    /// @param[in] name The name of the section.
    /// @param[in] writable Whether the section is writable.
    /// @param[in] executable Whether the section is executable.
    /// @param[in] nobits Whether the section is zero filled.
    /// @return Index into `sections` of its header.
    U32 add_user_section(const std::string &name, bool writable, bool executable,
                         bool nobits = false);

    /// The number of byte sections.
    static constexpr size_t kNumByteSections = 4;

    /// The place of each byte section in `byte_sections()`.
    static constexpr size_t kDataIndex = 0;
    static constexpr size_t kRodataIndex = 1;
    static constexpr size_t kInitArrayIndex = 2;
    static constexpr size_t kFiniArrayIndex = 3;

    using ByteSections = ByteSection[kNumByteSections];

    /// @return The byte sections, in the order .data, .rodata, .init_array, .fini_array.
    static const ByteSections &byte_sections();

    /// @param[in] type A section type or relocation section type.
    /// @return The byte section with the type, or null if the type is none of them.
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

    /// Adds a string to the string table. Fatal if the string is in it already.
    ///
    /// @param[in] string The string to add.
    /// @return The index of the string.
    U32 add_string(const std::string &string);

    /// Adds a symbol to the symbol table. A symbol that is in it already, because it was used
    /// before, is defined by this call: it gets the section and the value. Defining it twice is a
    /// fatal error. The binding becomes the new one if that is GLOBAL or WEAK_DECLARED, or LOCAL for
    /// a symbol that was only used.
    ///
    /// @param[in] symbol The name of the symbol.
    /// @param[in] value The value of the symbol if it is defined.
    /// @param[in] binding_info The visibility of the symbol.
    /// @param[in] section The section it is defined in, -1 if it is not defined in a section.
    void add_symbol(const std::string &symbol, word value,
                    SymbolTableEntry::BindingInfo binding_info, U32 section = U32(-1));

    /// Adds a section header. Fatal if there is a section with the name already.
    ///
    /// @param[in] section_name The name of the section.
    /// @param[in] type The type of the section.
    /// @return The index of the header in `sections`.
    U32 add_section(const std::string &section_name, SectionHeader::Type type);

    /// @param[in] symbol The index of a symbol, the key of `symbol_table`.
    /// @return The name of the symbol.
    std::string get_symbol_name(U32 symbol) const;

    /// Get the size of the .text section.
    ///
    /// @return Size of .text section in bytes.
    word get_text_section_size() const;

    /// Get size of .data section.
    ///
    /// @return Size of .data section in bytes.
    word get_data_section_size() const;

    /// Get size of .bss section.
    ///
    /// @return Size of .bss section in bytes.
    word get_bss_section_size() const;

    /// The current size of a section, which is the offset that the next item added to it will have.
    ///
    /// @param[in] section Index into `sections`.
    /// @return Size of the section in bytes.
    word get_section_size(U32 section) const;

    /// Prints an objdump-style listing to stdout: the symbols, the contents of the sections with
    /// their relocations, and the disassembled .text. Prints an error line instead if the object
    /// has no sections (it was never read or assembled).
    void print() const;

    /// Same, to a stream.
    ///
    /// @param[out] out Where the listing goes.
    void print(std::ostream &out) const;

  private:
    /// The file that was read, for the error messages.
    File m_obj_file;

    /// Reads the sections, symbols and relocations from the bytes of a file.
    ///
    /// @param[in] bytes The bytes of the file.
    void deserialize(const std::vector<byte> &bytes);

    /// deserialize () that reports a truncated or corrupt file as a fatal error.
    ///
    /// @param[in] bytes The bytes of the file.
    void deserialize_checked(const std::vector<byte> &bytes);

    /// Fatal if what was read is not a file that the linker and the loader can
    /// use: sections that are missing, and symbols, sections and relocations
    /// that refer to something that is not there.
    void validate() const;
};