#include "assembler/assembler.h"
#include "util/logger.h"

#include <string>

using basm::Token;
using basm::TokenType;

void Assembler::fail(const Token &at, const std::string &message)
{
    m_state = State::ASSEMBLER_ERROR;
    basm::fatal_at(*m_sources, at.loc, message);
}

void Assembler::warn(const Token &at, const std::string &message)
{
    if (m_state != State::ASSEMBLER_ERROR) m_state = State::ASSEMBLER_WARNING;
    std::string text =
        basm::format_diagnostic(*m_sources, {basm::Severity::WARNING, at.loc, message});
    while (!text.empty() && text.back() == '\n') text.pop_back();
    AEMU_WARN("{}", text);
}

const Token &Assembler::expect(TokenType type, const std::string &message)
{
    if (!m_cursor.check(type))
    {
        fail(m_cursor.peek(), message + ", got " + basm::describe(m_cursor.peek()));
    }
    return m_cursor.next();
}

void Assembler::check(bool condition, const std::string &message)
{
    if (!condition)
    {
        fail(m_statement != nullptr ? *m_statement : m_cursor.peek(), message);
    }
}

void Assembler::expect_end_of_statement()
{
    if (!m_cursor.check(TokenType::NEWLINE) && !m_cursor.at_end())
    {
        fail(m_cursor.peek(),
             "unexpected " + basm::describe(m_cursor.peek()) + " at the end of the statement");
    }
}

///
/// @brief
/// @todo               Implement full expression parser.
///
/// @return             Value of expression.
///
dword Assembler::parse_expression(dword min, dword max)
{
    AEMU_DEBUG("Assembler::parse_expression() - Parsing expression.");

    // For now, only parse expressions sequentially, without care of precedence.
    dword exp_value = 0;
    const Token *operator_token = nullptr;
    while (true)
    {
        const Token &token = m_cursor.peek();
        if (!basm::is_integer_literal(token.type) && !token.is(TokenType::LITERAL_CHAR))
        {
            fail(token, (operator_token != nullptr
                             ? "expected an operand after '" + operator_token->str() + "'"
                             : std::string("expected a number"))
                            + ", got " + basm::describe(token));
        }
        m_cursor.next();
        const dword value = token.int_value;

        if (operator_token != nullptr)
        {
            switch (operator_token->type)
            {
            case TokenType::OPERATOR_ADDITION:
                exp_value += value;
                break;
            case TokenType::OPERATOR_SUBTRACTION:
                exp_value -= value;
                break;
            case TokenType::OPERATOR_DIVISION:
                if (value == 0)
                {
                    fail(*operator_token, "division by zero");
                }
                exp_value /= value;
                break;
            case TokenType::OPERATOR_MULTIPLICATION:
                exp_value *= value;
                break;
            default:
                fail(*operator_token,
                     "expected an operator, got " + basm::describe(*operator_token));
            }
        }
        else
        {
            exp_value = value;
        }

        // Temporary only support 4 operations.
        if (m_cursor.check_any({TokenType::OPERATOR_ADDITION, TokenType::OPERATOR_DIVISION,
                                TokenType::OPERATOR_MULTIPLICATION,
                                TokenType::OPERATOR_SUBTRACTION}))
        {
            operator_token = &m_cursor.next();
        }
        else
        {
            break;
        }
    }

    if (exp_value < min || exp_value > max)
    {
        m_state = Assembler::State::ASSEMBLER_WARNING;
        AEMU_WARN("Assembler::parse_expression() - Parsed value {} is outside of the target range "
                  "{} - {}.",
                  exp_value, min, max);
    }
    else
    {
        AEMU_DEBUG("Assembler::parse_expression() - Parsed value {}.", exp_value);
    }

    return exp_value;
}

///
/// @brief               Declares a symbol to be global outside this compilation unit.
///                      Must be declared outside any defined sections like .text, .bss, and .data.
/// USAGE:               .global <symbol>
///
void Assembler::_global()
{
    check(m_cur_section == Section::NONE,
          "cannot declare a symbol as global inside a section, declare it outside of .text, "
          ".bss, and .data");

    m_cursor.next();

    const std::string symbol = expect(TokenType::SYMBOL, "expected a symbol after .global").str();
    m_obj.add_symbol(symbol, 0, ObjectFile::SymbolTableEntry::BindingInfo::GLOBAL);
}

///
/// @brief                Declares a symbol to exist in another compilation unit but not defined here.
///                         Symbol's binding info will be marked as weak.
///                         Must be declared outside any defined sections like .text, .bss, and .data.
/// USAGE:                .extern <symbol>
///
void Assembler::_extern()
{
    check(m_cur_section == Section::NONE,
          "cannot declare a symbol as extern inside a section, declare it outside of .text, "
          ".bss, and .data");

    m_cursor.next();

    const std::string symbol = expect(TokenType::SYMBOL, "expected a symbol after .extern").str();
    m_obj.add_symbol(symbol, 0, ObjectFile::SymbolTableEntry::BindingInfo::WEAK);
}

