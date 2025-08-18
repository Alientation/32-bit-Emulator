#include "assembler/assembler.h"
#include "util/logger.h"

#include <string>

///
/// @brief
/// @todo               Implement full expression parser.
///
/// @return             Value of expression.
///
dword Assembler::parse_expression (dword min, dword max)
{
    DEBUG ("Assembler::parse_expression() - Parsing expression.");

    // For now, only parse expressions sequentially, without care of precedence.
    dword exp_value = 0;
    Tokenizer::Token *operator_token = nullptr;
    while (true)
    {
        const Tokenizer::Token &token = m_tokenizer.consume ();

        dword value = 0;
        if (token.type == Tokenizer::LITERAL_NUMBER_DECIMAL)
        {
            value = std::stoull (token.value);
        }
        else if (token.type == Tokenizer::LITERAL_NUMBER_HEXADECIMAL)
        {
            value = std::stoull (token.value.substr (1), nullptr, 16);
        }
        else if (token.type == Tokenizer::LITERAL_NUMBER_BINARY)
        {
            value = std::stoull (token.value.substr (1), nullptr, 2);
        }
        else if (token.type == Tokenizer::LITERAL_NUMBER_OCTAL)
        {
            value = std::stoull (token.value.substr (1), nullptr, 8);
        }
        else if (operator_token != nullptr)
        {
            ERROR ("Assembler::parse_expression() - Expected operand to follow operator.");
        }

        if (operator_token != nullptr)
        {
            switch (operator_token->type)
            {
            case Tokenizer::OPERATOR_ADDITION:
                exp_value += value;
                break;
            case Tokenizer::OPERATOR_SUBTRACTION:
                exp_value -= value;
                break;
            case Tokenizer::OPERATOR_DIVISION:
                exp_value /= value;
                break;
            case Tokenizer::OPERATOR_MULTIPLICATION:
                exp_value *= value;
                break;
            default:
                ERROR ("Assembler::parse_expression() - Expected operator token but got %s",
                       operator_token->value.c_str ());
            }
            operator_token = nullptr;
        }
        else
        {
            exp_value = value;
        }

        // Temporary only support 4 operations.
        if (m_tokenizer.is_next ({Tokenizer::OPERATOR_ADDITION, Tokenizer::OPERATOR_DIVISION,
                                  Tokenizer::OPERATOR_MULTIPLICATION,
                                  Tokenizer::OPERATOR_SUBTRACTION}))
        {
            operator_token = &m_tokenizer.consume ();
        }
        else
        {
            break;
        }
    }

    if (exp_value < min || exp_value > max)
    {
        m_state = Assembler::State::ASSEMBLER_WARNING;
        WARN ("Assembler::parse_expression() - Parsed value %llu is outside of the target range "
              "%llu - %llu.",
              exp_value, min, max);
    }
    else
    {
        DEBUG ("Assembler::parse_expression() - Parsed value %llu.", exp_value);
    }

    return exp_value;
}

///
/// @brief               Declares a symbol to be global outside this compilation unit.
///                      Must be declared outside any defined sections like .text, .bss, and .data.
/// USAGE:               .global <symbol>
///
void Assembler::_global ()
{
    if (m_cur_section != Section::NONE)
    {
        ERROR ("Assembler::_global() - Cannot declare symbol as global "
               "inside a section. Must be declared outside of .text, .bss, and .data.");
        m_state = State::ASSEMBLER_ERROR;
        return;
    }

    m_tokenizer.consume ();

    const std::string symbol = m_tokenizer.consume ().value;
    m_obj.add_symbol (symbol, 0, ObjectFile::SymbolTableEntry::BindingInfo::GLOBAL);
}

