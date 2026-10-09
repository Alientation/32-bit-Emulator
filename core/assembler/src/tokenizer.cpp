#include "assembler/tokenizer.h"

#include "util/logger.h"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <optional>
#include <unordered_map>

namespace basm
{

// ---------------------------------------------------------------------------------------------
// SourceManager
// ---------------------------------------------------------------------------------------------

SourceId SourceManager::add(std::string name, std::string text)
{
    Source source;
    source.name = std::move(name);
    source.text = std::move(text);

    // A line ends with "\n", "\r\n" or a lone "\r".
    source.line_starts.push_back(0);
    const std::string &t = source.text;
    for (std::size_t i = 0; i < t.size(); i++)
    {
        if (t[i] == '\n' || (t[i] == '\r' && (i + 1 >= t.size() || t[i + 1] != '\n')))
        {
            source.line_starts.push_back(U32(i + 1));
        }
    }

    m_sources.push_back(std::move(source));
    return SourceId(m_sources.size() - 1);
}

SourceId SourceManager::add_file(const std::string &path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) return kInvalidSource;
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return add(path, std::move(text));
}

const SourceManager::Source *SourceManager::get(SourceId id) const
{
    return id < m_sources.size() ? &m_sources[id] : nullptr;
}

std::string_view SourceManager::name(SourceId id) const
{
    const Source *s = get(id);
    return s ? std::string_view(s->name) : std::string_view();
}

std::string_view SourceManager::text(SourceId id) const
{
    const Source *s = get(id);
    return s ? std::string_view(s->text) : std::string_view();
}

std::string_view SourceManager::line_text(SourceId id, U32 line) const
{
    const Source *s = get(id);
    if (s == nullptr || line == 0 || line > s->line_starts.size()) return {};

    const std::size_t begin = s->line_starts[line - 1];
    std::size_t end = line < s->line_starts.size() ? s->line_starts[line] : s->text.size();
    while (end > begin && (s->text[end - 1] == '\n' || s->text[end - 1] == '\r')) end--;
    return std::string_view(s->text).substr(begin, end - begin);
}

U32 SourceManager::add_expansion(std::string name, bool is_macro, const SourceLocation &site)
{
    m_expansions.push_back({std::move(name), is_macro, site});
    return static_cast<U32>(m_expansions.size());
}

const SourceManager::Expansion *SourceManager::expansion(U32 id) const
{
    if (id == 0 || id > m_expansions.size()) return nullptr;
    return &m_expansions[id - 1];
}

// ---------------------------------------------------------------------------------------------
// Token helpers
// ---------------------------------------------------------------------------------------------

std::string_view to_string(TokenType type)
{
    static constexpr std::string_view kNames[] = {
#define BASM_X(name) #name,
        BASM_TOKEN_TYPES(BASM_X)
#undef BASM_X
    };
    const std::size_t i = static_cast<std::size_t>(type);
    return i < std::size(kNames) ? kNames[i] : std::string_view("UNKNOWN");
}

std::string describe(const Token &token)
{
    switch (token.type)
    {
    case TokenType::END_OF_FILE:
        return "end of file";
    case TokenType::NEWLINE:
        return "end of line";
    default:
        return "'" + std::string(token.text) + "'";
    }
}