///
/// @brief                 Moves where the assembler is in a section. Can only move forward, not backward.
/// USAGE:                .org <expression>
///
void Assembler::_org()
{
    m_cursor.next();

    const word val = parse_expression();

    if (val >= 0xffffff)
    {
        // Safety exit. Likely unintentional behavior.
        fail(*m_statement,
             "new value is large and likely unintentional (" + std::to_string(val) + ")");
    }

    switch (m_cur_section)
    {
    case Section::BSS:
        check(val >= m_obj.bss_section, ".org cannot move the assembler backwards, expected >= "
                                            + std::to_string(m_obj.bss_section) + ", got "
                                            + std::to_string(val));
        m_obj.bss_section = val;
        break;
    case Section::DATA:
        check(val >= m_obj.data_section.size(),
              ".org cannot move the assembler backwards, expected >= "
                  + std::to_string(m_obj.data_section.size()) + ", got " + std::to_string(val));
        for (size_t i = m_obj.data_section.size(); i < val; i++)
        {
            m_obj.data_section.push_back(0);
        }
        break;
    case Section::TEXT:
        // It is likely not very useful to allow .org to move pc in a text section,
        // comparatively to .data and .bss.
        check(val >= m_obj.text_section.size() * 4,
              ".org cannot move the assembler backwards, expected >= "
                  + std::to_string(m_obj.text_section.size() * 4) + ", got " + std::to_string(val));
        check(val % 4 == 0, ".org cannot move to a byte that is not word aligned in .text, got "
                                + std::to_string(val));

        for (size_t i = m_obj.text_section.size() * 4; i < val; i += 4)
        {
            m_obj.text_section.push_back(0);
        }
        break;
    case Section::NONE:
        fail(*m_statement, ".org is not inside a section, there is nothing to move");
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
void Assembler::_scope()
{
    m_cursor.next();
    m_scopes.push_back(m_total_scopes++);
}

///
/// @brief                  Ends a local scope.
/// USAGE:                  .scend
///
void Assembler::_scend()
{
    check(!m_scopes.empty(), ".scend must have a matching .scope");

    m_cursor.next();
    m_scopes.pop_back();
}

///
/// @brief                  Moves where the assembler is in a section forward by a certain amount of bytes.
/// USAGE:                  .advance <expression>
///
void Assembler::_advance()
{
    m_cursor.next();

    const word val = parse_expression();
    if (val >= 0xffffff)
    {
        // Safety exit. Likely unintentional behavior.
        AEMU_WARN("Assembler::_advance() - offset value is large and likely unintentional. ({}).",
                  val);
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
            m_obj.data_section.push_back(0);
        }
        break;
    case Section::TEXT:
        // It is likely not very useful to allow .org to move pc in a text section,
        // comparatively to .data and .bss.
        check(val % 4 == 0, ".advance cannot move to a byte that is not word aligned in .text, got "
                                + std::to_string(val));

        for (word i = 0; i < val; i += 4)
        {
            m_obj.text_section.push_back(0);
        }
        break;
    case Section::NONE:
        fail(*m_statement, ".advance is not inside a section, there is nothing to move");
    }
}

///
/// @brief                  Aligns where the assembler is in the current section.
/// @note                   This is useless unless we can specify in the program header of the
///                         object file the alignment of the whole program
/// USAGE:                  .align <expression>
///
void Assembler::_align()
{
    m_cursor.next();

    const word val = parse_expression();
    if (val >= 0xffff)
    {
        // Safety exit. Likely unintentional behavior.
        AEMU_WARN("Assembler::_align() - Alignment value is large and likely unintentional. ({}).",
                  val);
        m_state = State::ASSEMBLER_WARNING;
        return;
    }
    check(val != 0, ".align expects a non-zero alignment");

    switch (m_cur_section)
    {
    case Section::BSS:
        m_obj.bss_section += (val - (m_obj.bss_section % val)) % val;
        break;
    case Section::DATA:
        while (m_obj.data_section.size() % val != 0)
        {
            m_obj.data_section.push_back(0);
        }
        break;
    case Section::TEXT:
        // It is likely not very useful to allow .org to move pc in a text section,
        // comparatively to .data and .bss.
        check(val % 4 == 0, ".align in .text must be a multiple of 4, got " + std::to_string(val));

        while (m_obj.text_section.size() * 4 % val != 0)
        {
            m_obj.text_section.push_back(0);
        }
        break;
    case Section::NONE:
        fail(*m_statement, ".align is not inside a section, there is nothing to align");
    }
}

///
/// @brief                  Creates a new section.
/// @warning                Not implemented yet.
/// USAGE:                  .section <string>, <flags>
///
void Assembler::_section()
{
    m_cursor.next();

    expect(TokenType::LITERAL_STRING, ".section expects a string argument");

    fail(*m_statement, ".section is not implemented yet");
}

///
/// @brief                  Creates a new text section.
/// @warning                Currently will simply add on to the previously defined text section if it exists.
/// USAGE:                  .text
///
void Assembler::_text()
{
    m_cursor.next();

    m_cur_section = Section::TEXT;
    m_cur_section_index = m_obj.section_table[".text"];
}

///
/// @brief                  Creates a new data section.
/// @warning                Currently will simply add on to the previously defined data section if it exists
/// USAGE:                  .data
///
void Assembler::_data()
{
    m_cursor.next();

    m_cur_section = Section::DATA;
    m_cur_section_index = m_obj.section_table[".data"];
}

///
/// @brief                  Creates a new bss section.
/// @warning                Currently will simply add on to the previously defined bss section if it exists
/// USAGE:                  .bss
///
void Assembler::_bss()
{
    m_cursor.next();

    m_cur_section = Section::BSS;
    m_cur_section_index = m_obj.section_table[".bss"];
}

///
/// @brief                  Stops assembling
/// USAGE:                  .stop
///
void Assembler::_stop()
{
    while (!m_cursor.at_end())
    {
        m_cursor.next();
    }
}

std::vector<dword> Assembler::parse_arguments()
{
    std::vector<dword> args;
    while (!m_cursor.at_end() && !m_cursor.check(TokenType::NEWLINE))
    {
        args.push_back(parse_expression());
        if (!m_cursor.accept(TokenType::COMMA))
        {
            break;
        }
    }
    return args;
}

std::vector<byte> convert_little_endian(std::vector<dword> data, U8 n_bytes)
{
    std::vector<byte> little_endian_data;

    for (size_t i = 0; i < data.size(); i++)
    {
        for (U8 j = 0; j < n_bytes; j++)
        {
            little_endian_data.push_back(data.at(i) & 0xFF);
            data.at(i) >>= 8;
        }
    }

    return little_endian_data;
}

void Assembler::define_data(const char *directive, U8 n_bytes)
{
    check(m_cur_section == Section::DATA,
          std::string(directive) + " can only define data in the .data section");

    m_cursor.next();

    const std::vector<byte> data = convert_little_endian(parse_arguments(), n_bytes);
    for (size_t i = 0; i < data.size(); i++)
    {
        m_obj.data_section.push_back(data.at(i));
    }
}

void Assembler::_byte()
{
    define_data(".byte", 1);
}

void Assembler::_dbyte()
{
    define_data(".dbyte", 2);
}

void Assembler::_word()
{
    define_data(".word", 4);
}

void Assembler::_dword()
{
    define_data(".dword", 8);
}

// TODO: This is pointless, same as .byte.
void Assembler::_sbyte()
{
    define_data(".sbyte", 1);
}

// TODO: Figure out why signed versions of these data defining directives are needed.
void Assembler::_sdbyte()
{
    define_data(".sdbyte", 2);
}

void Assembler::_sword()
{
    define_data(".sword", 4);
}

void Assembler::_sdword()
{
    define_data(".sdword", 8);
}

void Assembler::_char()
{
    check(m_cur_section == Section::DATA, ".char can only define data in the .data section");
    m_cursor.next();

    do
    {
        const Token &literal = expect(TokenType::LITERAL_CHAR, "expected a character literal");
        m_obj.data_section.push_back(static_cast<byte>(literal.int_value));
    } while (m_cursor.accept(TokenType::COMMA));
}

void Assembler::_ascii()
{
    check(m_cur_section == Section::DATA, ".ascii can only define data in the .data section");
    m_cursor.next();

    // Note, does not automatically add the null terminator.
    do
    {
        const std::string str = basm::unescape_string_literal(
            expect(TokenType::LITERAL_STRING, "expected a string literal"));
        for (const char c : str)
        {
            m_obj.data_section.push_back(static_cast<byte>(c));
        }
    } while (m_cursor.accept(TokenType::COMMA));
}

void Assembler::_asciz()
{
    check(m_cur_section == Section::DATA, ".asciz can only define data in the .data section");
    m_cursor.next();

    do
    {
        const std::string str = basm::unescape_string_literal(
            expect(TokenType::LITERAL_STRING, "expected a string literal"));
        for (const char c : str)
        {
            m_obj.data_section.push_back(static_cast<byte>(c));
        }
        m_obj.data_section.push_back('\0');
    } while (m_cursor.accept(TokenType::COMMA));
}