///
/// @brief                Declares a symbol to exist in another compilation unit but not defined here.
///                         Symbol's binding info will be marked as weak.
///                         Must be declared outside any defined sections like .text, .bss, and .data.
/// USAGE:                .extern <symbol>
///
void Assembler::_extern ()
{
    if (m_cur_section != Section::NONE)
    {
        ERROR ("Assembler::_extern() - Cannot "
               "declare symbol as extern inside a section. Must be declared outside of "
               ".text, .bss, and .data.");
        m_state = State::ASSEMBLER_ERROR;
        return;
    }

    m_tokenizer.consume ();

    const std::string symbol = m_tokenizer.consume ().value;
    m_obj.add_symbol (symbol, 0, ObjectFile::SymbolTableEntry::BindingInfo::WEAK);
}

///
/// @brief                 Moves where the assembler is in a section. Can only move forward, not backward.
/// USAGE:                .org <expression>
///
void Assembler::_org ()
{
    m_tokenizer.consume ();

    const word val = parse_expression ();

    if (val >= 0xffffff)
    {
        // Safety exit. Likely unintentional behavior.
        WARN ("Assembler::_org() - new value is large and likely unintentional. (%d).", val);
        m_state = State::ASSEMBLER_WARNING;
        return;
    }

    switch (m_cur_section)
    {
    case Section::BSS:
        if (val < m_obj.bss_section)
        {
            ERROR ("Assembler::_org() - .org directive cannot move "
                   "assembler pc backwards. Expected >= %u. Got %u.",
                   m_obj.bss_section, val);
            m_state = State::ASSEMBLER_ERROR;
            return;
        }
        m_obj.bss_section = val;
        break;
    case Section::DATA:
        if (val < m_obj.data_section.size ())
        {
            ERROR ("Assembler::_org() - .org directive cannot move "
                   "assembler pc backwards. Expected >= %llu. Got %u.",
                   m_obj.data_section.size (), val);
            m_state = State::ASSEMBLER_ERROR;
            return;
        }
        for (size_t i = m_obj.data_section.size (); i < val; i++)
        {
            m_obj.data_section.push_back (0);
        }
        break;
    case Section::TEXT:
        // It is likely not very useful to allow .org to move pc in a text section,
        // comparatively to .data and .bss.
        if (val < m_obj.text_section.size () * 4)
        {
            ERROR ("Assembler::_org() - .org directive cannot move "
                   "assembler pc backwards. Expected >= %llu. Got %u.",
                   m_obj.text_section.size () * 4, val);
            m_state = State::ASSEMBLER_ERROR;
            return;
        }

        if (val % 4 != 0)
        {
            ERROR ("Assembler::_org() - .org directive cannot move "
                   "assembler pc to a non-word aligned byte in .text section. Expected aligned "
                   "4 byte. Got %u.",
                   val);
            m_state = State::ASSEMBLER_ERROR;
            return;
        }

        for (size_t i = m_obj.text_section.size () * 4; i < val; i += 4)
        {
            m_obj.text_section.push_back (0);
        }
        break;
    case Section::NONE:
        ERROR ("Assembler::_org() - Not defined inside section. Cannot move section pointer.");
        m_state = State::ASSEMBLER_ERROR;
        return;
    }
}

///
/// @brief                  Defines a local scope. Any symbol defined inside will be marked as
///                         local and will not be able to be marked as global. Symbols defined
///                         here will be postfixed with a special identifier <symbol>:<scope_id>.
///                         Local symbols defined at current scope level or above will have
///                         higher precedence over globally defined symbols.
/// USAGE:                  .scope
///
void Assembler::_scope ()
{
    m_tokenizer.consume ();
    m_scopes.push_back (m_total_scopes++);
}

///
/// @brief                  Ends a local scope.
/// USAGE:                  .scend
///
void Assembler::_scend ()
{
    if (m_scopes.empty ())
    {
        ERROR ("Assembler::_scend() - .scend directive must have a matching .scope directive.");
        m_state = State::ASSEMBLER_ERROR;
        return;
    }

    m_tokenizer.consume ();
    m_scopes.pop_back ();
}

