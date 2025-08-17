#pragma once

#include "assembler/object_file.h"

#include <set>

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
    Linker (std::vector<ObjectFile> obj_files, File exe_file);
    Linker (std::vector<ObjectFile> obj_files, File exe_file, File ld_file);

  private:
    std::vector<ObjectFile> m_obj_files;

    File m_exe_file;
    File m_ld_file;

    struct Token
    {
        enum class Type
        {
            WHITESPACE,
            ENTRY,
            SECTIONS,
            TEXT,
            DATA,
            BSS,
            SECTION_COUNTER,
            LITERAL_NUMBER_BINARY,
            LITERAL_NUMBER_DECIMAL,
            LITERAL_NUMBER_HEXADECIMAL,
            OPEN_PARENTHESIS,
            CLOSE_PARENTHESIS,
            SEMI_COLON,
            COMMA,
            EQUAL,
            AT,
            SYMBOL,
        };

        Type type;
        std::string val;

        Token (Type type, std::string val);
    };

    static const std::vector<std::pair<std::string, Token::Type>> TOKEN_SPEC;

    std::vector<Token> m_tokens;

    std::string entry_symbol = "_start";

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

    bool physical = false;
    std::vector<SectionAddress> sections;

    void link ();
    void tokenize_ld ();
    void parse_ld ();
    void _entry (size_t &tok_i);
    void _sections (size_t &tok_i);

    word parse_value (size_t &tok_i);
    void skip_tokens (size_t &tok_i, const std::string &regex);
    void skip_tokens (size_t &tok_i, const std::set<Token::Type> &tokenTypes);
    bool expect_token (size_t tok_i, const std::string &errorMsg);
    bool expect_token (size_t tok_i, const std::set<Token::Type> &tokenTypes,
                       const std::string &errorMsg);
    bool is_token (size_t tok_i, const std::set<Token::Type> &tokenTypes,
                   const std::string &errorMsg = "Linker::is_token() - Unexpected end of file");
    bool in_bounds (size_t tok_i);
    Token &consume (size_t &tok_i,
                    const std::string &errorMsg = "Linker::consume() - Unexpected end of file");
    Token &consume (size_t &tok_i, const std::set<Token::Type> &expectedTypes,
                    const std::string &errorMsg = "Linker::consume() - Unexpected token");
};