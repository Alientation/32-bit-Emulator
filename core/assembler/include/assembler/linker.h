#pragma once

#include "assembler/object_file.h"
#include "assembler/tokenizer.h"

#include <array>
#include <unordered_map>
#include <vector>

/*
    Linker script

    Keep things simple

    allow addresses of each section to be specified
        - specify whether it is physical or virtual memory addresses
    allow entry point symbol to be defined

*/

class Linker
{
  public:
    /// Links with the default linker script.
    Linker(std::vector<ObjectFile> obj_files, File exe_file);
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
    ObjectFile new_executable() const;
    /// Returns the offset of the sections of each object file within the merged sections.
    std::vector<SectionBase> merge_sections(ObjectFile &exe) const;
    /// Returns the final address of each section.
    SectionBase place_sections(ObjectFile &exe) const;
    std::vector<SymbolMap> merge_symbols(ObjectFile &exe, const std::vector<SectionBase> &bases,
                                         const SectionBase &addresses) const;
    void define_entry(ObjectFile &exe) const;
    /// Defines the bounds of .init_array and .fini_array as symbols.
    void define_array_symbols(ObjectFile &exe, const SectionBase &addresses) const;
    void relocate(ObjectFile &exe, const std::vector<SectionBase> &bases,
                  const SectionBase &addresses, const std::vector<SymbolMap> &symbols) const;

    /// Lexes the linker script.
    void tokenize_ld();

    /// Reads the commands of the linker script: `ENTRY(symbol)` and `SECTIONS(...)`.
    void parse_ld();
    void _entry();
    void _sections();

    word parse_value();

    /// Consumes the next token if it has the type, otherwise fails saying what was expected.
    const basm::Token &expect(basm::TokenType type, const std::string &expected);

    [[noreturn]] void fail(const basm::Token &at, const std::string &message);
};