///
/// @brief                  Moves where the assembler is in a section forward by a certain amount of bytes.
/// USAGE:                  .advance <expression>
///
void Assembler::_advance ()
{
    m_tokenizer.consume ();

    const word val = parse_expression ();
    if (val >= 0xffffff)
    {
        // Safety exit. Likely unintentional behavior.
        WARN ("Assembler::_advance() - offset value is large and likely unintentional. (%u).", val);
        m_state = State::ASSEMBLER_WARNING;
        return;
    }

    switch (m_cur_section)
    {
    case Section::BSS:
        m_obj.bss_section += val;
        break;
    case Section::DATA:
        for (word i = 0; i < val; i++)
        {
            m_obj.data_section.push_back (0);
        }
        break;
    case Section::TEXT:
        // It is likely not very useful to allow .org to move pc in a text section,
        // comparatively to .data and .bss.
        if (val % 4 != 0)
        {
            ERROR ("Assembler::_advance() - .advance directive cannot"
                   " move assembler pc to a non-word aligned byte in .text section. Expected "
                   "aligned 4 byte."
                   " Got %u.",
                   val);
            m_state = State::ASSEMBLER_ERROR;
            return;
        }

        for (word i = 0; i < val; i += 4)
        {
            m_obj.text_section.push_back (0);
        }
        break;
    case Section::NONE:
        ERROR ("Assembler::_advance() - Not defined inside section. Cannot move section pointer.");
        m_state = State::ASSEMBLER_ERROR;
        return;
    }
}

///
/// @brief                  Aligns where the assembler is in the current section.
/// @note                   This is useless unless we can specify in the program header of the
///                         object file the alignment of the whole program
/// USAGE:                  .align <expression>
///
void Assembler::_align ()
{
    m_tokenizer.consume ();

    const word val = parse_expression ();
    if (val >= 0xffff)
    {
        // Safety exit. Likely unintentional behavior.
        WARN ("Assembler::_align() - Alignment value is large and likely unintentional. (%u).",
              val);
        m_state = State::ASSEMBLER_WARNING;
        return;
    }

    switch (m_cur_section)
    {
    case Section::BSS:
        m_obj.bss_section += (val - (m_obj.bss_section % val)) % val;
        break;
    case Section::DATA:
        while (m_obj.data_section.size () % val != 0)
        {
            m_obj.data_section.push_back (0);
        }
        break;
    case Section::TEXT:
        // It is likely not very useful to allow .org to move pc in a text section,
        // comparatively to .data and .bss.
        if (val % 4 != 0)
        {
            ERROR ("Assembler::_advance() - .advance directive cannot "
                   "move assembler pc to a non-word aligned byte in .text section. Expected "
                   "aligned 4 byte."
                   " Got %u.",
                   val);
            m_state = State::ASSEMBLER_ERROR;
            return;
        }

        while (m_obj.text_section.size () * 4 % val != 0)
        {
            m_obj.text_section.push_back (0);
        }
        break;
    case Section::NONE:
        ERROR ("Assembler::_align() - Not defined inside a section. Cannot align section pointer.");
        m_state = State::ASSEMBLER_ERROR;
        return;
    }
}

///
/// @brief                  Creates a new section.
/// @warning                Not implemented yet.
/// USAGE:                  .section <string>, <flags>
///
void Assembler::_section ()
{
    m_tokenizer.consume ();

    m_tokenizer.expect_next (Tokenizer::LITERAL_STRING,
                             "Assembler::_section() - .section expects a "
                             "string argument to follow.");

    ERROR ("Assembler::_section() - .section directive is not implemented yet.");
    m_state = State::ASSEMBLER_ERROR;
    return;
}

///
/// @brief                  Creates a new text section.
/// @warning                Currently will simply add on to the previously defined text section if it exists.
/// USAGE:                  .text
///
void Assembler::_text ()
{
    m_tokenizer.consume ();

    m_cur_section = Section::TEXT;
    m_cur_section_index = m_obj.section_table[".text"];
}

