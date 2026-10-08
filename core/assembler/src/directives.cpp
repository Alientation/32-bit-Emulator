#include "assembler/assembler.h"
#include "util/logger.h"

#include <string>

using basm::Token;
using basm::TokenType;

void Assembler::fail(const Token &at, const std::string &message)
{
    basm::fatal_at(*m_sources, at.loc, message);
}

void Assembler::warn(const Token &at, const std::string &message)
{
    if (m_warnings_as_errors)
    {
        fail(at, message + " (a warning, and warnings are errors)");
    }

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

namespace
{

/// Binding strength of a binary operator, 0 if the token is not one. Same order as C.
int binary_precedence(TokenType type)
{
    switch (type)
    {
    case TokenType::OPERATOR_LOGICAL_OR:
        return 1;
    case TokenType::OPERATOR_LOGICAL_AND:
        return 2;
    case TokenType::OPERATOR_BITWISE_OR:
        return 3;
    case TokenType::OPERATOR_BITWISE_XOR:
        return 4;
    case TokenType::OPERATOR_BITWISE_AND:
        return 5;
    case TokenType::OPERATOR_LOGICAL_EQUAL:
    case TokenType::OPERATOR_LOGICAL_NOT_EQUAL:
        return 6;
    case TokenType::OPERATOR_LOGICAL_LESS_THAN:
    case TokenType::OPERATOR_LOGICAL_LESS_THAN_OR_EQUAL:
    case TokenType::OPERATOR_LOGICAL_GREATER_THAN:
    case TokenType::OPERATOR_LOGICAL_GREATER_THAN_OR_EQUAL:
        return 7;
    case TokenType::OPERATOR_BITWISE_LEFT_SHIFT:
    case TokenType::OPERATOR_BITWISE_RIGHT_SHIFT:
        return 8;
    case TokenType::OPERATOR_ADDITION:
    case TokenType::OPERATOR_SUBTRACTION:
        return 9;
    case TokenType::OPERATOR_MULTIPLICATION:
    case TokenType::OPERATOR_DIVISION:
    case TokenType::OPERATOR_MODULUS:
        return 10;
    default:
        return 0;
    }
}

/// How many parentheses and unary operators may nest, so that a hostile source cannot overflow
/// the stack.
constexpr int kMaxExpressionDepth = 200;

} // namespace

bool Assembler::at_expression()
{
    return m_cursor.peek().is_one_of({TokenType::LITERAL_CHAR, TokenType::OPEN_PARENTHESIS,
                                      TokenType::OPERATOR_SUBTRACTION, TokenType::OPERATOR_ADDITION,
                                      TokenType::OPERATOR_BITWISE_COMPLEMENT,
                                      TokenType::OPERATOR_LOGICAL_NOT, TokenType::SYMBOL})
           || basm::is_integer_literal(m_cursor.peek().type);
}

std::string Assembler::scoped_name(const std::string &name) const
{
    return m_scopes.empty() ? name : name + "::SCOPE:" + std::to_string(m_scopes.back());
}

void Assembler::require_number(const ExprValue &value)
{
    if (value.label != nullptr)
    {
        fail(*value.label,
             "the address of '" + value.label->str()
                 + "' is not known until linking; only the difference of two labels of the same "
                   "section is a number");
    }
}

///
/// @brief              Looks `symbol` up like a label is: the scopes that are open from the
///                     innermost out, then the file. A constant is a number and a label is its
///                     offset in its section. Both have to be defined already.
///
Assembler::ExprValue Assembler::lookup_symbol(const Token &symbol)
{
    const std::string name = symbol.str();
    for (size_t i = m_scopes.size() + 1; i-- > 0;)
    {
        const std::string key = i == 0 ? name : name + "::SCOPE:" + std::to_string(m_scopes[i - 1]);

        const auto constant = m_constants.find(key);
        if (constant != m_constants.end())
        {
            return {.value = constant->second};
        }

        const auto entry = m_obj.string_table.find(key);
        if (entry != m_obj.string_table.end())
        {
            const ObjectFile::SymbolTableEntry &label = m_obj.symbol_table.at(entry->second);
            if (label.section != U32(-1))
            {
                return {.value = sdword(label.symbol_value),
                        .label = &symbol,
                        .section = label.section};
            }
        }
    }
    fail(symbol, "'" + name
                     + "' is not defined before this point; an expression can use a constant "
                       "(.equ) or a label that comes earlier in the file");
}

bool Assembler::is_constant(const std::string &name) const
{
    for (size_t i = m_scopes.size() + 1; i-- > 0;)
    {
        const std::string key = i == 0 ? name : name + "::SCOPE:" + std::to_string(m_scopes[i - 1]);
        if (m_constants.count(key) != 0)
        {
            return true;
        }
        if (m_obj.string_table.count(key) != 0)
        {
            // A label of this name shadows a constant of an outer scope.
            return false;
        }
    }
    return false;
}

///
/// @brief              operand := number | char | constant | label
///                             | '(' expression ')' | ('-' | '+' | '~' | '!') operand
///
Assembler::ExprValue Assembler::parse_unary_expression()
{
    const Token &token = m_cursor.peek();
    if (!at_expression())
    {
        fail(token, "expected a number, got " + basm::describe(token));
    }
    check(m_expression_depth < kMaxExpressionDepth, "the expression is nested too deeply");

    struct Depth
    {
        int &depth;

        explicit Depth(int &d) :
            depth(d)
        {
            depth++;
        }

        ~Depth()
        {
            depth--;
        }
    } nesting(m_expression_depth);

    m_cursor.next();
    switch (token.type)
    {
    case TokenType::OPERATOR_SUBTRACTION:
    {
        const ExprValue operand = parse_unary_expression();
        require_number(operand);
        return {.value = sdword(0 - U64(operand.value))};
    }
    case TokenType::OPERATOR_ADDITION:
        return parse_unary_expression();
    case TokenType::OPERATOR_BITWISE_COMPLEMENT:
    {
        const ExprValue operand = parse_unary_expression();
        require_number(operand);
        return {.value = ~operand.value};
    }
    case TokenType::OPERATOR_LOGICAL_NOT:
    {
        const ExprValue operand = parse_unary_expression();
        require_number(operand);
        return {.value = operand.value == 0 ? 1 : 0};
    }
    case TokenType::SYMBOL:
        return lookup_symbol(token);
    case TokenType::OPEN_PARENTHESIS:
    {
        const ExprValue value = parse_binary_expression(1);
        if (!m_cursor.accept(TokenType::CLOSE_PARENTHESIS))
        {
            fail(m_cursor.peek(), "expected ')' to close the '(' at column "
                                      + std::to_string(token.loc.column) + ", got "
                                      + basm::describe(m_cursor.peek()));
        }
        return value;
    }
    default:
        // A number or a character.
        return {.value = sdword(token.int_value)};
    }
}

///
/// @brief              Precedence climbing. All the binary operators associate to the left.
///
Assembler::ExprValue Assembler::parse_binary_expression(int min_precedence)
{
    ExprValue left = parse_unary_expression();
    while (true)
    {
        const Token &op = m_cursor.peek();
        const int precedence = binary_precedence(op.type);
        if (precedence == 0 || precedence < min_precedence)
        {
            return left;
        }
        m_cursor.next();
        if (!at_expression())
        {
            fail(m_cursor.peek(), "expected an operand after '" + op.str() + "', got "
                                      + basm::describe(m_cursor.peek()));
        }
        const ExprValue right = parse_binary_expression(precedence + 1);

        if (left.label != nullptr || right.label != nullptr)
        {
            // The difference of two labels of a section does not depend on where the section
            // ends up. Nothing else can be done with an address yet.
            if (op.type == TokenType::OPERATOR_SUBTRACTION && left.label != nullptr
                && right.label != nullptr)
            {
                if (left.section != right.section)
                {
                    fail(op, "'" + left.label->str() + "' and '" + right.label->str()
                                 + "' are in different sections, so their distance is not known "
                                   "until linking");
                }
                left = {.value = sdword(U64(left.value) - U64(right.value))};
                continue;
            }
            require_number(left);
            require_number(right);
        }

        sdword lhs = left.value;
        const sdword rhs = right.value;

        // The arithmetic wraps around, done on unsigned values where signed would overflow.
        const U64 l = U64(lhs);
        const U64 r = U64(rhs);
        switch (op.type)
        {
        case TokenType::OPERATOR_ADDITION:
            lhs = sdword(l + r);
            break;
        case TokenType::OPERATOR_SUBTRACTION:
            lhs = sdword(l - r);
            break;
        case TokenType::OPERATOR_MULTIPLICATION:
            lhs = sdword(l * r);
            break;
        case TokenType::OPERATOR_DIVISION:
        case TokenType::OPERATOR_MODULUS:
            if (rhs == 0)
            {
                fail(op, op.type == TokenType::OPERATOR_DIVISION
                             ? "division by zero"
                             : "remainder of a division by zero");
            }
            if (rhs == -1)
            {
                // The most negative number divided by -1 does not fit.
                lhs = op.type == TokenType::OPERATOR_DIVISION ? sdword(0 - l) : 0;
            }
            else
            {
                lhs = op.type == TokenType::OPERATOR_DIVISION ? lhs / rhs : lhs % rhs;
            }
            break;
        case TokenType::OPERATOR_BITWISE_LEFT_SHIFT:
        case TokenType::OPERATOR_BITWISE_RIGHT_SHIFT:
            if (rhs < 0 || rhs >= 64)
            {
                fail(op, "the shift amount must be 0 to 63, got " + std::to_string(rhs));
            }
            lhs = op.type == TokenType::OPERATOR_BITWISE_LEFT_SHIFT ? sdword(l << rhs) : lhs >> rhs;
            break;
        case TokenType::OPERATOR_BITWISE_AND:
            lhs = sdword(l & r);
            break;
        case TokenType::OPERATOR_BITWISE_XOR:
            lhs = sdword(l ^ r);
            break;
        case TokenType::OPERATOR_BITWISE_OR:
            lhs = sdword(l | r);
            break;
        case TokenType::OPERATOR_LOGICAL_EQUAL:
            lhs = lhs == rhs;
            break;
        case TokenType::OPERATOR_LOGICAL_NOT_EQUAL:
            lhs = lhs != rhs;
            break;
        case TokenType::OPERATOR_LOGICAL_LESS_THAN:
            lhs = lhs < rhs;
            break;
        case TokenType::OPERATOR_LOGICAL_LESS_THAN_OR_EQUAL:
            lhs = lhs <= rhs;
            break;
        case TokenType::OPERATOR_LOGICAL_GREATER_THAN:
            lhs = lhs > rhs;
            break;
        case TokenType::OPERATOR_LOGICAL_GREATER_THAN_OR_EQUAL:
            lhs = lhs >= rhs;
            break;
        case TokenType::OPERATOR_LOGICAL_AND:
            lhs = lhs != 0 && rhs != 0;
            break;
        case TokenType::OPERATOR_LOGICAL_OR:
            lhs = lhs != 0 || rhs != 0;
            break;
        default:
            fail(op, "expected an operator, got " + basm::describe(op));
        }
        left = {.value = lhs};
    }
}

sdword Assembler::parse_signed_expression()
{
    const ExprValue value = parse_binary_expression(1);
    require_number(value);
    return value.value;
}

///
/// @brief               Gives a number a name, to be used in the expressions that follow. It is a
///                      constant of the file (of the scope, if there is one), not a symbol of the
///                      object file. Can be anywhere, also in a macro.
/// USAGE:               .equ <name>, <expression>
///
void Assembler::_equ()
{
    m_cursor.next();

    const Token &name = expect(TokenType::SYMBOL, "expected a name after .equ");
    expect(TokenType::COMMA, "expected ',' and the value of the constant");
    const sdword value = parse_signed_expression();

    const std::string key = scoped_name(name.str());
    const auto label = m_obj.string_table.find(key);
    check(m_constants.count(key) == 0, "'" + name.str() + "' is already a constant");
    check(label == m_obj.string_table.end()
              || m_obj.symbol_table.at(label->second).section == U32(-1),
          "'" + name.str() + "' is already a label");
    m_constants[key] = value;
}

///
/// @brief              Parses and evaluates an expression, see the declaration.
/// @return             Value of expression.
///
dword Assembler::parse_expression(dword min, dword max)
{
    AEMU_DEBUG("Assembler::parse_expression() - Parsing expression.");

    const dword exp_value = dword(parse_signed_expression());

    if (exp_value < min || exp_value > max)
    {
        warn(m_statement != nullptr ? *m_statement : m_cursor.peek(),
             "the value " + std::to_string(exp_value) + " is outside of the target range "
                 + std::to_string(min) + " - " + std::to_string(max));
    }
    else
    {
        AEMU_DEBUG("Assembler::parse_expression() - Parsed value {}.", exp_value);
    }

    return exp_value;
}

///
/// @brief               Declares a symbol to be global outside this compilation unit. It does not
///                      depend on the section, so it can be anywhere (a macro can declare one).
/// USAGE:               .global <symbol>
///
void Assembler::_global()
{
    m_cursor.next();

    const std::string symbol = expect(TokenType::SYMBOL, "expected a symbol after .global").str();
    m_obj.add_symbol(symbol, 0, ObjectFile::SymbolTableEntry::BindingInfo::GLOBAL);
}

///
/// @brief                Declares a symbol to exist in another compilation unit but not defined here.
///                         Symbol's binding info will be marked as weak. Can be anywhere, like
///                         .global.
/// USAGE:                .extern <symbol>
///
void Assembler::_extern()
{
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
    m_scope_sites.push_back(m_statement);
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
    m_scope_sites.pop_back();
}

///
/// @brief                  Moves where the assembler is in a section forward by a certain amount of bytes.
/// USAGE:                  .advance <expression>
///
void Assembler::_advance()
{
    m_cursor.next();

    const word val = parse_expression();

    // Safety exit. Likely unintentional behavior, and it is no use to go on with a section of a
    // different size than the program means.
    check(val < 0xffffff,
          ".advance is large and likely unintentional (" + std::to_string(val) + ")");

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
    check(val < 0xffff, ".align is large and likely unintentional (" + std::to_string(val) + ")");
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

    // The alignment is of the offset in this section. The section is put at an address, and joined
    // with the same section of other files, in a way that keeps it.
    ObjectFile::SectionHeader &header = m_obj.sections[m_cur_section_index];
    header.alignment = std::max(header.alignment, val);
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
    m_stopped = true;
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

    // `.word symbol` is the address of the symbol, which is only known when the program is linked.
    // Zeros are there until then, and a relocation says where the address goes.
    const auto define_address = [&]
    {
        check(n_bytes == sizeof(word),
              std::string(directive) + " cannot hold the address of a symbol, only .word can");

        const std::string symbol = m_cursor.next().str();
        m_obj.add_symbol(symbol, 0, ObjectFile::SymbolTableEntry::BindingInfo::WEAK);
        m_obj.rel_data.push_back({.offset = word(m_obj.data_section.size()),
                                  .symbol = m_obj.string_table[symbol],
                                  .type = ObjectFile::RelocationEntry::Type::R_EMU32_ABS32,
                                  .shift = 0,
                                  .token = m_cursor.position()});
        m_obj.data_section.insert(m_obj.data_section.end(), sizeof(word), 0);
    };

    const auto define_value = [&]
    {
        const dword value = parse_expression();

        // An unsigned number of the size, or a negative number (0 - 1) that fits as signed.
        const S64 signed_value = S64(value);
        const dword limit = n_bytes < sizeof(dword) ? dword(1) << (8 * n_bytes) : 0;
        check(limit == 0 || value < limit || (signed_value < 0 && signed_value >= -S64(limit / 2)),
              std::string(directive) + " value "
                  + std::to_string(signed_value < 0 ? signed_value : S64(value))
                  + " does not fit in " + std::to_string(n_bytes) + " byte(s)");

        const std::vector<byte> data = convert_little_endian({value}, n_bytes);
        m_obj.data_section.insert(m_obj.data_section.end(), data.begin(), data.end());
    };

    // The directive may have no arguments at all.
    if (m_cursor.at_end() || m_cursor.check(TokenType::NEWLINE))
    {
        return;
    }
    do
    {
        // A symbol on its own is an address, unless it names a constant. In an expression it is a
        // constant or the offset of a label (`end - start`).
        const bool lone_symbol =
            m_cursor.check(TokenType::SYMBOL)
            && (m_cursor.peek(1).is_one_of(
                {TokenType::COMMA, TokenType::NEWLINE, TokenType::END_OF_FILE}))
            && !is_constant(m_cursor.peek().str());
        if (lone_symbol)
        {
            define_address();
        }
        else
        {
            define_value();
        }
    } while (m_cursor.accept(TokenType::COMMA));
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
