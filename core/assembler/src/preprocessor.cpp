#include "assembler/preprocessor.h"
#include "util/logger.h"

#include <algorithm>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <optional>
#include <span>

using basm::Token;
using basm::TokenCursor;
using basm::TokenFlag;
using basm::TokenType;

namespace
{

/// Token to point an error at when the line ended before something expected.
const Token &where(const TokenCursor &line)
{
    if (line.at_end() && !line.tokens().empty()) return line.tokens().back();
    return line.peek();
}

std::string describe_next(const TokenCursor &line)
{
    return line.at_end() ? "end of line" : basm::describe(line.peek());
}

bool is_value_conditional(TokenType type)
{
    switch (type)
    {
    case TokenType::PREPROCESSOR_IFEQU:
    case TokenType::PREPROCESSOR_IFNEQU:
    case TokenType::PREPROCESSOR_IFLESS:
    case TokenType::PREPROCESSOR_IFMORE:
    case TokenType::PREPROCESSOR_ELSEEQU:
    case TokenType::PREPROCESSOR_ELSENEQU:
    case TokenType::PREPROCESSOR_ELSELESS:
    case TokenType::PREPROCESSOR_ELSEMORE:
        return true;
    default:
        return false;
    }
}

bool is_if(TokenType type)
{
    switch (type)
    {
    case TokenType::PREPROCESSOR_IFDEF:
    case TokenType::PREPROCESSOR_IFNDEF:
    case TokenType::PREPROCESSOR_IFEQU:
    case TokenType::PREPROCESSOR_IFNEQU:
    case TokenType::PREPROCESSOR_IFLESS:
    case TokenType::PREPROCESSOR_IFMORE:
        return true;
    default:
        return false;
    }
}

bool is_else(TokenType type)
{
    switch (type)
    {
    case TokenType::PREPROCESSOR_ELSE:
    case TokenType::PREPROCESSOR_ELSEDEF:
    case TokenType::PREPROCESSOR_ELSENDEF:
    case TokenType::PREPROCESSOR_ELSEEQU:
    case TokenType::PREPROCESSOR_ELSENEQU:
    case TokenType::PREPROCESSOR_ELSELESS:
    case TokenType::PREPROCESSOR_ELSEMORE:
        return true;
    default:
        return false;
    }
}

bool is_conditional(TokenType type)
{
    return is_if(type) || is_else(type) || type == TokenType::PREPROCESSOR_ENDIF;
}

/// The text of the tokens, with one space wherever the source had whitespace.
std::string join(const std::vector<Token> &tokens)
{
    std::string text;
    for (std::size_t i = 0; i < tokens.size(); i++)
    {
        if (i > 0 && tokens[i].has(TokenFlag::SPACE_BEFORE)) text += ' ';
        text += tokens[i].text;
    }
    return text;
}

/// The value of text that is a whole number written the way the assembler writes one: 42, 0x2A,
/// 0b101 or 0o17, with an optional '-'.
std::optional<long long> as_number(const std::string &text)
{
    std::size_t i = 0;
    const bool negative = !text.empty() && text[0] == '-';
    if (negative) i++;

    int base = 10;
    if (i + 1 < text.size() && text[i] == '0')
    {
        const char prefix = static_cast<char>(text[i + 1] | 0x20); // lower case
        if (prefix == 'x') base = 16;
        else if (prefix == 'b') base = 2;
        else if (prefix == 'o') base = 8;
    }
    if (base != 10) i += 2;
    if (i >= text.size()) return std::nullopt;

    long long value = 0;
    const auto [end, error] =
        std::from_chars(text.data() + i, text.data() + text.size(), value, base);
    if (error != std::errc() || end != text.data() + text.size()) return std::nullopt;
    return negative ? -value : value;
}

/// Compares the values of an #ifequ, #ifless, ... They are compared as numbers when both are
/// numbers (9 is less than 10) and as text otherwise.
int compare_values(const std::string &left, const std::string &right)
{
    const std::optional<long long> a = as_number(left);
    const std::optional<long long> b = as_number(right);
    if (a && b) return *a < *b ? -1 : (*a > *b ? 1 : 0);
    return left.compare(right) < 0 ? -1 : (left == right ? 0 : 1);
}

constexpr bool is_ident_char(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

constexpr bool is_operator_char(char c)
{
    return c == '<' || c == '>' || c == '=' || c == '!' || c == '&' || c == '|';
}

/// Whether printing two tokens next to each other would lex differently than they did.
bool would_merge(char last, char first)
{
    return (is_ident_char(last) && is_ident_char(first))
           || (is_operator_char(last) && is_operator_char(first));
}

} // namespace

Preprocessor::Preprocessor(const File &input_file, const std::string &output_file_path,
                           PreprocessorOptions options) :
    m_options(std::move(options)),
    m_input_file(input_file),
    m_sources(std::make_shared<basm::SourceManager>())
{
    // default output file path if not supplied in the constructor. The file is only created if it
    // is going to be written, there is nothing to leave behind (or to fail on a directory that
    // cannot be written to) otherwise.
    const bool create = m_options.write_output;
    if (output_file_path.empty())
    {
        m_output_file =
            File(m_input_file.get_name(), PROCESSED_EXTENSION, m_input_file.get_dir_str(), create);
    }
    else
    {
        m_output_file = File(output_file_path, create);
    }

    AEMU_CHECK(input_file.get_extension() == SOURCE_EXTENSION
                   || input_file.get_extension() == INCLUDE_EXTENSION,
               "Preprocessor::Preprocessor() - Invalid source file: '{}'.",
               input_file.get_extension());

    m_state = State::UNPROCESSED;
}

Preprocessor::~Preprocessor()
{
}

Preprocessor::State Preprocessor::get_state()
{
    return m_state;
}

void Preprocessor::fail(const Token &at, const std::string &message)
{
    m_state = State::PROCESSED_ERROR;
    basm::fatal_at(*m_sources, at.loc, message);
}

File Preprocessor::preprocess()
{
    AEMU_DEBUG("Preprocessor::preprocess() - Preprocessing file: {}", m_input_file.get_name());

    AEMU_CHECK(m_state == State::UNPROCESSED,
               "Preprocessor::preprocess() - Preprocessor is not in the UNPROCESSED state");
    m_state = State::PROCESSING;

    define_from_options();
    push_file(m_input_file.get_path(), nullptr);
    run();

    if (!m_out.empty() && m_out.back() != '\n') m_out += '\n';

    // The token list ends the way the lexer ends one, a NEWLINE then END_OF_FILE.
    const basm::SourceLocation end_loc =
        m_out_tokens.empty() ? basm::SourceLocation{} : m_out_tokens.back().loc;
    if (m_out_tokens.empty() || !m_out_tokens.back().is(TokenType::NEWLINE))
    {
        Token newline;
        newline.type = TokenType::NEWLINE;
        newline.flags = TokenFlag::SYNTHETIC;
        newline.loc = end_loc;
        m_out_tokens.push_back(newline);
    }
    Token eof;
    eof.type = TokenType::END_OF_FILE;
    eof.flags = TokenFlag::SYNTHETIC;
    eof.loc = end_loc;
    m_out_tokens.push_back(eof);

    if (m_options.write_output)
    {
        // Truncates the intermediate output file.
        std::ofstream out(m_output_file.get_path(),
                          std::ios::out | std::ios::trunc | std::ios::binary);
        AEMU_CHECK(out.good(), "Preprocessor::preprocess() - Cannot write to '{}'.",
                   m_output_file.get_path());
        out << m_out;
        out.close();
    }

    m_state = State::PROCESSED_SUCCESS;
    AEMU_DEBUG("Preprocessor::preprocess() - Preprocessed file: {}", m_input_file.get_name());
    return m_output_file;
}

basm::PreprocessedSource Preprocessor::take_result()
{
    AEMU_CHECK(m_state == State::PROCESSED_SUCCESS,
               "Preprocessor::take_result() - The file is not preprocessed");
    return {m_sources, std::move(m_out_tokens)};
}

bool Preprocessor::active() const
{
    return m_conds.empty() || m_conds.back().active;
}

bool Preprocessor::is_symbol_def(const std::string &name, std::size_t num_params) const
{
    const auto it = m_symbols.find(name);
    return it != m_symbols.end() && it->second.find(num_params) != it->second.end();
}

Preprocessor::Frame &Preprocessor::push_frame(Tokens tokens)
{
    auto frame = std::make_unique<Frame>();
    frame->tokens = std::move(tokens);
    frame->cursor = TokenCursor(std::span<const Token>(frame->tokens));
    frame->cond_base = m_conds.size();
    m_frames.push_back(std::move(frame));
    return *m_frames.back();
}

void Preprocessor::push_file(const std::string &path, const Token *include_site)
{
    if (include_site != nullptr && m_frames.size() >= kMaxFrames)
    {
        fail(*include_site, "#include nested too deeply, is a file including itself?");
    }

    const basm::SourceId id = m_sources->add_file(path);
    if (id == basm::kInvalidSource)
    {
        if (include_site != nullptr) fail(*include_site, "cannot open '" + path + "'");
        AEMU_FATAL("Preprocessor::preprocess() - Cannot open '{}'.", path);
    }

    basm::LexOptions options;
    options.collapse_newlines = true;
    basm::LexResult lexed = basm::lex(*m_sources, id, options);
    basm::fatal_if_errors(*m_sources, lexed);

    Frame &frame = push_frame(std::move(lexed.tokens));
    frame.is_file = true;
    frame.dir = File(path).get_dir_str();
}

void Preprocessor::run()
{
    while (!m_frames.empty())
    {
        Frame &frame = *m_frames.back();
        if (frame.cursor.at_end())
        {
            finish_frame(frame);
            m_frames.pop_back();
            continue;
        }

        const Token &token = frame.cursor.peek();
        if (basm::is_preprocessor_directive(token.type))
        {
            handle_directive(frame);
            continue;
        }

        // The tokens of a conditional block that is not taken are dropped.
        if (!active())
        {
            frame.cursor.next();
            continue;
        }

        if (token.is(TokenType::SYMBOL) && expand_symbol(frame, token))
        {
            continue;
        }

        emit(token);
        frame.cursor.next();
    }
}

void Preprocessor::finish_frame(Frame &frame)
{
    if (m_conds.size() > frame.cond_base)
    {
        const Cond &open = m_conds.back();
        basm::fatal_at(*m_sources, open.loc, "conditional block is never closed with #endif");
    }
}

void Preprocessor::emit(const Token &token)
{
    m_out_tokens.push_back(token);

    if (token.is(TokenType::NEWLINE))
    {
        m_out += '\n';
        m_last_char = '\n';
        return;
    }

    // Indentation is not kept, so nothing needs a separator at the start of a line.
    if (m_last_char != '\n'
        && (token.has(TokenFlag::SPACE_BEFORE)
            || (!token.text.empty() && would_merge(m_last_char, token.text.front()))))
    {
        m_out += ' ';
    }

    m_out += token.text;
    m_last_char = token.text.empty() ? m_last_char : token.text.back();

    // The lexer drops the colon of a label from its text.
    if (token.is(TokenType::LABEL))
    {
        m_out += ':';
        m_last_char = ':';
    }
}

bool Preprocessor::parse_call_args(TokenCursor &cursor, std::vector<Tokens> &args)
{
    cursor.next(); // '('

    Tokens current;
    bool separated = false;
    int depth = 0;
    while (true)
    {
        const Token &token = cursor.peek();
        if (token.is_one_of({TokenType::NEWLINE, TokenType::END_OF_FILE}))
        {
            return false;
        }

        if (depth == 0 && token.is(TokenType::CLOSE_PARENTHESIS))
        {
            cursor.next();
            if (separated || !current.empty())
            {
                args.push_back(std::move(current));
            }
            return true;
        }

        if (depth == 0 && token.is(TokenType::COMMA))
        {
            args.push_back(std::move(current));
            current.clear();
            separated = true;
            cursor.next();
            continue;
        }

        if (token.is_one_of(
                {TokenType::OPEN_PARENTHESIS, TokenType::OPEN_BRACKET, TokenType::OPEN_BRACE}))
        {
            depth++;
        }
        else if (token.is_one_of({TokenType::CLOSE_PARENTHESIS, TokenType::CLOSE_BRACKET,
                                  TokenType::CLOSE_BRACE}))
        {
            depth--;
        }
        current.push_back(token);
        cursor.next();
    }
}

std::vector<std::string> Preprocessor::parse_params(TokenCursor &line)
{
    std::vector<std::string> params;
    line.next(); // '('
    if (line.accept(TokenType::CLOSE_PARENTHESIS))
    {
        return params;
    }

    while (true)
    {
        const Token &param = line.peek();
        if (!param.is(TokenType::SYMBOL))
        {
            fail(where(line), "expected a parameter name, got " + describe_next(line));
        }
        for (const std::string &other : params)
        {
            if (other == param.text)
            {
                fail(param, "duplicate parameter '" + other + "'");
            }
        }
        params.push_back(param.str());
        line.next();

        if (line.accept(TokenType::COMMA))
        {
            continue;
        }
        if (line.accept(TokenType::CLOSE_PARENTHESIS))
        {
            return params;
        }
        fail(where(line), "expected ',' or ')' in the parameter list, got " + describe_next(line));
    }
}

Preprocessor::Tokens Preprocessor::substitute(const Tokens &body,
                                              const std::vector<std::string> &params,
                                              const std::vector<Tokens> &args, U32 expansion) const
{
    Tokens result;
    result.reserve(body.size());
    for (const Token &token : body)
    {
        std::size_t index = params.size();
        if (token.is(TokenType::SYMBOL))
        {
            for (std::size_t i = 0; i < params.size(); i++)
            {
                if (params[i] == token.text)
                {
                    index = i;
                    break;
                }
            }
        }

        if (index == params.size())
        {
            result.push_back(token);
            result.back().loc.expansion = expansion;
            continue;
        }

        // The argument takes the place, and the spacing, of the parameter.
        for (std::size_t i = 0; i < args[index].size(); i++)
        {
            Token copy = args[index][i];
            if (i == 0)
            {
                copy.flags = static_cast<std::uint8_t>((copy.flags & ~TokenFlag::SPACE_BEFORE)
                                                       | (token.flags & TokenFlag::SPACE_BEFORE));
            }
            result.push_back(copy);
        }
    }

    return result;
}

bool Preprocessor::expand_symbol(Frame &frame, const Token &token)
{
    const auto symbol = m_symbols.find(token.str());
    if (symbol == m_symbols.end())
    {
        return false;
    }
    const std::map<std::size_t, Symbol> &definitions = symbol->second;

    // Copy, the cursor is about to move.
    const Token site = token;
    const std::size_t mark = frame.cursor.mark();
    frame.cursor.next();

    const Symbol *definition = nullptr;
    std::vector<Tokens> args;

    // Arguments must start right after the name, `F (x)` is `F` followed by `(x)`.
    if (frame.cursor.check(TokenType::OPEN_PARENTHESIS)
        && !frame.cursor.peek().has(TokenFlag::SPACE_BEFORE))
    {
        const std::size_t after_name = frame.cursor.mark();
        if (parse_call_args(frame.cursor, args))
        {
            const auto it = definitions.find(args.size());
            if (it != definitions.end())
            {
                definition = &it->second;
            }
            else if (definitions.find(0) == definitions.end())
            {
                fail(site, "'" + site.str() + "' is not defined with " + std::to_string(args.size())
                               + " argument(s)");
            }
        }
        else if (definitions.find(0) == definitions.end())
        {
            fail(site, "missing ')' after the arguments of '" + site.str() + "'");
        }

        if (definition == nullptr)
        {
            // A symbol without parameters followed by a parenthesis.
            frame.cursor.rewind(after_name);
            args.clear();
        }
    }

    if (definition == nullptr)
    {
        const auto it = definitions.find(0);
        if (it == definitions.end())
        {
            // Only defined with parameters but used without any. Leave it as it is.
            frame.cursor.rewind(mark);
            return false;
        }
        definition = &it->second;
    }

    if (m_frames.size() >= kMaxFrames)
    {
        fail(site, "'" + site.str() + "' expands too deeply, does it refer to itself?");
    }

    const U32 expansion_id = m_sources->add_expansion(site.str(), false, site.loc);
    Tokens expansion = substitute(definition->value, definition->params, args, expansion_id);
    if (!expansion.empty())
    {
        expansion.front().flags =
            static_cast<std::uint8_t>((expansion.front().flags & ~TokenFlag::SPACE_BEFORE)
                                      | (site.flags & TokenFlag::SPACE_BEFORE));
        push_frame(std::move(expansion));
    }
    return true;
}

void Preprocessor::handle_directive(Frame &frame)
{
    const TokenType type = frame.cursor.peek().type;
    if (!is_conditional(type) && !active())
    {
        frame.cursor.skip_line();
        return;
    }

    const std::span<const Token> tokens = frame.cursor.take_line();
    TokenCursor line(tokens);

    switch (type)
    {
    case TokenType::PREPROCESSOR_INCLUDE:
        _include(line);
        break;
    case TokenType::PREPROCESSOR_MACRO:
        _macro(frame, line);
        break;
    case TokenType::PREPROCESSOR_MACRET:
        _macret(frame, line);
        break;
    case TokenType::PREPROCESSOR_MACEND:
        fail(line.peek(), "#macend without a matching #macro");
    case TokenType::PREPROCESSOR_INVOKE:
        _invoke(line);
        break;
    case TokenType::PREPROCESSOR_DEFINE:
        _define(line);
        break;
    case TokenType::PREPROCESSOR_UNDEF:
        _undef(line);
        break;
    default:
        _conditional(frame, line);
        break;
    }
}

void Preprocessor::_include(TokenCursor &line)
{
    const Token directive = line.next();

    bool system = false;
    if (line.accept(TokenType::OPERATOR_LOGICAL_LESS_THAN))
    {
        system = true;
    }

    if (!line.check(TokenType::LITERAL_STRING))
    {
        fail(where(line), "#include expects \"file\" or <\"file\">, got " + describe_next(line));
    }
    const std::string name = basm::unescape_string_literal(line.next());

    if (system && !line.accept(TokenType::OPERATOR_LOGICAL_GREATER_THAN))
    {
        fail(where(line), "expected '>' after the file name, got " + describe_next(line));
    }
    if (!line.at_end())
    {
        fail(line.peek(), "unexpected " + describe_next(line) + " after #include");
    }

    std::string path;
    if (system)
    {
        // Files in the include directories (-I).
        bool found = false;
        // subfile_exists is not const, so the directories are copied.
        for (Directory dir : m_options.system_dirs)
        {
            if (!dir.subfile_exists(name))
            {
                continue;
            }

            // The same directory listed twice is the same file, it is not a second one.
            const std::string candidate = dir.get_path() + File::SEPARATOR + name;
            if (found
                && std::filesystem::weakly_canonical(candidate)
                       != std::filesystem::weakly_canonical(path))
            {
                fail(directive, "'" + name + "' found in more than one include directory");
            }
            if (!found)
            {
                path = candidate;
                found = true;
            }
        }

        if (!found)
        {
            fail(directive, "'" + name + "' not found in the include directories");
        }
    }
    else
    {
        // Relative to the file that has the #include.
        std::string dir;
        for (auto it = m_frames.rbegin(); it != m_frames.rend(); ++it)
        {
            if ((*it)->is_file)
            {
                dir = (*it)->dir;
                break;
            }
        }
        path = trim_dir_path(dir + File::SEPARATOR + name);
    }

    if (!File(path).exists())
    {
        fail(directive, "included file '" + path + "' does not exist");
    }

    AEMU_DEBUG("Preprocessor::_include() - include path: {}", path);
    push_file(path, &directive);
}

void Preprocessor::_macro(Frame &frame, TokenCursor &line)
{
    const Token directive = line.next();

    const Token &name = line.peek();
    if (!name.is(TokenType::SYMBOL))
    {
        fail(where(line), "expected a macro name after #macro, got " + describe_next(line));
    }
    line.next();

    if (!line.check(TokenType::OPEN_PARENTHESIS))
    {
        fail(where(line), "expected '(' after the macro name, got " + describe_next(line));
    }

    Macro macro;
    macro.name = name.str();
    macro.params = parse_params(line);
    if (!line.at_end())
    {
        fail(line.peek(), "unexpected " + describe_next(line) + " after the macro header");
    }

    // The body is everything up to #macend and is not interpreted until it is invoked.
    while (true)
    {
        const Token &token = frame.cursor.peek();
        if (token.is(TokenType::END_OF_FILE))
        {
            fail(directive, "#macro without a matching #macend");
        }
        if (token.is(TokenType::PREPROCESSOR_MACRO))
        {
            fail(token, "macro definitions cannot be nested");
        }
        if (token.is(TokenType::PREPROCESSOR_MACEND))
        {
            break;
        }
        macro.body.push_back(token);
        frame.cursor.next();
    }

    TokenCursor end(frame.cursor.take_line());
    end.next(); // '#macend'
    if (!end.at_end())
    {
        fail(end.peek(), "unexpected " + describe_next(end) + " after #macend");
    }

    const std::string key = macro.name + "/" + std::to_string(macro.params.size());
    if (m_macros.find(key) != m_macros.end())
    {
        fail(name, "macro '" + macro.name + "' with " + std::to_string(macro.params.size())
                       + " parameter(s) is already defined");
    }
    m_macros.emplace(key, std::move(macro));
}

void Preprocessor::_macret(Frame &frame, TokenCursor &line)
{
    const Token directive = line.next();
    if (frame.macro == nullptr)
    {
        fail(directive, "#macret outside of a macro");
    }

    Tokens value;
    while (!line.at_end())
    {
        value.push_back(line.next());
    }

    if (!frame.output.empty())
    {
        Symbol symbol;
        symbol.value = std::move(value);
        m_symbols[frame.output][0] = std::move(symbol);
    }
    else if (!value.empty())
    {
        fail(directive, "#macret has a value but the #invoke has no symbol to put it in");
    }

    // Leaves the macro, closing the blocks it left open.
    m_conds.resize(std::min(m_conds.size(), frame.cond_base));
    frame.cursor.rewind(frame.tail);
}

void Preprocessor::_invoke(TokenCursor &line)
{
    const Token directive = line.next();

    const Token &name = line.peek();
    if (!name.is(TokenType::SYMBOL))
    {
        fail(where(line), "expected a macro name after #invoke, got " + describe_next(line));
    }
    line.next();

    if (!line.check(TokenType::OPEN_PARENTHESIS))
    {
        fail(where(line), "expected '(' after the macro name, got " + describe_next(line));
    }
    std::vector<Tokens> args;
    if (!parse_call_args(line, args))
    {
        fail(name, "missing ')' after the macro arguments");
    }

    std::string output;
    if (line.check(TokenType::SYMBOL))
    {
        output = line.next().str();
    }
    if (!line.at_end())
    {
        fail(line.peek(), "unexpected " + describe_next(line) + " after the macro arguments");
    }

    const auto found = m_macros.find(name.str() + "/" + std::to_string(args.size()));
    if (found == m_macros.end())
    {
        fail(name, "no macro '" + name.str() + "' is defined with " + std::to_string(args.size())
                       + " argument(s)");
    }
    const Macro &macro = found->second;

    if (m_frames.size() >= kMaxFrames)
    {
        fail(directive,
             "macro expansion nested too deeply, does '" + macro.name + "' invoke itself?");
    }

    // The expansion is its own scope so labels in a macro do not clash between invocations.
    Tokens expansion;
    expansion.push_back(Token::synthetic(TokenType::ASSEMBLER_SCOPE, ".scope", &directive));
    expansion.push_back(Token::synthetic(TokenType::NEWLINE, "\n", &directive));

    const U32 expansion_id = m_sources->add_expansion(macro.name, true, directive.loc);
    const Tokens body = substitute(macro.body, macro.params, args, expansion_id);
    expansion.insert(expansion.end(), body.begin(), body.end());

    const std::size_t tail = expansion.size();
    expansion.push_back(Token::synthetic(TokenType::ASSEMBLER_SCEND, ".scend", &directive));
    expansion.push_back(Token::synthetic(TokenType::NEWLINE, "\n", &directive));

    Frame &pushed = push_frame(std::move(expansion));
    pushed.macro = &macro;
    pushed.output = std::move(output);
    pushed.tail = tail;
}

void Preprocessor::_define(TokenCursor &line)
{
    line.next(); // '#define'

    const Token &name = line.peek();
    if (!name.is(TokenType::SYMBOL))
    {
        fail(where(line), "expected a symbol after #define, got " + describe_next(line));
    }
    line.next();

    Symbol symbol;
    if (line.check(TokenType::OPEN_PARENTHESIS) && !line.peek().has(TokenFlag::SPACE_BEFORE))
    {
        symbol.params = parse_params(line);
    }
    while (!line.at_end())
    {
        symbol.value.push_back(line.next());
    }

    // A new definition with the same number of parameters replaces the old one.
    m_symbols[name.str()][symbol.params.size()] = std::move(symbol);
}

void Preprocessor::define_from_options()
{
    for (const auto &[name, value] : m_options.defines)
    {
        // Lexed as the line `#define name value`, so it means what that line means in a source
        // and a bad name is reported the same way.
        const basm::SourceId source =
            m_sources->add("<-D " + name + ">", "#define " + name + " " + value + "\n");
        basm::LexResult lexed = basm::lex(*m_sources, source);
        basm::fatal_if_errors(*m_sources, lexed);

        TokenCursor all(std::span<const Token>(lexed.tokens));
        TokenCursor line(all.take_line());
        _define(line);
    }
}

void Preprocessor::_undef(TokenCursor &line)
{
    line.next(); // '#undef'

    const Token &name = line.peek();
    if (!name.is(TokenType::SYMBOL))
    {
        fail(where(line), "expected a symbol after #undef, got " + describe_next(line));
    }
    line.next();

    const auto it = m_symbols.find(name.str());
    if (line.check(TokenType::LITERAL_NUMBER_DECIMAL))
    {
        const std::size_t num_params = static_cast<std::size_t>(line.next().int_value);
        if (it != m_symbols.end())
        {
            it->second.erase(num_params);
            if (it->second.empty())
            {
                m_symbols.erase(it);
            }
        }
    }
    else if (it != m_symbols.end())
    {
        m_symbols.erase(it);
    }

    if (!line.at_end())
    {
        fail(line.peek(), "unexpected " + describe_next(line) + " after #undef");
    }
}

bool Preprocessor::evaluate_condition(const Token &directive, TokenCursor &line)
{
    const Token &operand = line.peek();
    if (line.at_end())
    {
        fail(where(line), "expected a symbol after " + directive.str() + ", got end of line");
    }

    if (!is_value_conditional(directive.type))
    {
        if (!operand.is(TokenType::SYMBOL))
        {
            fail(operand,
                 "expected a symbol after " + directive.str() + ", got " + basm::describe(operand));
        }
        const std::string name = operand.str();
        line.next();
        if (!line.at_end())
        {
            fail(line.peek(), "unexpected " + describe_next(line) + " after " + directive.str());
        }

        const bool defined = is_symbol_def(name, 0);
        return directive.is_one_of({TokenType::PREPROCESSOR_IFDEF, TokenType::PREPROCESSOR_ELSEDEF})
                   ? defined
                   : !defined;
    }

    // The left side is the value of a symbol, which is empty if it is not defined. It can also
    // be a single token that is not a symbol, which is what a macro parameter becomes once the
    // argument is substituted for it (`#ifequ mode 1` in a macro is `#ifequ 2 1` when invoked
    // with 2), and then it is the text of that token.
    std::string symbol_value;
    if (operand.is(TokenType::SYMBOL))
    {
        const std::string name = operand.str();
        if (is_symbol_def(name, 0))
        {
            symbol_value = join(m_symbols.at(name).at(0).value);
        }
    }
    else
    {
        symbol_value = operand.str();
    }
    line.next();

    Tokens rest;
    while (!line.at_end())
    {
        rest.push_back(line.next());
    }
    const std::string value = join(rest);

    switch (directive.type)
    {
    case TokenType::PREPROCESSOR_IFEQU:
    case TokenType::PREPROCESSOR_ELSEEQU:
        return compare_values(symbol_value, value) == 0;
    case TokenType::PREPROCESSOR_IFNEQU:
    case TokenType::PREPROCESSOR_ELSENEQU:
        return compare_values(symbol_value, value) != 0;
    case TokenType::PREPROCESSOR_IFLESS:
    case TokenType::PREPROCESSOR_ELSELESS:
        return compare_values(symbol_value, value) < 0;
    default:
        return compare_values(symbol_value, value) > 0;
    }
}

void Preprocessor::_conditional(Frame &frame, TokenCursor &line)
{
    const Token directive = line.next();

    if (is_if(directive.type))
    {
        Cond cond{};
        cond.loc = directive.loc;
        cond.parent_active = active();
        if (!cond.parent_active)
        {
            // Nothing in here is looked at, but the block still has to be matched up.
            cond.taken = true;
        }
        else
        {
            cond.active = evaluate_condition(directive, line);
            cond.taken = cond.active;
        }
        m_conds.push_back(cond);
        return;
    }

    if (m_conds.size() <= frame.cond_base)
    {
        fail(directive, directive.str() + " without a matching #if");
    }

    if (directive.is(TokenType::PREPROCESSOR_ENDIF))
    {
        if (!line.at_end())
        {
            fail(line.peek(), "unexpected " + describe_next(line) + " after #endif");
        }
        m_conds.pop_back();
        return;
    }

    Cond &cond = m_conds.back();
    if (cond.seen_else)
    {
        fail(directive, directive.str() + " after #else");
    }

    const bool plain_else = directive.is(TokenType::PREPROCESSOR_ELSE);
    if (plain_else)
    {
        if (!line.at_end())
        {
            fail(line.peek(), "unexpected " + describe_next(line) + " after #else");
        }
        cond.seen_else = true;
    }

    if (!cond.parent_active)
    {
        return;
    }

    if (cond.taken)
    {
        cond.active = false;
        return;
    }

    cond.active = plain_else || evaluate_condition(directive, line);
    cond.taken = cond.active;
}