///
/// @brief                  Creates a new data section.
/// @warning                Currently will simply add on to the previously defined data section if it exists
/// USAGE:                  .data
///
void Assembler::_data ()
{
    m_tokenizer.consume ();

    m_cur_section = Section::DATA;
    m_cur_section_index = m_obj.section_table[".data"];
}

///
/// @brief                  Creates a new bss section.
/// @warning                Currently will simply add on to the previously defined bss section if it exists
/// USAGE:                  .bss
///
void Assembler::_bss ()
{
    m_tokenizer.consume ();

    m_cur_section = Section::BSS;
    m_cur_section_index = m_obj.section_table[".bss"];
}

///
/// @brief                  Stops assembling
/// USAGE:                  .stop
///
void Assembler::_stop ()
{
    m_tokenizer.set_toki (m_tokenizer.get_tokens ().size ());
}

std::vector<dword> Assembler::parse_arguments ()
{
    std::vector<dword> args;
    while (!m_tokenizer.is_next (Tokenizer::WHITESPACE_NEWLINE))
    {
        args.push_back (parse_expression ());
        if (m_tokenizer.is_next (Tokenizer::COMMA))
        {
            m_tokenizer.consume ();
        }
        else
        {
            break;
        }
    }
    return args;
}

std::vector<byte> convert_little_endian (std::vector<dword> data, U8 n_bytes)
{
    std::vector<byte> little_endian_data;

    for (size_t i = 0; i < data.size (); i++)
    {
        for (U8 j = 0; j < n_bytes; j++)
        {
            little_endian_data.push_back (data.at (i) & 0xFF);
            data.at (i) >>= 8;
        }
    }

    return little_endian_data;
}

void Assembler::_byte ()
{
    EXPECT_TRUE_SS (m_cur_section == Section::DATA,
                    std::stringstream ()
                        << "Assembler::_byte() - Can only define data in .data section.");

    m_tokenizer.consume ();

    const std::vector<byte> data = convert_little_endian (parse_arguments (), 1);
    for (size_t i = 0; i < data.size (); i++)
    {
        m_obj.data_section.push_back (data.at (i));
    }
}

void Assembler::_dbyte ()
{
    EXPECT_TRUE_SS (m_cur_section == Section::DATA,
                    std::stringstream ()
                        << "Assembler::_dbyte() - Can only define data in .data section.");

    m_tokenizer.consume ();

    const std::vector<byte> data = convert_little_endian (parse_arguments (), 2);
    for (size_t i = 0; i < data.size (); i++)
    {
        m_obj.data_section.push_back (data.at (i));
    }
}

void Assembler::_word ()
{
    EXPECT_TRUE_SS (m_cur_section == Section::DATA,
                    std::stringstream ()
                        << "Assembler::_word() - Can only define data in .data section.");

    m_tokenizer.consume ();

    std::vector<byte> data = convert_little_endian (parse_arguments (), 4);
    for (size_t i = 0; i < data.size (); i++)
    {
        m_obj.data_section.push_back (data.at (i));
    }
}

void Assembler::_dword ()
{
    EXPECT_TRUE_SS (m_cur_section == Section::DATA,
                    std::stringstream ()
                        << "Assembler::_dword() - Can only define data in .data section.");

    m_tokenizer.consume ();

    const std::vector<byte> data = convert_little_endian (parse_arguments (), 8);
    for (size_t i = 0; i < data.size (); i++)
    {
        m_obj.data_section.push_back (data.at (i));
    }
}

// TODO: This is pointless, same as .byte.
void Assembler::_sbyte ()
{
    EXPECT_TRUE_SS (m_cur_section == Section::DATA,
                    std::stringstream ()
                        << "Assembler::_sbyte() - Can only define data in .data section.");

    m_tokenizer.consume ();

    const std::vector<byte> data = convert_little_endian (parse_arguments (), 1);
    for (size_t i = 0; i < data.size (); i++)
    {
        m_obj.data_section.push_back (data.at (i));
    }
}

