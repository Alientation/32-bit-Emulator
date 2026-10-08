#pragma once

#include "assembler/object_file.h"
#include "assembler/tokenizer.h"

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
    Linker(std::vector<ObjectFile> obj_files, File exe_file);
    Linker(std::vector<ObjectFile> obj_files, File exe_file, File ld_file);

  private:
    std::vector<ObjectFile> m_obj_files;

    File m_exe_file;
    File m_ld_file;

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
            DATA,
            BSS
        };
        Type type;

        bool set_address = false;
        word address = 0;
        bool physical = false;
    };

    bool m_physical = false;
    std::vector<SectionAddress> m_sections;

    void link();

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