namespace
{

static constexpr bool is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static constexpr bool is_ident_start(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static constexpr bool is_ident_char(char c)
{
    return is_ident_start(c) || is_digit(c);
}

static constexpr bool is_newline(char c)
{
    return c == '\n' || c == '\r';
}

static constexpr int digit_value(char c)
{
    if (c >= '0' && c <= '9')
    {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f')
    {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F')
    {
        return c - 'A' + 10;
    }
    return -1;
}

static constexpr U64 escape_value(char c)
{
    switch (c)
    {
    case 'n':
        return '\n';
    case 't':
        return '\t';
    case 'r':
        return '\r';
    case '0':
        return '\0';
    case 'a':
        return '\a';
    case 'b':
        return '\b';
    case 'f':
        return '\f';
    case 'v':
        return '\v';
    default:
        return (unsigned char) (c);
    }
}

bool digits_valid(std::string_view digits, int base)
{
    for (char c : digits)
    {
        const int d = digit_value(c);
        if (d < 0 || d >= base) return false;
    }
    return true;
}

/// Digits must already be validated. Returns false on overflow.
bool parse_uint(std::string_view digits, int base, U64 &out)
{
    U64 value = 0;
    for (char c : digits)
    {
        const U64 d = U64(digit_value(c));
        if (value > (UINT64_MAX - d) / U64(base)) return false;
        value = value * U64(base) + d;
    }
    out = value;
    return true;
}

} // namespace

std::string unescape_string_literal(const Token &token)
{
    std::string_view body = token.text;
    if (body.size() >= 2 && body.front() == '"' && body.back() == '"')
    {
        body = body.substr(1, body.size() - 2);
    }

    std::string result;
    result.reserve(body.size());
    for (std::size_t i = 0; i < body.size(); i++)
    {
        if (body[i] == '\\' && i + 1 < body.size())
        {
            result.push_back(char(escape_value(body[++i])));
        }
        else
        {
            result.push_back(body[i]);
        }
    }
    return result;
}

// ---------------------------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------------------------

std::string format_diagnostic(const SourceManager &sources, const Diagnostic &diagnostic)
{
    const char *severity = diagnostic.severity == Severity::ERROR ? "error" : "warning";
    const SourceLocation &loc = diagnostic.loc;

    if (loc.line == 0 || loc.source == kInvalidSource)
    {
        return std::string(severity) + ": " + diagnostic.message + "\n";
    }

    // `file:line:col: <severity>: <message>`, the source line and a caret.
    auto entry =
        [&sources](const SourceLocation &at, const std::string &label, const std::string &message)
    {
        std::string out = std::string(sources.name(at.source)) + ":" + std::to_string(at.line) + ":"
                          + std::to_string(at.column) + ": " + label + ": " + message + "\n";

        const std::string_view line = sources.line_text(at.source, at.line);
        out += "  " + std::string(line) + "\n  ";
        for (std::size_t i = 0; i + 1 < at.column; i++)
        {
            // Keep tabs so the caret lines up whatever the tab width is.
            out += (i < line.size() && line[i] == '\t') ? '\t' : ' ';
        }
        out += "^\n";
        return out;
    };

    std::string out = entry(loc, severity, diagnostic.message);

    // Where the macros and symbols that produced the token were used, innermost first. Every
    // site was recorded before the expansion that refers to it, so the chain is finite.
    for (U32 id = loc.expansion; id != 0;)
    {
        const SourceManager::Expansion *expansion = sources.expansion(id);
        if (expansion == nullptr || expansion->site.line == 0
            || expansion->site.source == kInvalidSource)
        {
            break;
        }
        out += entry(expansion->site, "note",
                     std::string("in expansion of ") + (expansion->is_macro ? "macro" : "symbol")
                         + " '" + expansion->name + "'");
        id = expansion->site.expansion;
    }
    return out;
}

// ---------------------------------------------------------------------------------------------
// Keyword tables
// ---------------------------------------------------------------------------------------------

namespace
{

struct Keyword
{
    TokenType type;
    U8 flags;
    bool allows_s; ///< `<name>s` is the flag setting variant.
};

using KeywordMap = std::unordered_map<std::string_view, Keyword>;

const KeywordMap &assembly_keywords()
{
    static const KeywordMap map = []
    {
        KeywordMap m;
        auto add = [&m](std::string_view text, TokenType type, bool allows_s = false, U8 flags = 0)
        { m[text] = Keyword{.type = type, .flags = flags, .allows_s = allows_s}; };

        add("sp", TokenType::REGISTER_SP);
        add("xzr", TokenType::REGISTER_XZR);

        // Every instruction of instruction_list.h under the text of its row.
#define BASM_KEYWORD(X, NAME, text, allows_s, ...)                                                 \
    add(text, TokenType::INSTRUCTION_##NAME, allows_s);
        BASM_INSTRUCTION_LIST(BASM_KEYWORD, )
#undef BASM_KEYWORD

        // Other spellings.
        add("vcint.s32.f32", TokenType::INSTRUCTION_VCINT);
        add("vcflo.s32.f32", TokenType::INSTRUCTION_VCFLO);
        add("ldrsb", TokenType::INSTRUCTION_LDRB, false, SIGN_EXTEND);
        add("ldrsh", TokenType::INSTRUCTION_LDRH, false, SIGN_EXTEND);
        return m;
    }();
    return map;
}

const KeywordMap &condition_keywords()
{
    static const KeywordMap map = []
    {
        KeywordMap m;
#define C(text, name)                                                                              \
    m[text] = Keyword{.type = TokenType::CONDITION_##name, .flags = 0, .allows_s = false}
        C("eq", EQ);
        C("ne", NE);
        C("cs", CS);
        C("hs", HS);
        C("cc", CC);
        C("lo", LO);
        C("mi", MI);
        C("pl", PL);
        C("vs", VS);
        C("vc", VC);
        C("hi", HI);
        C("ls", LS);
        C("ge", GE);
        C("lt", LT);
        C("gt", GT);
        C("le", LE);
        C("al", AL);
        C("nv", NV);
#undef C
        return m;
    }();
    return map;
}

const KeywordMap &assembler_directives()
{
    static const KeywordMap map = []
    {
        KeywordMap m;
#define D(text, name)                                                                              \
    m[text] = Keyword{.type = TokenType::ASSEMBLER_##name, .flags = 0, .allows_s = false}
        D(".global", GLOBAL);
        D(".extern", EXTERN);
        D(".weak", WEAK);
        D(".comm", COMM);
        D(".rodata", RODATA);
        D(".init_array", INIT_ARRAY);
        D(".fini_array", FINI_ARRAY);
        D(".equ", EQU);
        D(".org", ORG);
        D(".scope", SCOPE);
        D(".scend", SCEND);
        D(".advance", ADVANCE);
        D(".fill", FILL);
        D(".align", ALIGN);
        D(".section", SECTION);
        D(".pushsection", PUSHSECTION);
        D(".popsection", POPSECTION);
        D(".bss", BSS);
        D(".data", DATA);
        D(".text", TEXT);
        D(".stop", STOP);
        D(".byte", BYTE);
        D(".dbyte", DBYTE);
        D(".word", WORD);
        D(".dword", DWORD);
        D(".ascii", ASCII);
        D(".asciz", ASCIZ);
#undef D
        return m;
    }();
    return map;
}

const KeywordMap &linker_keywords()
{
    static const KeywordMap map = []
    {
        KeywordMap m;
        m["ENTRY"] = Keyword{.type = TokenType::KEYWORD_ENTRY, .flags = 0, .allows_s = false};
        m["SECTIONS"] = Keyword{.type = TokenType::KEYWORD_SECTIONS, .flags = 0, .allows_s = false};
        return m;
    }();
    return map;
}

const KeywordMap &linker_directives()
{
    static const KeywordMap map = []
    {
        KeywordMap m;
        m[".text"] = Keyword{.type = TokenType::ASSEMBLER_TEXT, .flags = 0, .allows_s = false};
        m[".data"] = Keyword{.type = TokenType::ASSEMBLER_DATA, .flags = 0, .allows_s = false};
        m[".bss"] = Keyword{.type = TokenType::ASSEMBLER_BSS, .flags = 0, .allows_s = false};
        m[".rodata"] = Keyword{.type = TokenType::ASSEMBLER_RODATA, .flags = 0, .allows_s = false};
        m[".init_array"] =
            Keyword{.type = TokenType::ASSEMBLER_INIT_ARRAY, .flags = 0, .allows_s = false};
        m[".fini_array"] =
            Keyword{.type = TokenType::ASSEMBLER_FINI_ARRAY, .flags = 0, .allows_s = false};
        return m;
    }();
    return map;
}

const KeywordMap &preprocessor_directives()
{
    static const KeywordMap map = []
    {
        KeywordMap m;
#define P(text, name)                                                                              \
    m[text] = Keyword{.type = TokenType::PREPROCESSOR_##name, .flags = 0, .allows_s = false}
        P("#include", INCLUDE);
        P("#macro", MACRO);
        P("#macret", MACRET);
        P("#macend", MACEND);
        P("#invoke", INVOKE);
        P("#define", DEFINE);
        P("#undef", UNDEF);
        P("#ifdef", IFDEF);
        P("#ifndef", IFNDEF);
        P("#ifequ", IFEQU);
        P("#ifnequ", IFNEQU);
        P("#ifless", IFLESS);
        P("#ifmore", IFMORE);
        P("#else", ELSE);
        P("#elsedef", ELSEDEF);
        P("#elsendef", ELSENDEF);
        P("#elseequ", ELSEEQU);
        P("#elsenequ", ELSENEQU);
        P("#elseless", ELSELESS);
        P("#elsemore", ELSEMORE);
        P("#endif", ENDIF);
#undef P
        return m;
    }();
    return map;
}

/// Exact match, or the flag setting `s` variant of an instruction that allows it.
std::optional<Keyword> find_keyword(const KeywordMap &map, std::string_view text)
{
    if (auto it = map.find(text); it != map.end()) return it->second;
    if (text.size() > 1 && text.back() == 's')
    {
        if (auto it = map.find(text.substr(0, text.size() - 1));
            it != map.end() && it->second.allows_s)
        {
            return Keyword{.type = it->second.type,
                           .flags = U8(it->second.flags | SETS_FLAGS),
                           .allows_s = false};
        }
    }
    return std::nullopt;
}

/// x0 - x29 only (no leading zeros).
std::optional<TokenType> parse_register(std::string_view text)
{
    if (text.size() < 2 || text.size() > 3 || text[0] != 'x' || !is_digit(text[1]))
        return std::nullopt;
    if (text.size() == 3 && (!is_digit(text[2]) || text[1] == '0')) return std::nullopt;
    const int n = text.size() == 2 ? text[1] - '0' : (text[1] - '0') * 10 + (text[2] - '0');
    if (n > 29) return std::nullopt;
    return TokenType(U16(TokenType::REGISTER_X0) + n);
}

// ---------------------------------------------------------------------------------------------
// Lexer
// ---------------------------------------------------------------------------------------------

class Lexer
{
  public:
    Lexer(std::string_view src, SourceId id, const LexOptions &options) :
        m_src(src),
        m_id(id),
        m_opts(options)
    {
    }

    LexResult run()
    {
        const std::size_t n = m_src.size();
        while (m_pos < n)
        {
            const char c = m_src[m_pos];
            if (c == ' ' || c == '\t' || c == '\f' || c == '\v')
            {
                m_pos++;
                m_space = true;
            }
            else if (is_newline(c))
            {
                lex_newline();
            }
            else if (c == '\\')
            {
                lex_backslash();
            }
            else if (!lex_comment())
            {
                lex_token();
            }
        }

        // Every statement is terminated, including the last one.
        if (m_opts.keep_newlines && !m_result.tokens.empty()
            && m_result.tokens.back().type != TokenType::NEWLINE)
        {
            emit_text(TokenType::NEWLINE, n, 0, here());
        }
        m_space = false;
        emit_text(TokenType::END_OF_FILE, n, 0, here());
        return std::move(m_result);
    }

  private:
    // ----- location / emit helpers -----

    SourceLocation here() const
    {
        return {.source = m_id,
                .offset = U32(m_pos),
                .line = m_line,
                .column = U32(m_pos - m_line_start + 1)};
    }

    char peek(std::size_t ahead = 0) const
    {
        return m_pos + ahead < m_src.size() ? m_src[m_pos + ahead] : '\0';
    }

    void emit_text(TokenType type, std::size_t begin, std::size_t len, const SourceLocation &loc,
                   U8 flags = 0, U64 value = 0)
    {
        Token tok;
        tok.type = type;
        tok.flags = flags;
        if (m_space) tok.flags |= SPACE_BEFORE;
        if (m_first) tok.flags |= FIRST_ON_LINE;
        tok.text = m_src.substr(begin, len);
        tok.loc = loc;
        tok.int_value = value;
        m_result.tokens.push_back(tok);
        m_space = false;
        m_first = false;
    }

    /// Token covering [begin, m_pos).
    void emit(TokenType type, std::size_t begin, const SourceLocation &loc, U8 flags = 0,
              U64 value = 0)
    {
        emit_text(type, begin, m_pos - begin, loc, flags, value);
    }

    void report(Severity severity, const SourceLocation &loc, std::string message)
    {
        if (severity == Severity::ERROR) m_result.has_errors = true;
        m_result.diagnostics.push_back({severity, loc, std::move(message)});
    }

    void error(const SourceLocation &loc, std::string message)
    {
        report(Severity::ERROR, loc, std::move(message));
    }

    /// Moves to `end`, keeping line information right for any line breaks skipped.
    void advance_to(std::size_t end)
    {
        for (std::size_t i = m_pos; i < end; i++)
        {
            if (m_src[i] == '\n'
                || (m_src[i] == '\r' && (i + 1 >= m_src.size() || m_src[i + 1] != '\n')))
            {
                m_line++;
                m_line_start = i + 1;
            }
        }
        m_pos = end;
    }

    bool prev_is_operand() const
    {
        if (m_result.tokens.empty())
        {
            return false;
        }
        const TokenType t = m_result.tokens.back().type;
        return t == TokenType::SYMBOL || is_number_literal(t) || t == TokenType::LITERAL_CHAR
               || t == TokenType::CLOSE_PARENTHESIS || t == TokenType::CLOSE_BRACKET;
    }

    bool prev_char_is_ident() const
    {
        return m_pos > 0 && is_ident_char(m_src[m_pos - 1]);
    }

    bool assembly() const
    {
        return m_opts.mode == LexMode::ASSEMBLY;
    }

    // ----- trivia -----

    void lex_newline()
    {
        const std::size_t begin = m_pos;
        const SourceLocation loc = here();
        m_pos += (m_src[m_pos] == '\r' && peek(1) == '\n') ? 2 : 1;

        const bool collapse =
            m_opts.collapse_newlines
            && (m_result.tokens.empty() || m_result.tokens.back().type == TokenType::NEWLINE);
        if (m_opts.keep_newlines && !collapse) emit(TokenType::NEWLINE, begin, loc);

        m_line++;
        m_line_start = m_pos;
        m_space = false;
        m_first = true;
    }

    /// `\` + newline joins lines. Any other backslash is a BACK_SLASH token.
    void lex_backslash()
    {
        std::size_t i = m_pos + 1;
        while (i < m_src.size() && (m_src[i] == ' ' || m_src[i] == '\t')) i++;

        if (i < m_src.size() && is_newline(m_src[i]))
        {
            i += (m_src[i] == '\r' && i + 1 < m_src.size() && m_src[i + 1] == '\n') ? 2 : 1;
            advance_to(i);
            m_space = true;
            return;
        }

        const std::size_t begin = m_pos;
        const SourceLocation loc = here();
        m_pos++;
        emit(TokenType::BACK_SLASH, begin, loc);
    }

    bool lex_comment()
    {
        const char c = m_src[m_pos];
        const char d = peek(1);

        bool single = false;
        bool multi = false;
        if (assembly())
        {
            single = c == ';' && d != '*';
            multi = c == ';' && d == '*';
        }
        else
        {
            single = c == '/' && d == '/';
            multi = c == '/' && d == '*';
        }

        if (!single && !multi) return false;

        const std::size_t begin = m_pos;
        const SourceLocation loc = here();
        if (single)
        {
            while (m_pos < m_src.size() && !is_newline(m_src[m_pos])) m_pos++;
        }
        else
        {
            const std::string_view close = assembly() ? "*;" : "*/";
            std::size_t end = m_src.find(close, m_pos + 2);
            if (end == std::string_view::npos)
            {
                error(loc, "unterminated multi-line comment");
                end = m_src.size();
            }
            else
            {
                end += 2;
            }
            advance_to(end);
        }

        if (m_opts.keep_comments)
        {
            const bool first = m_first;
            emit(multi ? TokenType::COMMENT_MULTI_LINE : TokenType::COMMENT_SINGLE_LINE, begin,
                 loc);
            m_first = first;
        }
        m_space = true;
        return true;
    }

    // ----- tokens -----

    void lex_token()
    {
        const char c = m_src[m_pos];
        const std::size_t begin = m_pos;
        const SourceLocation loc = here();

        if (is_ident_start(c))
        {
            lex_word(begin, loc);
        }
        else if (is_digit(c))
        {
            lex_number(begin, loc);
        }
        else
        {
            switch (c)
            {
            case '.':
                lex_dot(begin, loc);
                break;
            case '#':
                lex_hash(begin, loc);
                break;
            case '"':
                lex_string(begin, loc);
                break;
            case '\'':
                lex_char(begin, loc);
                break;
            case '$':
                if (assembly())
                {
                    lex_prefixed(begin, loc, 1, 16, TokenType::LITERAL_NUMBER_HEXADECIMAL,
                                 "hexadecimal", is_ident_char(peek(1)));
                }
                else
                {
                    lex_punct(begin, loc);
                }
                break;
            case '%':
                if (assembly() && (peek(1) == '0' || peek(1) == '1') && !prev_is_operand())
                {
                    lex_prefixed(begin, loc, 1, 2, TokenType::LITERAL_NUMBER_BINARY, "binary",
                                 true);
                }
                else
                {
                    lex_punct(begin, loc);
                }
                break;
            case '@':
                if (assembly())
                {
                    lex_prefixed(begin, loc, 1, 8, TokenType::LITERAL_NUMBER_OCTAL, "octal",
                                 is_ident_char(peek(1)));
                }
                else
                {
                    lex_punct(begin, loc);
                }
                break;
            default:
                lex_punct(begin, loc);
                break;
            }
        }
    }

    bool is_condition_position(std::size_t begin) const
    {
        const std::vector<Token> &toks = m_result.tokens;
        if (toks.size() < 2 || m_space) return false;
        const Token &dot = toks[toks.size() - 1];
        const Token &mnemonic = toks[toks.size() - 2];
        return dot.type == TokenType::PERIOD && !dot.has(SPACE_BEFORE)
               && dot.loc.offset + 1 == begin
               && (mnemonic.type == TokenType::INSTRUCTION_B
                   || mnemonic.type == TokenType::INSTRUCTION_BL
                   || mnemonic.type == TokenType::INSTRUCTION_BX
                   || mnemonic.type == TokenType::INSTRUCTION_BLX
                   || mnemonic.type == TokenType::INSTRUCTION_SWI);
    }

    /// The condition of the conditional select instructions is the last operand: `csel x0, x1, x2,
    /// lt`, `cset x0, eq`. It follows a comma instead of a dot.
    bool is_select_condition_position() const
    {
        const std::vector<Token> &toks = m_result.tokens;
        if (toks.empty() || toks.back().type != TokenType::COMMA)
        {
            return false;
        }

        std::size_t first = toks.size();
        while (first > 0 && toks[first - 1].type != TokenType::NEWLINE)
        {
            first--;
        }
        while (first < toks.size() && toks[first].type == TokenType::LABEL)
        {
            first++;
        }
        return first < toks.size() && toks[first].type >= TokenType::INSTRUCTION_CSEL
               && toks[first].type <= TokenType::INSTRUCTION_CNEG;
    }

    void lex_word(std::size_t begin, const SourceLocation &loc)
    {
        const std::size_t n = m_src.size();
        std::size_t end = begin + 1;
        while (end < n && is_ident_char(m_src[end])) end++;

        // Floating point mnemonics contain dots: vadd.f32, vcint.u32.f32, ...
        if (assembly() && m_src[begin] == 'v' && end < n && m_src[end] == '.')
        {
            for (std::string_view suffix : {std::string_view(".u32.f32"),
                                            std::string_view(".s32.f32"), std::string_view(".f32")})
            {
                if (m_src.compare(end, suffix.size(), suffix) != 0) continue;
                const std::size_t e2 = end + suffix.size();
                if (e2 < n && is_ident_char(m_src[e2])) continue;
                if (assembly_keywords().count(m_src.substr(begin, e2 - begin)) != 0)
                {
                    end = e2;
                    break;
                }
            }
        }

        const std::string_view text = m_src.substr(begin, end - begin);

        // `name:` is a label. The colon is consumed but not part of the token text.
        if (assembly() && end < n && m_src[end] == ':')
        {
            m_pos = end + 1;
            emit_text(TokenType::LABEL, begin, text.size(), loc);
            return;
        }

        m_pos = end;
        TokenType type = TokenType::SYMBOL;
        U8 flags = 0;

        if (assembly())
        {
            std::optional<Keyword> kw;
            if (is_condition_position(begin) || is_select_condition_position())
            {
                kw = find_keyword(condition_keywords(), text);
            }
            if (!kw)
            {
                if (const std::optional<TokenType> reg = parse_register(text))
                    kw = Keyword{*reg, 0, false};
                else kw = find_keyword(assembly_keywords(), text);
            }
            if (kw)
            {
                type = kw->type;
                flags = kw->flags;
            }
        }
        else if (const std::optional<Keyword> kw = find_keyword(linker_keywords(), text))
        {
            type = kw->type;
        }

        emit(type, begin, loc, flags);
    }

    void lex_dot(std::size_t begin, const SourceLocation &loc)
    {
        const char d = peek(1);

        // `.5`
        if (assembly() && is_digit(d) && !prev_char_is_ident())
        {
            m_pos++;
            while (is_digit(peek())) m_pos++;
            finish_float(begin, loc);
            return;
        }

        // `.global`. After an identifier character the dot is a separator (`b.eq`).
        if (is_ident_start(d) && !prev_char_is_ident())
        {
            std::size_t end = begin + 2;
            while (end < m_src.size() && is_ident_char(m_src[end])) end++;
            m_pos = end;

            const KeywordMap &table = assembly() ? assembler_directives() : linker_directives();
            if (const std::optional<Keyword> kw =
                    find_keyword(table, m_src.substr(begin, end - begin)))
            {
                emit(kw->type, begin, loc);
            }
            else
            {
                error(loc,
                      "unknown directive '" + std::string(m_src.substr(begin, end - begin)) + "'");
                emit(TokenType::INVALID, begin, loc);
            }
            return;
        }

        m_pos++;
        emit(TokenType::PERIOD, begin, loc);
    }

    void lex_hash(std::size_t begin, const SourceLocation &loc)
    {
        if (!assembly() || !is_ident_start(peek(1)))
        {
            m_pos++;
            error(loc, "stray '#'");
            emit(TokenType::INVALID, begin, loc);
            return;
        }

        std::size_t end = begin + 2;
        while (end < m_src.size() && is_ident_char(m_src[end])) end++;
        m_pos = end;

        const std::string_view text = m_src.substr(begin, end - begin);
        if (const std::optional<Keyword> kw = find_keyword(preprocessor_directives(), text))
        {
            emit(kw->type, begin, loc);
        }
        else
        {
            error(loc, "unknown preprocessor directive '" + std::string(text) + "'");
            emit(TokenType::INVALID, begin, loc);
        }
    }

    void lex_string(std::size_t begin, const SourceLocation &loc)
    {
        const std::size_t n = m_src.size();
        std::size_t i = begin + 1;
        while (i < n && m_src[i] != '"' && !is_newline(m_src[i]))
        {
            if (m_src[i] == '\\' && i + 1 < n && !is_newline(m_src[i + 1])) i++;
            i++;
        }

        if (i < n && m_src[i] == '"')
        {
            m_pos = i + 1;
            emit(TokenType::LITERAL_STRING, begin, loc);
        }
        else
        {
            m_pos = i;
            error(loc, "unterminated string literal");
            emit(TokenType::INVALID, begin, loc);
        }
    }

    void lex_char(std::size_t begin, const SourceLocation &loc)
    {
        const std::size_t n = m_src.size();
        std::size_t i = begin + 1;
        U64 value = 0;
        bool ok = i < n && !is_newline(m_src[i]);

        if (ok && m_src[i] == '\\')
        {
            i++;
            ok = i < n && !is_newline(m_src[i]);
            if (ok) value = escape_value(m_src[i++]);
        }
        else if (ok)
        {
            value = static_cast<unsigned char>(m_src[i++]);
        }

        if (ok && i < n && m_src[i] == '\'')
        {
            m_pos = i + 1;
            emit(TokenType::LITERAL_CHAR, begin, loc, 0, value);
        }
        else
        {
            m_pos = begin + 1;
            error(loc, "malformed character literal");
            emit(TokenType::INVALID, begin, loc);
        }
    }

    // ----- numbers -----

    void finish_float(std::size_t begin, const SourceLocation &loc)
    {
        if (is_ident_char(peek()))
        {
            while (is_ident_char(peek())) m_pos++;
            error(loc, "invalid floating point literal '"
                           + std::string(m_src.substr(begin, m_pos - begin)) + "'");
            emit(TokenType::INVALID, begin, loc);
            return;
        }
        emit(TokenType::LITERAL_FLOAT_32, begin, loc);
    }

    /// Integer literal whose digits start after `prefix_len` characters. The whole identifier
    /// run is consumed so that `$12G` is one bad token instead of a number and a symbol.
    /// `consume` false means there is nothing valid after the prefix character (stray `$`).
    void lex_prefixed(std::size_t begin, const SourceLocation &loc, std::size_t prefix_len,
                      int base, TokenType type, const char *name, bool consume)
    {
        std::size_t end = begin + prefix_len;
        if (consume)
        {
            while (end < m_src.size() && is_ident_char(m_src[end])) end++;
        }
        m_pos = end;

        const std::string_view text = m_src.substr(begin, end - begin);
        const std::string_view digits = text.substr(prefix_len);

        std::string hint;
        if (base == 10 && text.size() > 1 && text[0] == '0'
            && (text[1] == 'x' || text[1] == 'X' || text[1] == 'b' || text[1] == 'B'))
        {
            hint = " (hexadecimal is written $FF, binary %101, octal @17)";
        }

        if (digits.empty() || !digits_valid(digits, base))
        {
            error(loc,
                  "invalid " + std::string(name) + " literal '" + std::string(text) + "'" + hint);
            emit(TokenType::INVALID, begin, loc);
            return;
        }

        U64 value = 0;
        if (!parse_uint(digits, base, value))
        {
            error(loc, "integer literal '" + std::string(text) + "' is too large");
            emit(TokenType::INVALID, begin, loc);
            return;
        }
        emit(type, begin, loc, 0, value);
    }

    void lex_number(std::size_t begin, const SourceLocation &loc)
    {
        const std::size_t n = m_src.size();

        if (assembly())
        {
            // 12.5
            std::size_t i = begin;
            while (i < n && is_digit(m_src[i]))
            {
                i++;
            }
            if (i + 1 < n && m_src[i] == '.' && is_digit(m_src[i + 1]))
            {
                i++;
                while (i < n && is_digit(m_src[i]))
                {
                    i++;
                }
                m_pos = i;
                finish_float(begin, loc);
                return;
            }
            lex_prefixed(begin, loc, 0, 10, TokenType::LITERAL_NUMBER_DECIMAL, "decimal", true);
            return;
        }

        // Linker script: 0x.., 0b.., decimal.
        if (m_src[begin] == '0' && (peek(1) == 'x' || peek(1) == 'X'))
        {
            lex_prefixed(begin, loc, 2, 16, TokenType::LITERAL_NUMBER_HEXADECIMAL, "hexadecimal",
                         true);
        }
        else if (m_src[begin] == '0' && (peek(1) == 'b' || peek(1) == 'B'))
        {
            lex_prefixed(begin, loc, 2, 2, TokenType::LITERAL_NUMBER_BINARY, "binary", true);
        }
        else
        {
            lex_prefixed(begin, loc, 0, 10, TokenType::LITERAL_NUMBER_DECIMAL, "decimal", true);
        }
    }

    // ----- punctuation and operators -----

    static std::string describe_byte(char c)
    {
        const unsigned char u = static_cast<unsigned char>(c);
        if (u >= 0x20 && u < 0x7F) return std::string("'") + c + "'";
        char buf[8];
        std::snprintf(buf, sizeof(buf), "0x%02X", u);
        return std::string("byte ") + buf;
    }

    void lex_punct(std::size_t begin, const SourceLocation &loc)
    {
        const char c = m_src[begin];
        const char d = peek(1);

        auto one = [&](TokenType type)
        {
            m_pos = begin + 1;
            emit(type, begin, loc);
        };
        auto two = [&](TokenType type)
        {
            m_pos = begin + 2;
            emit(type, begin, loc);
        };

        // Relocation specifiers: :lo12: :hi20: :lo19: :hi13:
        if (assembly() && c == ':')
        {
            struct Reloc
            {
                std::string_view text;
                TokenType type;
            };

            static constexpr Reloc kRelocs[] = {
                {.text = ":lo12:", .type = TokenType::RELOCATION_EMU32_O_LO12},
                {.text = ":hi20:", .type = TokenType::RELOCATION_EMU32_ADRP_HI20},
                {.text = ":lo19:", .type = TokenType::RELOCATION_EMU32_MOV_LO19},
                {.text = ":hi13:", .type = TokenType::RELOCATION_EMU32_MOV_HI13},
            };
            for (const Reloc &r : kRelocs)
            {
                if (m_src.compare(begin, r.text.size(), r.text) == 0)
                {
                    m_pos = begin + r.text.size();
                    emit(r.type, begin, loc);
                    return;
                }
            }
        }

        switch (c)
        {
        case ',':
            return one(TokenType::COMMA);
        case ':':
            return one(TokenType::COLON);
        case '(':
            return one(TokenType::OPEN_PARENTHESIS);
        case ')':
            return one(TokenType::CLOSE_PARENTHESIS);
        case '[':
            return one(TokenType::OPEN_BRACKET);
        case ']':
            return one(TokenType::CLOSE_BRACKET);
        case '{':
            return one(TokenType::OPEN_BRACE);
        case '}':
            return one(TokenType::CLOSE_BRACE);
        case '+':
            return one(TokenType::OPERATOR_ADDITION);
        case '-':
            return one(TokenType::OPERATOR_SUBTRACTION);
        case '*':
            return one(TokenType::OPERATOR_MULTIPLICATION);
        case '/':
            return one(TokenType::OPERATOR_DIVISION);
        case '%':
            return one(TokenType::OPERATOR_MODULUS);
        case '^':
            return one(TokenType::OPERATOR_BITWISE_XOR);
        case '~':
            return one(TokenType::OPERATOR_BITWISE_COMPLEMENT);
        case '|':
            return d == '|' ? two(TokenType::OPERATOR_LOGICAL_OR)
                            : one(TokenType::OPERATOR_BITWISE_OR);
        case '&':
            return d == '&' ? two(TokenType::OPERATOR_LOGICAL_AND)
                            : one(TokenType::OPERATOR_BITWISE_AND);
        case '!':
            return d == '=' ? two(TokenType::OPERATOR_LOGICAL_NOT_EQUAL)
                            : one(TokenType::OPERATOR_LOGICAL_NOT);
        case '<':
            if (d == '<')
            {
                return two(TokenType::OPERATOR_BITWISE_LEFT_SHIFT);
            }
            return d == '=' ? two(TokenType::OPERATOR_LOGICAL_LESS_THAN_OR_EQUAL)
                            : one(TokenType::OPERATOR_LOGICAL_LESS_THAN);
        case '>':
            if (d == '>')
            {
                return two(TokenType::OPERATOR_BITWISE_RIGHT_SHIFT);
            }
            return d == '=' ? two(TokenType::OPERATOR_LOGICAL_GREATER_THAN_OR_EQUAL)
                            : one(TokenType::OPERATOR_LOGICAL_GREATER_THAN);
        case '=':
            if (d == '=')
            {
                return two(TokenType::OPERATOR_LOGICAL_EQUAL);
            }
            // In assembly it only means something in `ldr xd, =value`.
            return one(TokenType::EQUAL);
        case ';':
            if (!assembly())
            {
                return one(TokenType::SEMICOLON);
            }
            break;
        case '@':
            if (!assembly())
            {
                return one(TokenType::AT);
            }
            break;
        default:
            break;
        }

        // Unknown character. Swallow a whole UTF-8 sequence so one bad glyph is one error.
        std::size_t end = begin + 1;
        if (static_cast<unsigned char>(c) >= 0xC0)
        {
            while (end < m_src.size() && (static_cast<unsigned char>(m_src[end]) & 0xC0) == 0x80)
                end++;
        }
        m_pos = end;
        error(loc, "unexpected character " + describe_byte(c));
        emit(TokenType::INVALID, begin, loc);
    }

    std::string_view m_src;
    SourceId m_id;
    LexOptions m_opts;

    std::size_t m_pos = 0;
    U32 m_line = 1;
    std::size_t m_line_start = 0;
    bool m_space = false;
    bool m_first = true;

    LexResult m_result;
};

} // namespace

LexResult lex(const SourceManager &sources, SourceId id, const LexOptions &options)
{
    return Lexer(sources.text(id), id, options).run();
}

LexResult lex_text(SourceManager &sources, std::string name, std::string text,
                   const LexOptions &options)
{
    const SourceId id = sources.add(std::move(name), std::move(text));
    return lex(sources, id, options);
}

void fatal_if_errors(const SourceManager &sources, const LexResult &result)
{
    std::size_t errors = 0;
    for (const Diagnostic &d : result.diagnostics)
    {
        std::string text = format_diagnostic(sources, d);
        while (!text.empty() && text.back() == '\n') text.pop_back();
        if (d.severity == Severity::ERROR)
        {
            errors++;
            AEMU_ERROR("{}", text);
        }
        else
        {
            AEMU_WARN("{}", text);
        }
    }

    if (errors > 0) AEMU_FATAL("{} lexical error(s), see above.", errors);
}

void fatal_at(const SourceManager &sources, const SourceLocation &loc, const std::string &message)
{
    std::string text = format_diagnostic(sources, {Severity::ERROR, loc, message});
    while (!text.empty() && text.back() == '\n') text.pop_back();
    AEMU_FATAL("{}", text);
}

// ---------------------------------------------------------------------------------------------
// TokenCursor
// ---------------------------------------------------------------------------------------------

const Token &TokenCursor::peek(std::size_t ahead) const
{
    static const Token kEof{};
    if (m_pos + ahead < m_tokens.size()) return m_tokens[m_pos + ahead];
    if (!m_tokens.empty() && m_tokens.back().type == TokenType::END_OF_FILE) return m_tokens.back();
    return kEof;
}

const Token &TokenCursor::next()
{
    const Token &tok = peek();
    if (tok.type != TokenType::END_OF_FILE && m_pos < m_tokens.size()) m_pos++;
    return tok;
}

bool TokenCursor::accept(TokenType type)
{
    return try_next(type) != nullptr;
}

const Token *TokenCursor::try_next(TokenType type)
{
    if (!check(type)) return nullptr;
    return &next();
}

void TokenCursor::skip_newlines()
{
    while (check(TokenType::NEWLINE)) next();
}

void TokenCursor::skip_line()
{
    while (!at_end() && !check(TokenType::NEWLINE)) next();
    accept(TokenType::NEWLINE);
}

std::span<const Token> TokenCursor::take_line()
{
    const std::size_t begin = m_pos;
    while (!at_end() && !check(TokenType::NEWLINE)) next();
    const std::span<const Token> line = m_tokens.subspan(begin, m_pos - begin);
    accept(TokenType::NEWLINE);
    return line;
}

} // namespace basm