// TODO: Figure out why signed versions of these data defining directives are needed.
void Assembler::_sdbyte ()
{
    EXPECT_TRUE_SS (m_cur_section == Section::DATA,
                    std::stringstream ()
                        << "Assembler::_sdbyte() - Can only define data in .data section.");

    m_tokenizer.consume ();

    const std::vector<byte> data = convert_little_endian (parse_arguments (), 2);
    for (size_t i = 0; i < data.size (); i++)
    {
        m_obj.data_section.push_back (data.at (i));
    }
}

void Assembler::_sword ()
{
    EXPECT_TRUE_SS (m_cur_section == Section::DATA,
                    std::stringstream ()
                        << "Assembler::_sword() - Can only define data in .data section.");

    m_tokenizer.consume ();

    const std::vector<byte> data = convert_little_endian (parse_arguments (), 4);
    for (size_t i = 0; i < data.size (); i++)
    {
        m_obj.data_section.push_back (data.at (i));
    }
}

void Assembler::_sdword ()
{
    EXPECT_TRUE_SS (m_cur_section == Section::DATA,
                    std::stringstream ()
                        << "Assembler::_sdword() - Can only define data in .data section.");

    m_tokenizer.consume ();

    const std::vector<byte> data = convert_little_endian (parse_arguments (), 8);
    for (size_t i = 0; i < data.size (); i++)
    {
        m_obj.data_section.push_back (data.at (i));
    }
}

void Assembler::_char ()
{
    EXPECT_TRUE_SS (m_cur_section == Section::DATA,
                    std::stringstream ()
                        << "Assembler::_char() - Can only define data in .data section.");
    m_tokenizer.consume ();

    while (true)
    {
        m_tokenizer.expect_next (Tokenizer::Type::LITERAL_CHAR,
                                 "Assembler::_char() - Expected literal"
                                 " char. Got "
                                     + m_tokenizer.get_token ().value);

        // Get the second character since literal chars are surrounded by single quotes.
        m_obj.data_section.push_back (m_tokenizer.consume ().value.at (1));
        if (m_tokenizer.is_next (Tokenizer::COMMA))
        {
            m_tokenizer.consume ();
        }
        else
        {
            break;
        }
    }
}

void Assembler::_ascii ()
{
    EXPECT_TRUE_SS (m_cur_section == Section::DATA,
                    std::stringstream ()
                        << "Assembler::_ascii() - Can only define data in .data section.");
    m_tokenizer.consume ();

    while (true)
    {
        m_tokenizer.expect_next (Tokenizer::Type::LITERAL_STRING,
                                 "Assembler::_ascii() - Expected "
                                 "literal string. Got "
                                     + m_tokenizer.get_token ().value);

        const std::string str = m_tokenizer.consume ().value;

        // Ignore the surrounding double quotes.
        for (size_t i = 1; i < str.size () - 1; i++)
        {
            m_obj.data_section.push_back (str[i]);
        }

        // Note, does not automatically add the null terminator.

        if (m_tokenizer.is_next (Tokenizer::COMMA))
        {
            m_tokenizer.consume ();
        }
        else
        {
            break;
        }
    }
}

void Assembler::_asciz ()
{
    EXPECT_TRUE_SS (m_cur_section == Section::DATA,
                    std::stringstream ()
                        << "Assembler::_asciz() - Can only define data in .data section.");
    m_tokenizer.consume ();

    while (true)
    {
        m_tokenizer.expect_next (Tokenizer::Type::LITERAL_STRING,
                                 "Assembler::_ascii() - Expected "
                                 "literal string. Got "
                                     + m_tokenizer.get_token ().value);

        const std::string str = m_tokenizer.consume ().value;

        // Ignore the surrounding double quotes.
        for (size_t i = 1; i < str.size () - 1; i++)
        {
            m_obj.data_section.push_back (str[i]);
        }
        m_obj.data_section.push_back ('\0');

        if (m_tokenizer.is_next (Tokenizer::COMMA))
        {
            m_tokenizer.consume ();
        }
        else
        {
            break;
        }
    }
}
