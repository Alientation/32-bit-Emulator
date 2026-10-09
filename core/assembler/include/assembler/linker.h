#pragma once

#include "assembler/object_file.h"
#include "assembler/tokenizer.h"

#include <array>
#include <unordered_map>
#include <vector>

/// Links object files into an executable: joins their sections, gives them addresses, resolves the
/// symbols and applies every relocation. The linker script (.ld) can give the sections addresses
/// (virtual, or physical with `@P;`) and name the entry point; without one the built-in layout is
/// used. See docs/belf-format.md.
class Linker
{
  public:
    /// Links with the default linker script.
    ///
    /// @param obj_files the object files to link
    /// @param exe_file the executable to write
    Linker(std::vector<ObjectFile> obj_files, File exe_file);

    /// Links with a linker script.
    ///
    /// @param obj_files the object files to link
    /// @param exe_file the executable to write
    /// @param ld_file the linker script
    Linker(std::vector<ObjectFile> obj_files, File exe_file, File ld_file);

    /// Reads the linker script, places the sections of all object files, resolves every symbol and
    /// relocation, and writes the executable. Fatal on a bad script or an undefined symbol.
    /// Call once.
    void link();

  private:
    std::vector<ObjectFile> m_obj_files;

    File m_exe_file;

    /// The linker script, unless the default one built into the linker is used.
    File m_ld_file;
    bool m_use_default_script = false;

    /// Owns the text of the linker script that the tokens point into.
    basm::SourceManager m_sources;
    basm::LexResult m_lexed;
    basm::TokenCursor m_cursor;

    std::string m_entry_symbol = "_start";

    struct SectionAddress
    {
        enum class Type
        {
            TEXT,
            BYTES, ///< one of ObjectFile::byte_sections(), `byte_index` says which
            BSS,
            USER ///< a section of the program's own, `name` says which
        };
        Type type;

        /// Index into ObjectFile::byte_sections() (.data, .rodata, .init_array, .fini_array).
        size_t byte_index = 0;

        /// The name of a USER section, as written in the script (a string).
        std::string name = {};

        bool set_address = false;
        word address = 0;
        bool physical = false;
    };

    bool m_physical = false;
    std::vector<SectionAddress> m_sections;

    /// One value for each of .text, .bss and the byte sections (.data, .rodata, .init_array,
    /// .fini_array, in the order of ObjectFile::byte_sections()), and for each of the user sections
    /// of the executable (in the order of `ObjectFile::user_sections` of the executable).
    struct SectionBase
    {
        word text = 0;
        std::array<word, 4> bytes = {};
        word bss = 0;
        std::vector<word> users = {};
    };

    /// For one object file, maps each of its symbols (by index into its symbol table) to the
    /// symbol of the executable.
    using SymbolMap = std::unordered_map<U32, U32>;

    // The stages of link().

    /// @return an executable with the header and no contents yet
    ObjectFile new_executable() const;

    /// Puts the sections of the object files one after another, in the order of the object
    /// files. A section of an object file starts at a multiple of its alignment (what `.align`
    /// asked for), so that the offsets it was assembled with stay aligned. The merged section
    /// has the largest alignment.
    ///
    /// @param exe the executable that gets the merged sections
    /// @return where the sections of each object file start within the merged ones
    std::vector<SectionBase> merge_sections(ObjectFile &exe) const;
    /// Gives the sections their addresses according to the linker script. A section that the
    /// script does not give an address follows the previous one.
    ///
    /// @param exe the executable whose sections are placed
    /// @return the final address of each section
    SectionBase place_sections(ObjectFile &exe) const;
    /// Puts the symbols of all object files into the symbol table of the executable, with the final
    /// addresses. A symbol of one object file is the same symbol of another by name, unless it is
    /// local.
    ///
    /// @param exe the executable that gets the symbols
    /// @param bases where the sections of each object file start within the merged ones
    /// @param addresses the final address of each section
    /// @return for each object file, the executable's symbol for each of its symbols
    std::vector<SymbolMap> merge_symbols(ObjectFile &exe, const std::vector<SectionBase> &bases,
                                         const SectionBase &addresses) const;
    /// The loader starts the program at _start. ENTRY(symbol) makes `symbol` the entry point by
    /// aliasing _start to it.
    ///
    /// @param exe the executable that gets the symbol
    void define_entry(ObjectFile &exe) const;
    /// Defines `__init_array_start`, `__init_array_end`, `__fini_array_start` and
    /// `__fini_array_end`, the bounds of the two arrays, for the startup code that calls what is
    /// in them. A symbol with that name that the program defines itself is left alone, one that
    /// it only refers to gets the value.
    ///
    /// @param exe the executable that gets the symbols
    /// @param addresses the final address of each section
    void define_array_symbols(ObjectFile &exe, const SectionBase &addresses) const;
    /// Every section has its final address by now, so each relocation is resolved here and the
    /// executable needs none at load time.
    ///
    /// @param exe the executable whose relocations are applied
    /// @param bases where the sections of each object file start within the merged ones
    /// @param addresses the final address of each section
    /// @param symbols for each object file, the executable's symbol for each of its symbols
    void relocate(ObjectFile &exe, const std::vector<SectionBase> &bases,
                  const SectionBase &addresses, const std::vector<SymbolMap> &symbols) const;

    /// Lexes the linker script.
    void tokenize_ld();

    /// Reads the commands of the linker script: `ENTRY(symbol)` and `SECTIONS(...)`.
    void parse_ld();

    /// `ENTRY(symbol)`: the symbol that the program starts at.
    void _entry();

    /// `SECTIONS { ... }`: the address of each section, and whether it is physical.
    void _sections();

    /// Parses an address.
    ///
    /// @return the number, which has to fit in 32 bits
    word parse_value();

    /// Consumes the next token if it has the type, otherwise fails saying what was expected.
    ///
    /// @param type the type the token has to have
    /// @param expected what the error says was expected
    /// @return the token
    const basm::Token &expect(basm::TokenType type, const std::string &expected);

    /// Logs the error at the token (file, line, column, source line) and terminates.
    ///
    /// @param at the token the error is about
    /// @param message what is wrong
    [[noreturn]] void fail(const basm::Token &at, const std::string &message);
};
