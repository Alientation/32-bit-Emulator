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
    if (value.label == nullptr)
    {
        return;
    }
    if (value.section == U32(-1))
    {
        fail(*value.label, "'" + value.label->str()
                               + "' is not defined before this point; an expression can use a "
                                 "constant (.equ) or a label that comes earlier in the file");
    }
    fail(*value.label,
         "the address of '" + value.label->str()
             + "' is not known until linking, so it is not a number here; only the difference of "
               "two labels of the same section is a number");
}

///
/// @brief              Looks `symbol` up like a label is: the scopes that are open from the
///                     innermost out, then the file. A constant is a number. A label that is
///                     defined is its offset in its section. Anything else is a symbol that is
///                     not defined yet (it can be later in the file, or in another file), which has
///                     no value but can be the target of a relocation.
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
                        .section = label.section,
                        .base = sdword(label.symbol_value)};
            }
        }
    }
    return {.label = &symbol};
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
            // The address of a label is not known until linking. What can be done without it:
            // adding a number to it or taking one off (the sum is a relocation with an addend),
            // and the difference of two labels of a section, which does not depend on where the
            // section ends up.
            const bool is_add = op.type == TokenType::OPERATOR_ADDITION;
            const bool is_sub = op.type == TokenType::OPERATOR_SUBTRACTION;
            if ((is_add || is_sub) && left.label != nullptr && right.label == nullptr)
            {
                left.value = sdword(is_add ? U64(left.value) + U64(right.value)
                                           : U64(left.value) - U64(right.value));
                continue;
            }
            if (is_add && left.label == nullptr && right.label != nullptr)
            {
                ExprValue sum = right;
                sum.value = sdword(U64(left.value) + U64(right.value));
                left = sum;
                continue;
            }
            if (is_sub && left.label != nullptr && right.label != nullptr)
            {
                // A symbol that is not defined yet has no place in its section.
                if (left.section == U32(-1))
                {
                    require_number(left);
                }
                if (right.section == U32(-1))
                {
                    require_number(right);
                }
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

Assembler::ExprValue Assembler::parse_symbol_operand(const std::string &expected)
{
    if (!at_expression())
    {
        fail(m_cursor.peek(), expected + ", got " + basm::describe(m_cursor.peek()));
    }
    const Token &first = m_cursor.peek();
    const ExprValue target = parse_binary_expression(1);
    if (target.label == nullptr)
    {
        fail(first, expected + ", got a number");
    }
    return target;
}

void Assembler::add_relocation(std::vector<ObjectFile::RelocationEntry> &relocations, word offset,
                               ObjectFile::RelocationEntry::Type type, const ExprValue &target)
{
    const S64 addend = S64(target.value) - S64(target.base);
    check(addend >= INT32_MIN && addend <= INT32_MAX,
          "the number added to '" + target.label->str() + "' does not fit in 32 bits");

    // The name stays what was written. A symbol of a scope is resolved to the one of that scope
    // when the file is done (fill_local), by the position of the relocation in the tokens.
    const std::string name = target.label->str();
    m_obj.add_symbol(name, 0, ObjectFile::SymbolTableEntry::BindingInfo::WEAK);
    relocations.push_back({.offset = offset,
                           .symbol = m_obj.string_table[name],
                           .type = type,
                           .addend = sword(addend),
                           .token = m_cursor.position()});
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
/// @brief                Declares a symbol weak. If this file defines it, another file may define it
///                       too and that definition is used instead (a file that defines it without
///                       `.weak` wins, and of several weak ones the first linked does). If nothing
///                       defines it, its value is 0 and it is not an error. Can be anywhere, like
///                       .global.
/// USAGE:                .weak <symbol>
///
void Assembler::_weak()
{
    m_cursor.next();

    const std::string symbol = expect(TokenType::SYMBOL, "expected a symbol after .weak").str();
    m_obj.add_symbol(symbol, 0, ObjectFile::SymbolTableEntry::BindingInfo::WEAK_DECLARED);
}

///
/// @brief                Reserves `size` zeroed bytes in .bss for a global symbol that other files
///                       may reserve too. It is a weak definition in .bss, so a file that defines the
///                       symbol for real wins, and when several files only reserve it they share the
///                       space of the first one linked. Every file should give the same size, the
///                       linker does not know it. Does not change the current section.
/// USAGE:                .comm <symbol>, <size>{, <alignment>}
///
void Assembler::_comm()
{
    m_cursor.next();

    const Token &name = expect(TokenType::SYMBOL, "expected a symbol after .comm");
    expect(TokenType::COMMA, "expected ',' and the size after the symbol of .comm");
    const word size = parse_expression();
    word alignment = 1;
    if (m_cursor.accept(TokenType::COMMA))
    {
        alignment = parse_expression();
        check(alignment != 0 && alignment < 0xffff, ".comm expects an alignment from 1 to 65535");
    }
    check(size < 0xffffff,
          ".comm size is large and likely unintentional (" + std::to_string(size) + ")");

    const U32 bss = m_obj.section_table[".bss"];
    const auto existing = m_obj.string_table.find(name.str());
    check(existing == m_obj.string_table.end()
              || m_obj.symbol_table.at(existing->second).section == U32(-1),
          "'" + name.str() + "' is already defined");

    m_obj.bss_section += (alignment - (m_obj.bss_section % alignment)) % alignment;
    m_obj.sections[bss].alignment = std::max(m_obj.sections[bss].alignment, alignment);
    m_obj.add_symbol(name.str(), m_obj.bss_section,
                     ObjectFile::SymbolTableEntry::BindingInfo::WEAK_DECLARED, bss);
    m_obj.bss_section += size;
}

const ObjectFile::ByteSection *Assembler::current_byte_section() const
{
    for (const ObjectFile::ByteSection &section : ObjectFile::byte_sections())
    {
        if (m_cur_section_index != U32(-1) && m_cur_section_index < m_obj.sections.size()
            && m_obj.sections[m_cur_section_index].type == section.type)
        {
            return &section;
        }
    }
    return nullptr;
}

ObjectFile::UserSection *Assembler::current_user_section()
{
    return m_cur_section == Section::USER || m_cur_section == Section::USER_BSS
               ? m_obj.user_section_at(m_cur_section_index)
               : nullptr;
}

word &Assembler::zero_size()
{
    return m_cur_section == Section::BSS ? m_obj.bss_section
                                         : current_user_section()->zero_size;
}

bool Assembler::in_byte_section() const
{
    return current_byte_section() != nullptr || m_cur_section == Section::USER;
}

bool Assembler::in_code_section() const
{
    return m_cur_section == Section::TEXT
           || (m_cur_section == Section::USER
               && m_obj.sections[m_cur_section_index].type
                      == ObjectFile::SectionHeader::Type::USER_RX);
}

std::vector<byte> &Assembler::section_bytes()
{
    if (ObjectFile::UserSection *user = current_user_section()) return user->bytes;
    return m_obj.*current_byte_section()->bytes;
}

std::vector<ObjectFile::RelocationEntry> &Assembler::section_relocations()
{
    if (ObjectFile::UserSection *user = current_user_section()) return user->relocations;
    return m_obj.*current_byte_section()->relocations;
}

void Assembler::emit_instruction(const word instruction)
{
    if (m_cur_section == Section::TEXT)
    {
        m_obj.text_section.push_back(instruction);
        return;
    }

    // Code in a user section is bytes there, like all of its contents.
    std::vector<byte> &bytes = section_bytes();
    check(bytes.size() % sizeof(word) == 0,
          "an instruction has to start at a multiple of 4 bytes in the section, it would be at "
              + std::to_string(bytes.size()) + " (.align 4 first)");
    for (size_t shift = 0; shift < 32; shift += 8)
    {
        bytes.push_back(byte(instruction >> shift));
    }
}

word Assembler::code_offset()
{
    return m_cur_section == Section::TEXT ? word(m_obj.text_section.size() * 4)
                                          : word(section_bytes().size());
}

std::vector<ObjectFile::RelocationEntry> &Assembler::code_relocations()
{
    return m_cur_section == Section::TEXT ? m_obj.rel_text : section_relocations();
}

void Assembler::enter_byte_section(Section section, const char *name)
{
    m_cur_section = section;
    m_cur_section_index = m_obj.section_table[name];
}

///
/// @brief                  Read only data. The program cannot write it.
/// USAGE:                  .rodata
///
void Assembler::_rodata()
{
    m_cursor.next();
    enter_byte_section(Section::RODATA, ".rodata");
}

///
/// @brief                  Words with the addresses of functions to call before `main` (`.word f`).
///                         The linker joins them and defines `__init_array_start` and
///                         `__init_array_end` around them. Read only.
/// USAGE:                  .init_array
///
void Assembler::_init_array()
{
    m_cursor.next();
    enter_byte_section(Section::INIT_ARRAY, ".init_array");
}

///
/// @brief                  Same as .init_array, for the functions to call after `main`
///                         (`__fini_array_start`, `__fini_array_end`).
/// USAGE:                  .fini_array
///
void Assembler::_fini_array()
{
    m_cursor.next();
    enter_byte_section(Section::FINI_ARRAY, ".fini_array");
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
    case Section::USER_BSS:
        check(val >= zero_size(), ".org cannot move the assembler backwards, expected >= "
                                      + std::to_string(zero_size()) + ", got "
                                      + std::to_string(val));
        zero_size() = val;
        break;
    case Section::DATA:
    case Section::RODATA:
    case Section::INIT_ARRAY:
    case Section::FINI_ARRAY:
    case Section::USER:
        check(val >= section_bytes().size(),
              ".org cannot move the assembler backwards, expected >= "
                  + std::to_string(section_bytes().size()) + ", got " + std::to_string(val));
        section_bytes().resize(val, 0);
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
    case Section::USER_BSS:
        zero_size() += val;
        break;
    case Section::DATA:
    case Section::RODATA:
    case Section::INIT_ARRAY:
    case Section::FINI_ARRAY:
    case Section::USER:
        section_bytes().insert(section_bytes().end(), val, 0);
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
    case Section::USER_BSS:
        zero_size() += (val - (zero_size() % val)) % val;
        break;
    case Section::DATA:
    case Section::RODATA:
    case Section::INIT_ARRAY:
    case Section::FINI_ARRAY:
    case Section::USER:
        while (section_bytes().size() % val != 0)
        {
            section_bytes().push_back(0);
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
/// @brief                  Makes a section with a name of the program's choosing the current one,
///                         and creates it the first time. The flags are a string of the letters
///                         r (read), w (write) and x (execute): "r" (read only), "rw" or "rx". A
///                         section is not writable and executable at once. A section made without
///                         flags is "rw". The linker joins the sections of the same name of all
///                         the files, and the linker script places them by name.
///                         Instructions can be assembled in an executable section, data
///                         directives in any of them (a user section holds bytes).
///                         A third operand, "nobits", makes the section zero filled like .bss (flags
///                         "rw"): the file has its size and no bytes, and only .advance, .align,
///                         .org and labels are legal in it.
///                         The names of the sections the assembler has (".text", ".data", ".bss",
///                         ".rodata", ".init_array", ".fini_array") are those sections, so
///                         `.section ".data"` is `.data`.
/// USAGE:                  .section <string>[, <flags>[, "nobits"]]
///
void Assembler::_section()
{
    m_cursor.next();

    const Token &name_token =
        expect(TokenType::LITERAL_STRING, ".section expects the name of the section as a string");
    const std::string name = basm::unescape_string_literal(name_token);

    const Token *flags_token = nullptr;
    const Token *type_token = nullptr;
    bool writable = false;
    bool executable = false;
    bool nobits = false;
    if (m_cursor.accept(TokenType::COMMA))
    {
        flags_token = &expect(TokenType::LITERAL_STRING,
                              "expected the flags of the section as a string (\"r\", \"rw\" or "
                              "\"rx\")");
        const std::string flags = basm::unescape_string_literal(*flags_token);
        bool readable = false;
        for (const char flag : flags)
        {
            if (flag == 'r') readable = true;
            else if (flag == 'w') writable = true;
            else if (flag == 'x') executable = true;
            else
            {
                fail(*flags_token, std::string("unknown flag '") + flag
                                       + "', the flags are r (read), w (write) and x (execute)");
            }
        }
        if (!readable && !writable && !executable)
        {
            fail(*flags_token, "the flags of a section are \"r\", \"rw\" or \"rx\", got none");
        }
        if (writable && executable)
        {
            fail(*flags_token, "a section cannot be both writable and executable");
        }

        if (m_cursor.accept(TokenType::COMMA))
        {
            type_token = &expect(TokenType::LITERAL_STRING,
                                 "expected the type of the section as a string (\"nobits\")");
            const std::string type = basm::unescape_string_literal(*type_token);
            if (type != "nobits")
            {
                fail(*type_token, "unknown section type \"" + type + "\", the only one is \"nobits\"");
            }
            if (!writable || executable)
            {
                fail(*type_token, "a nobits section is zero filled and writable, its flags are "
                                  "\"rw\"");
            }
            nobits = true;
        }
    }

    // The sections that the assembler has: the name is that section, and the flags can only say
    // what it is.
    struct Builtin
    {
        const char *name;
        Section section;
        bool writable;
        bool executable;
    };
    static constexpr Builtin kBuiltin[] = {
        {".text", Section::TEXT, false, true},
        {".data", Section::DATA, true, false},
        {".bss", Section::BSS, true, false},
        {".rodata", Section::RODATA, false, false},
        {".init_array", Section::INIT_ARRAY, false, false},
        {".fini_array", Section::FINI_ARRAY, false, false},
    };
    for (const Builtin &builtin : kBuiltin)
    {
        if (name != builtin.name) continue;

        if (nobits && builtin.section != Section::BSS)
        {
            fail(*type_token, name + " cannot be nobits, only .bss is zero filled");
        }
        if (flags_token != nullptr
            && (writable != builtin.writable || executable != builtin.executable))
        {
            fail(*flags_token, "the flags of " + name + " are "
                                   + (builtin.executable ? "\"rx\""
                                      : builtin.writable ? "\"rw\""
                                                         : "\"r\"")
                                   + ", they cannot be changed");
        }
        m_cur_section = builtin.section;
        m_cur_section_index = m_obj.section_table[name];
        return;
    }

    // The names of the other sections of the file are not the program's to use: .symtab,
    // .strtab, .rel.* and what the relocations of a section of the program are called.
    check_section_name(name_token, name);

    ObjectFile::UserSection *user = m_obj.find_user_section(name);
    if (user == nullptr)
    {
        if (flags_token == nullptr) writable = true; // "rw"
        m_cur_section_index = m_obj.add_user_section(name, writable, executable, nobits);
        user = m_obj.find_user_section(name);
    }
    else if (flags_token != nullptr
             && (user->writable != writable || user->executable != executable
                 || user->nobits != nobits))
    {
        fail(*flags_token, "the section " + name + " was made "
                               + (user->executable ? "\"rx\""
                                  : user->nobits   ? "\"rw\", \"nobits\""
                                  : user->writable ? "\"rw\""
                                                   : "\"r\"")
                               + ", the flags cannot be changed");
    }
    else
    {
        m_cur_section_index = user->header_index;
    }
    m_cur_section = user->nobits ? Section::USER_BSS : Section::USER;
}

///
/// @brief                  Remembers the current section and switches to another one like
///                         .section does (the same operands), so that .popsection can go back.
///                         Meant for macros, which put something in .rodata or a section of their
///                         own and carry on in the section they were used in. They nest.
/// USAGE:                  .pushsection <string>[, <flags>[, "nobits"]]
///
void Assembler::_pushsection()
{
    m_saved_sections.push_back({m_cur_section, m_cur_section_index, m_statement});
    _section(); // starts by skipping the directive, which is this one
}

///
/// @brief                  Goes back to the section that the last .pushsection left.
/// USAGE:                  .popsection
///
void Assembler::_popsection()
{
    check(!m_saved_sections.empty(), ".popsection must have a matching .pushsection");

    m_cursor.next();
    m_cur_section = m_saved_sections.back().section;
    m_cur_section_index = m_saved_sections.back().index;
    m_saved_sections.pop_back();
}

void Assembler::check_section_name(const Token &at, const std::string &name)
{
    if (name.empty())
    {
        fail(at, "the name of a section cannot be empty");
    }
    for (const char c : name)
    {
        if (c <= ' ' || c == 0x7F)
        {
            fail(at, "the name of a section cannot have spaces or control characters");
        }
    }
    if (name.starts_with(".rel") || name == ".symtab" || name == ".strtab"
        || (m_obj.section_table.count(name) != 0 && m_obj.find_user_section(name) == nullptr))
    {
        fail(at, "'" + name + "' is a name that the object file uses for itself, use another");
    }
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

    enter_byte_section(Section::DATA, ".data");
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
    check(in_byte_section(),
          std::string(directive)
              + " can only define data in the .data section or another "
                "data section (.rodata, .init_array, .fini_array)");

    m_cursor.next();

    // The directive may have no arguments at all.
    if (m_cursor.at_end() || m_cursor.check(TokenType::NEWLINE))
    {
        return;
    }
    do
    {
        const Token &first = m_cursor.peek();
        append_value(directive, n_bytes, parse_binary_expression(1), first);
    } while (m_cursor.accept(TokenType::COMMA));
}

void Assembler::append_value(const char *directive, U8 n_bytes, const ExprValue &result,
                             const Token &first)
{
    if (result.label != nullptr)
    {
        // `.word symbol + 4` is the address of the symbol plus a number, which is only known
        // when the program is linked. Zeros are there until then, and a relocation says where
        // the address goes.
        if (n_bytes != sizeof(word))
        {
            fail(first, std::string(directive)
                            + " cannot hold the address of a symbol, only .word can");
        }
        add_relocation(section_relocations(), word(section_bytes().size()),
                       ObjectFile::RelocationEntry::Type::R_EMU32_ABS32, result);
        section_bytes().insert(section_bytes().end(), sizeof(word), 0);
        return;
    }

    const dword value = dword(result.value);

    // An unsigned number of the size, or a negative number (0 - 1) that fits as signed.
    const S64 signed_value = S64(value);
    const dword limit = n_bytes < sizeof(dword) ? dword(1) << (8 * n_bytes) : 0;
    check(limit == 0 || value < limit || (signed_value < 0 && signed_value >= -S64(limit / 2)),
          std::string(directive) + " value "
              + std::to_string(signed_value < 0 ? signed_value : S64(value)) + " does not fit in "
              + std::to_string(n_bytes) + " byte(s)");

    const std::vector<byte> data = convert_little_endian({value}, n_bytes);
    section_bytes().insert(section_bytes().end(), data.begin(), data.end());
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

///
/// @brief                  Repeats a value: `count` copies of a `size` byte value (little endian).
///                         The size is 1, 2, 4 or 8 and defaults to 1, the value defaults to 0 and
///                         is a number that fits (for a size of 4 it can also be the address of a
///                         symbol, like .word). For zeros, .advance does the same.
/// USAGE:                  .fill <count>{, <size>{, <value>}}
///
void Assembler::_fill()
{
    check(in_byte_section(),
          ".fill can only define data in the .data section or another data section (.rodata, "
          ".init_array, .fini_array)");
    m_cursor.next();

    const Token &count_token = m_cursor.peek();
    const word count = parse_expression();
    word size = 1;
    if (m_cursor.accept(TokenType::COMMA))
    {
        const Token &size_token = m_cursor.peek();
        size = parse_expression();
        if (size != 1 && size != 2 && size != 4 && size != 8)
        {
            fail(size_token, ".fill expects a size of 1, 2, 4 or 8 bytes, got "
                                 + std::to_string(size));
        }
    }
    if (static_cast<dword>(count) * size >= 0xffffff)
    {
        fail(count_token,
             ".fill is large and likely unintentional (" + std::to_string(count) + " of "
                 + std::to_string(size) + " bytes)");
    }

    ExprValue value;
    const Token *value_token = &count_token;
    if (m_cursor.accept(TokenType::COMMA))
    {
        value_token = &m_cursor.peek();
        value = parse_binary_expression(1);
    }
    for (word i = 0; i < count; i++)
    {
        append_value(".fill", U8(size), value, *value_token);
    }
}

void Assembler::_ascii()
{
    check(in_byte_section(), ".ascii can only define data in the .data section or "
                                             "another data section (.rodata, ...)");
    m_cursor.next();

    // Note, does not automatically add the null terminator.
    do
    {
        const std::string str = basm::unescape_string_literal(
            expect(TokenType::LITERAL_STRING, "expected a string literal"));
        for (const char c : str)
        {
            section_bytes().push_back(static_cast<byte>(c));
        }
    } while (m_cursor.accept(TokenType::COMMA));
}

void Assembler::_asciz()
{
    check(in_byte_section(), ".asciz can only define data in the .data section or "
                                             "another data section (.rodata, ...)");
    m_cursor.next();

    do
    {
        const std::string str = basm::unescape_string_literal(
            expect(TokenType::LITERAL_STRING, "expected a string literal"));
        for (const char c : str)
        {
            section_bytes().push_back(static_cast<byte>(c));
        }
        section_bytes().push_back('\0');
    } while (m_cursor.accept(TokenType::COMMA));
}
