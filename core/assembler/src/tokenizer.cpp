#include "assembler/tokenizer.h"

#define AEMU_ONLY_CRITICAL_LOG
#include "util/logger.h"

#include <regex>
#include <utility>

Tokenizer::Tokenizer(File src, bool keep_comments) :
    m_tokens(std::move(tokenize(src, keep_comments)))
{
    if (m_tokens.size() > 0)
    {
        m_tokenize_id = m_tokens[0].tokenize_id;
    }
    else
    {
        WARN("Tokenizing an empty file \'%s\'", src.get_abs_path().c_str());
    }
    verify();
}

Tokenizer::Tokenizer(std::string src, bool keep_comments) :
    m_tokens(std::move(tokenize(src, keep_comments)))
{
    if (m_tokens.size() > 0)
    {
        m_tokenize_id = m_tokens[0].tokenize_id;
    }
    else
    {
        WARN("Tokenizing an empty string.");
    }
    verify();
}

void Tokenizer::verify()
{
    for (Token &tok : m_tokens)
    {
        EXPECT_TRUE(tok.tokenize_id == m_tokenize_id,
                "Tokenizer::verify() - Something went wrong. Expected tokenize id to match at initialization.");
    }
}

size_t Tokenizer::get_toki()
{
    return m_state.toki;
}

struct Tokenizer::State Tokenizer::get_state()
{
    return m_state;
}

void Tokenizer::set_state(Tokenizer::State state)
{
    m_state = state;
}

bool Tokenizer::fix_indent()
{
    if (m_state.cur_indent >= m_state.target_indent)
    {
        return false;
    }

    std::string added;
    for (int indent = m_state.cur_indent; indent < m_state.target_indent; indent++)
    {
        added += "\t";
    }

    insert_tokens(tokenize(added), m_state.toki);
    return true;
}

void Tokenizer::insert_tokens(const std::vector<Token> &tokens, size_t loc)
{
    m_tokens.insert(m_tokens.begin() + loc, tokens.begin(), tokens.end());
}

void Tokenizer::remove_tokens(size_t start, size_t end)
{
    EXPECT_TRUE(start <= end, "Tokenizer::remove_tokens() - Invalid range of tokens to remove. (start > end)");
    EXPECT_TRUE(start < m_tokens.size(), "Tokenizer::remove_tokens() - Start of range is out of bounds.");
    EXPECT_TRUE(end <= m_tokens.size(), "Tokenizer::remove_tokens() - End of range is out of bounds.");

    while (start < end)
    {
        m_tokens[start].skip = true;
        start++;
    }
}

const std::vector<Tokenizer::Token> &Tokenizer::get_tokens()
{
    return m_tokens;
}

int Tokenizer::get_linei(size_t toki)
{
    EXPECT_TRUE(toki < m_tokens.size(), "Tokenizer::get_linei() - Token index out of bounds.");

    for (size_t i = toki; i <= toki; i--)
    {
        Token &tok = m_tokens[i];
        if (tok.tokenize_id != m_tokenize_id)
        {
            continue;
        }

        return tok.line;
    }

    return -1;
}

std::string Tokenizer::get_line(int linei)
{
    std::string line;

    int cur_linei;
    for (Token &tok : m_tokens)
    {
        if (tok.tokenize_id != m_tokenize_id)
        {
            continue;
        }

        for (char &c : tok.value)
        {
            if (c == '\n')
            {
                cur_linei++;
            }

            if (cur_linei == linei)
            {
                line += c;
            }
            else if (cur_linei > linei)
            {
                return line;
            }
        }
    }
    return line;
}

void Tokenizer::move_past_skipped_tokens()
{
    while (m_state.toki < m_tokens.size() && m_tokens[m_state.toki].skip)
    {
        handle_token();
        m_state.toki++;
    }
}

void Tokenizer::handle_token()
{
    // calculate some indent level information
    switch (m_tokens[m_state.toki].type)
    {
        case WHITESPACE_NEWLINE:
            m_state.prev_indent = m_state.cur_indent;
            m_state.cur_indent = 0;
            break;
        case WHITESPACE_TAB:
            m_state.cur_indent++;
            break;
        case LABEL:
            m_state.target_indent = 1;
            break;
        case ASSEMBLER_SCOPE:
            m_state.target_indent++;
            break;
        case PREPROCESSOR_MACRO:
            m_state.target_indent = 1;
            break;
        case ASSEMBLER_SCEND:
            m_state.target_indent--;
            break;
        case PREPROCESSOR_MACEND:
            m_state.target_indent = 0;
            break;
        default:
            break;
    }

    if (m_state.toki + 1 < m_tokens.size())
    {
        switch (m_tokens[m_state.toki + 1].type)
        {
            case LABEL:
                m_state.target_indent = 0;
                break;
            default:
                break;
        }
    }
}

Tokenizer::Token &Tokenizer::get_token()
{
    move_past_skipped_tokens();
    EXPECT_TRUE(has_next(), "Tokenizer::get_token(): Unexpected end of file.");
    return m_tokens[m_state.toki];
}

void Tokenizer::skip_next()
{
    move_past_skipped_tokens();
    EXPECT_TRUE(has_next(), "Tokenizer::skip_next(): Unexpected end of file.");
    handle_token();

    m_state.toki++;
}

void Tokenizer::filter_all(const std::set<Tokenizer::Type> &tok_types)
{
    for (size_t i = 0; i < m_tokens.size(); i++)
    {
        if (m_tokens[i].is(tok_types))
        {
            m_tokens[i].skip = true;
        }
    }
}

void Tokenizer::skip_next_regex(const std::string &regex)
{
    while (has_next() && std::regex_match(m_tokens[m_state.toki].value, std::regex(regex))) {
        skip_next();
    }
}

void Tokenizer::skip_next(const std::set<Tokenizer::Type> &tok_types)
{
    while (has_next() && tok_types.find(m_tokens[m_state.toki].type) != tok_types.end()) {
        skip_next();
    }
}

bool Tokenizer::expect_next(const std::string &error_msg)
{
    EXPECT_TRUE_SS(has_next(), std::stringstream(error_msg));
    return true;
}

bool Tokenizer::expect_next(const std::set<Tokenizer::Type> &expected_types,
                            const std::string &error_msg)
{
    EXPECT_TRUE_SS(has_next(), std::stringstream(error_msg));
    EXPECT_TRUE_SS(expected_types.find(m_tokens[m_state.toki].type) != expected_types.end(),
            std::stringstream(error_msg));
    return true;
}

bool Tokenizer::is_next(const std::set<Tokenizer::Type> &tok_types,
                        const std::string &error_msg)
{
    expect_next(error_msg);
    return tok_types.find(m_tokens[m_state.toki].type) != tok_types.end();
}

bool Tokenizer::has_next()
{
    move_past_skipped_tokens();
    return m_state.toki < m_tokens.size();
}

Tokenizer::Token &Tokenizer::consume(const std::string &error_msg)
{
    expect_next(error_msg);
    Tokenizer::Token &token = m_tokens[m_state.toki];
    skip_next();
    return token;
}

Tokenizer::Token &Tokenizer::consume(const std::set<Tokenizer::Type> &expected_types, const std::string &error_msg)
{
    expect_next(error_msg);
    EXPECT_TRUE_SS(expected_types.find(m_tokens[m_state.toki].type) != expected_types.end(),
            std::stringstream() << error_msg << " - Got " << m_tokens[m_state.toki].to_string());
    Tokenizer::Token &token = m_tokens[m_state.toki];
    skip_next();
    return token;
}


/**
 * Converts the source file contents into a list of tokens
 *
 * @param srcFile The source file to tokenize
 * @return A list of tokens
 */
std::vector<Tokenizer::Token> Tokenizer::tokenize(File srcFile, bool keep_comments)
{
    DEBUG("Tokenizer::tokenize() - Tokenizing file: %s", srcFile.get_name().c_str());
    FileReader reader(srcFile);

    // append a new line to the end to allow regex matching to match an ending whitespace
    std::string source_code = reader.read_all() + "\n";
    reader.close();

    std::vector<Token> tokens = tokenize(source_code, keep_comments);
    DEBUG("Tokenizer::tokenize() - Tokenized file: %s", srcFile.get_name().c_str());
    return tokens;
}

/**
 * Converts the source code into a list of tokens
 *
 * @param source_code The source code to tokenize
 * @return A list of tokens
 */
std::vector<Tokenizer::Token> Tokenizer::tokenize(std::string source_code, bool keep_comments)
{
    static int TOKENIZE_IDS = 0;
    int tokenize_id = TOKENIZE_IDS++;
    int cur_line = 0;

    std::vector<Token> tokens;
    auto is_alphanumeric = [](char c, int index)
    {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                (c == '.' && index == 0) || (c == '_') || (c == '#' && index == 0);
    };

    std::unordered_map<std::string, Type> simple_map =
    {
        {"x0", REGISTER_X0}, {"x1", REGISTER_X1},
        {"x2", REGISTER_X2}, {"x3", REGISTER_X3},
        {"x4", REGISTER_X4}, {"x5", REGISTER_X5},
        {"x6", REGISTER_X6}, {"x7", REGISTER_X7},
        {"x8", REGISTER_X8}, {"x9", REGISTER_X9},
        {"x10", REGISTER_X10}, {"x11", REGISTER_X11},
        {"x12", REGISTER_X12}, {"x13", REGISTER_X13},
        {"x14", REGISTER_X14}, {"x15", REGISTER_X15},
        {"x16", REGISTER_X16}, {"x17", REGISTER_X17},
        {"x18", REGISTER_X18}, {"x19", REGISTER_X19},
        {"x20", REGISTER_X20}, {"x21", REGISTER_X21},
        {"x22", REGISTER_X22}, {"x23", REGISTER_X23},
        {"x24", REGISTER_X24}, {"x25", REGISTER_X25},
        {"x26", REGISTER_X26}, {"x27", REGISTER_X27},
        {"x28", REGISTER_X28}, {"x29", REGISTER_X29},
        {"xzr", REGISTER_XZR}, {"sp", REGISTER_SP},

        {"#include", PREPROCESSOR_INCLUDE},
        {"#macro", PREPROCESSOR_MACRO},
        {"#macret", PREPROCESSOR_MACRET},
        {"#macend", PREPROCESSOR_MACEND},
        {"#invoke", PREPROCESSOR_INVOKE},
        {"#define", PREPROCESSOR_DEFINE},
        {"#undef", PREPROCESSOR_UNDEF},
        {"#ifdef", PREPROCESSOR_IFDEF},
        {"#ifndef", PREPROCESSOR_IFNDEF},
        {"#ifequ", PREPROCESSOR_IFEQU},
        {"#ifnequ", PREPROCESSOR_IFNEQU},
        {"#ifless", PREPROCESSOR_IFLESS},
        {"#ifmore", PREPROCESSOR_IFMORE},
        {"#else", PREPROCESSOR_ELSE},
        {"#elsedef", PREPROCESSOR_ELSEDEF},
        {"#elsendef", PREPROCESSOR_ELSENDEF},
        {"#elseequ", PREPROCESSOR_ELSEEQU},
        {"#elsenequ", PREPROCESSOR_ELSENEQU},
        {"#elseless", PREPROCESSOR_ELSELESS},
        {"#elsemore", PREPROCESSOR_ELSEMORE},
        {"#endif", PREPROCESSOR_ENDIF},

        {".global", ASSEMBLER_GLOBAL},
        {".extern", ASSEMBLER_EXTERN},
        {".org", ASSEMBLER_ORG},
        {".scope", ASSEMBLER_SCOPE},
        {".scend", ASSEMBLER_SCEND},
        {".advance", ASSEMBLER_ADVANCE},
        {".fill", ASSEMBLER_FILL},
        {".align", ASSEMBLER_ALIGN},
        {".section", ASSEMBLER_SECTION},
        {".bss", ASSEMBLER_BSS},
        {".data", ASSEMBLER_DATA},
        {".text", ASSEMBLER_TEXT},
        {".stop", ASSEMBLER_STOP},
        {".byte", ASSEMBLER_BYTE},
        {".dbyte", ASSEMBLER_DBYTE},
        {".word", ASSEMBLER_WORD},
        {".dword", ASSEMBLER_DWORD},
        {".sbyte", ASSEMBLER_SBYTE},
        {".sdbyte", ASSEMBLER_SDBYTE},
        {".sword", ASSEMBLER_SWORD},
        {".sdword", ASSEMBLER_SDWORD},
        {".char", ASSEMBLER_CHAR},
        {".ascii", ASSEMBLER_ASCII},
        {".asciz", ASSEMBLER_ASCIZ},

        {"hlt", INSTRUCTION_HLT},
        {"add", INSTRUCTION_ADD}, {"adds", INSTRUCTION_ADD},
        {"sub", INSTRUCTION_SUB}, {"subs", INSTRUCTION_SUB},
        {"rsb", INSTRUCTION_RSB}, {"rsbs", INSTRUCTION_RSB},
        {"adc", INSTRUCTION_ADC}, {"adcs", INSTRUCTION_ADC},
        {"sbc", INSTRUCTION_SBC}, {"sbcs", INSTRUCTION_SBC},
        {"rsc", INSTRUCTION_RSC}, {"rscs", INSTRUCTION_RSC},
        {"mul", INSTRUCTION_MUL}, {"muls", INSTRUCTION_MUL},
        {"umull", INSTRUCTION_UMULL}, {"umulls", INSTRUCTION_UMULL},
        {"smull", INSTRUCTION_SMULL}, {"smulls", INSTRUCTION_SMULL},
        {"vabs.f32", INSTRUCTION_VABS},
        {"vneg.f32", INSTRUCTION_VNEG},
        {"vsqrt.f32", INSTRUCTION_VSQRT},
        {"vadd.f32", INSTRUCTION_VADD},
        {"vsub.f32", INSTRUCTION_VSUB},
        {"vdiv.f32", INSTRUCTION_VDIV},
        {"vmul.f32", INSTRUCTION_VMUL},
        {"vcmp.f32", INSTRUCTION_VCMP},
        {"vsel.f32", INSTRUCTION_VSEL},
        {"vcint.u32.f32", INSTRUCTION_VCINT}, {"vcint.s32.f32", INSTRUCTION_VCINT},
        {"vcflo.u32.f32", INSTRUCTION_VCFLO}, {"vcflo.s32.f32", INSTRUCTION_VCFLO},
        {"vmov.f32", INSTRUCTION_VMOV},
        {"and", INSTRUCTION_AND}, {"ands", INSTRUCTION_AND},
        {"orr", INSTRUCTION_ORR}, {"orrs", INSTRUCTION_ORR},
        {"eor", INSTRUCTION_EOR}, {"eors", INSTRUCTION_EOR},
        {"bic", INSTRUCTION_BIC}, {"bics", INSTRUCTION_BIC},
        {"lsl", INSTRUCTION_LSL}, {"lsls", INSTRUCTION_LSL},
        {"lsr", INSTRUCTION_LSR}, {"lsrs", INSTRUCTION_LSR},
        {"asr", INSTRUCTION_ASR}, {"asrs", INSTRUCTION_ASR},
        {"ror", INSTRUCTION_ROR}, {"rors", INSTRUCTION_ROR},
        {"cmp", INSTRUCTION_CMP},
        {"cmn", INSTRUCTION_CMN},
        {"tst", INSTRUCTION_TST},
        {"teq", INSTRUCTION_TEQ},
        {"mov", INSTRUCTION_MOV}, {"movs", INSTRUCTION_MOV},
        {"mvn", INSTRUCTION_MVN}, {"mvns", INSTRUCTION_MVN},
        {"ldr", INSTRUCTION_LDR}, {"ldrs", INSTRUCTION_LDR},
        {"str", INSTRUCTION_STR}, {"strs", INSTRUCTION_STR},
        {"swp", INSTRUCTION_SWP}, {"swps", INSTRUCTION_SWP},
        {"ldrb", INSTRUCTION_LDRB}, {"ldrsb", INSTRUCTION_LDRB},
        {"strb", INSTRUCTION_STRB}, {"strsb", INSTRUCTION_STRB},
        {"swpb", INSTRUCTION_SWPB}, {"swpsb", INSTRUCTION_SWPB},
        {"ldrh", INSTRUCTION_LDRH}, {"ldrsh", INSTRUCTION_LDRH},
        {"strh", INSTRUCTION_STRH}, {"strsh", INSTRUCTION_STRH},
        {"swph", INSTRUCTION_SWPH}, {"swpsh", INSTRUCTION_SWPH},
        {"b", INSTRUCTION_B},
        {"bl", INSTRUCTION_BL},
        {"bx", INSTRUCTION_BX},
        {"blx", INSTRUCTION_BLX},
        {"swi", INSTRUCTION_SWI},
        {"adrp", INSTRUCTION_ADRP},

        {"ret", INSTRUCTION_RET},

        {"eq", CONDITION_EQ}, {"ne", CONDITION_NE},
        {"cs", CONDITION_CS}, {"hs", CONDITION_HS},
        {"cc", CONDITION_CC}, {"lo", CONDITION_LO},
        {"mi", CONDITION_MI}, {"pl", CONDITION_PL},
        {"vs", CONDITION_VS}, {"vc", CONDITION_VC},
        {"hi", CONDITION_HI}, {"ls", CONDITION_LS},
        {"ge", CONDITION_GE}, {"lt", CONDITION_LT}, {"gt", CONDITION_GT}, {"le", CONDITION_LE},
        {"al", CONDITION_AL}, {"nv", CONDITION_NV},
    };

    while (source_code.size() > 0)
    {
        // hopefully boost performance
        size_t substring_length = 0;
        while (substring_length < source_code.size() && is_alphanumeric(source_code[substring_length], substring_length))
        {
            substring_length++;
        }

        std::string sub = source_code.substr(0, substring_length);
        if (simple_map.find(sub) != simple_map.end())
        {
            tokens.emplace_back(simple_map.at(sub), sub, cur_line, tokenize_id);
            source_code = source_code.substr(substring_length);
            continue;
        }

        // try to match regex
        bool matched = false;
        for (std::pair<std::string, Type> regexPair : TOKEN_SPEC)
        {
            std::string regex = regexPair.first;
            Type type = regexPair.second;
            std::regex token_regex(regex);
            std::smatch match;
            if (std::regex_search(source_code, match, token_regex))
            {
                // matched regex
                std::string token_value = match.str();

                if (!keep_comments || (type != Tokenizer::COMMENT_SINGLE_LINE && type != Tokenizer::COMMENT_MULTI_LINE))
                {
                    tokens.emplace_back(type, token_value, cur_line, tokenize_id);
                }
                source_code = match.suffix();
                matched = true;

                for (char c : token_value)
                {
                    if (c == '\n')
                    {
                        cur_line++;
                    }
                }

                break;
            }
        }

        // check if regex matched
        EXPECT_TRUE(matched, "Tokenizer::tokenize() - Could not match regex to source code: %s", source_code.c_str());
    }

    for (Tokenizer::Token &token : tokens)
    {
        DEBUG("Token: %s", token.to_string().c_str());
    }

    return tokens;
}

Tokenizer::Token::Token(Tokenizer::Type type, std::string value, int line, int tokenize_id) noexcept :
    type(type),
    value(value),
    line(line),
    tokenize_id(tokenize_id)
{

}

Tokenizer::Token::Token(const Token &tok) noexcept :
    type(tok.type),
    value(tok.value),
    line(-1),
    tokenize_id(-1),
    skip(tok.skip)
{

}

Tokenizer::Token::Token(Token &&tok) noexcept :
    type(std::move(tok.type)),
    value(std::move(tok.value)),
    line(std::exchange(tok.line, -1)),
    tokenize_id(std::exchange(tok.tokenize_id, -1)),
    skip(std::exchange(tok.skip, false))
{

}

Tokenizer::Token &Tokenizer::Token::operator=(const Token &tok) noexcept
{
    type = tok.type;
    value = tok.value;
    line = -1;
    tokenize_id = -1;
    skip = tok.skip;
    return *this;
}

Tokenizer::Token &Tokenizer::Token::operator=(Token &&tok) noexcept
{
    type = std::move(tok.type);
    value = std::move(tok.value);
    line = std::exchange(tok.line, -1);
    tokenize_id = std::exchange(tok.tokenize_id, -1);
    skip = std::exchange(tok.skip, false);
    return *this;
}

std::string Tokenizer::Token::to_string()
{
    if (type == WHITESPACE_SPACE || type == WHITESPACE_TAB || type == WHITESPACE_NEWLINE)
    {
        std::string toString = TYPE_TO_NAME_MAP.at(type) + ":";
        for (size_t i = 0; i < value.length(); i++)
        {
            toString += " " + std::to_string(value[i]);
        }
        return toString + " (" + std::to_string(tokenize_id) + ")";
    }
    else if (type == COMMENT_SINGLE_LINE || type == COMMENT_MULTI_LINE)
    {
        return TYPE_TO_NAME_MAP.at(type) + " (" + std::to_string(tokenize_id) + ")";
    }

    return TYPE_TO_NAME_MAP.at(type) + ": " + value + + " (" + std::to_string(tokenize_id) + ")";
}

bool Tokenizer::Token::is(const std::set<Tokenizer::Type> &types)
{
    return types.find(type) != types.end();
}

int Tokenizer::Token::nlines()
{
    int nlines = 1;
    for (char c : value)
    {
        if (c == '\n')
        {
            nlines++;
        }
    }
    return nlines;
}

const std::unordered_map<Tokenizer::Type, std::string> Tokenizer::TYPE_TO_NAME_MAP =
{
    {UNKNOWN, "UNKNOWN"},

    {LABEL, "LABEL"},
    {TEXT, "TEXT"},
    {WHITESPACE_SPACE, "WHITESPACE_SPACE"}, {WHITESPACE_TAB, "WHITE_SPACE_TAB"}, {WHITESPACE_NEWLINE, "WHITESPACE_NEWLINE"},
    {COMMENT_SINGLE_LINE, "COMMENT_SINGLE_LINE"}, {COMMENT_MULTI_LINE, "COMMENT_MULTI_LINE"},
    {BACK_SLASH, "BACK_SLASH"}, {FORWARD_SLASH, "FORWARD_SLASH"},

    {PREPROCESSOR_INCLUDE, "PREPROCESSOR_INCLUDE"},
    {PREPROCESSOR_MACRO, "PREPROCESSOR_MACRO"}, {PREPROCESSOR_MACRET, "PREPROCESSOR_MACRET"},
    {PREPROCESSOR_MACEND, "PREPROCESSOR_MACEND"}, {PREPROCESSOR_INVOKE, "PREPROCESSOR_INVOKE"},
    {PREPROCESSOR_DEFINE, "PREPROCESSOR_DEFINE"}, {PREPROCESSOR_UNDEF, "PREPROCESSOR_UNDEF"},
    {PREPROCESSOR_IFDEF, "PREPROCESSOR_IFDEF"}, {PREPROCESSOR_IFNDEF, "PREPROCESSOR_IFNDEF"},
    {PREPROCESSOR_IFEQU, "PREPROCESSOR_IFEQU"}, {PREPROCESSOR_IFNEQU, "PREPROCESSOR_IFNEQU"},
    {PREPROCESSOR_IFLESS, "PREPROCESSOR_IFLESS"}, {PREPROCESSOR_IFMORE, "PREPROCESSOR_IFMORE"},
    {PREPROCESSOR_ELSE, "PREPROCESSOR_ELSE"}, {PREPROCESSOR_ELSEDEF, "PREPROCESSOR_ELSEDEF"},
    {PREPROCESSOR_ELSEEQU, "PREPROCESSOR_ELSEEQU"}, {PREPROCESSOR_ELSENEQU, "PREPROCESSOR_ELSENEQU"},
    {PREPROCESSOR_ELSELESS, "PREPROCESSOR_ELSELESS"}, {PREPROCESSOR_ELSEMORE, "PREPROCESSOR_ELSEMORE"},
    {PREPROCESSOR_ELSENDEF, "PREPROCESSOR_ELSENDEF"},
    {PREPROCESSOR_ENDIF, "PREPROCESSOR_ENDIF"},

    {ASSEMBLER_GLOBAL, "ASSEMBLER_GLOBAL"},
    {ASSEMBLER_EXTERN, "ASSEMBLER_EXTERN"},
    {ASSEMBLER_ORG, "ASSEMBLER_ORG"},
    {ASSEMBLER_SCOPE, "ASSEMBLER_SCOPE"},
    {ASSEMBLER_SCEND, "ASSEMBLER_SCEND"},
    {ASSEMBLER_ADVANCE, "ASSEMBLER_ADVANCE"},
    {ASSEMBLER_FILL, "ASSEMBLER_FILL"},
    {ASSEMBLER_ALIGN, "ASSEMBLER_ALIGN"},
    {ASSEMBLER_SECTION, "ASSEMBLER_SECTION"},
    {ASSEMBLER_BSS, "ASSEMBLER_BSS"},
    {ASSEMBLER_DATA, "ASSEMBLER_DATA"},
    {ASSEMBLER_TEXT, "ASSEMBLER_TEXT"},
    {ASSEMBLER_STOP, "ASSEMBLER_STOP"},
    {ASSEMBLER_BYTE, "ASSEMBLER_BYTE"},
    {ASSEMBLER_DBYTE, "ASSEMBLER_DBYTE"},
    {ASSEMBLER_WORD, "ASSEMBLER_WORD"},
    {ASSEMBLER_DWORD, "ASSEMBLER_DWORD"},
    {ASSEMBLER_SBYTE, "ASSEMBLER_SBYTE"},
    {ASSEMBLER_SDBYTE, "ASSEMBLER_SDBYTE"},
    {ASSEMBLER_SWORD, "ASSEMBLER_SWORD"},
    {ASSEMBLER_SDWORD, "ASSEMBLER_SDWORD"},
    {ASSEMBLER_CHAR, "ASSEMBLER_CHAR"},
    {ASSEMBLER_ASCII, "ASSEMBLER_ASCII"},
    {ASSEMBLER_ASCIZ, "ASSEMBLER_ASCIZ"},

    {RELOCATION_EMU32_O_LO12, "RELOCATION_EMU32_O_LO12"}, {RELOCATION_EMU32_ADRP_HI20, "RELOCATION_EMU32_ADRP_HI20"},
    {RELOCATION_EMU32_MOV_LO19, "RELOCATION_EMU32_MOV_LO19"}, {RELOCATION_EMU32_MOV_HI13, "RELOCATION_EMU32_MOV_HI13"},

    {REGISTER_X0, "REGISTER_X0"}, {REGISTER_X1, "REGISTER_X1"},
    {REGISTER_X2, "REGISTER_X2"}, {REGISTER_X3, "REGISTER_X3"},
    {REGISTER_X4, "REGISTER_X4"}, {REGISTER_X5, "REGISTER_X5"},
    {REGISTER_X6, "REGISTER_X6"}, {REGISTER_X7, "REGISTER_X7"},
    {REGISTER_X8, "REGISTER_X8"}, {REGISTER_X9, "REGISTER_X9"},
    {REGISTER_X10, "REGISTER_X10"}, {REGISTER_X11, "REGISTER_X11"},
    {REGISTER_X12, "REGISTER_X12"}, {REGISTER_X13, "REGISTER_X13"},
    {REGISTER_X14, "REGISTER_X14"}, {REGISTER_X15, "REGISTER_X15"},
    {REGISTER_X16, "REGISTER_X16"}, {REGISTER_X17, "REGISTER_X17"},
    {REGISTER_X18, "REGISTER_X18"}, {REGISTER_X19, "REGISTER_X19"},
    {REGISTER_X20, "REGISTER_X20"}, {REGISTER_X21, "REGISTER_X21"},
    {REGISTER_X22, "REGISTER_X22"}, {REGISTER_X23, "REGISTER_X23"},
    {REGISTER_X24, "REGISTER_X24"}, {REGISTER_X25, "REGISTER_X25"},
    {REGISTER_X26, "REGISTER_X26"}, {REGISTER_X27, "REGISTER_X27"},
    {REGISTER_X28, "REGISTER_X28"}, {REGISTER_X29, "REGISTER_X29"},
    {REGISTER_XZR, "REGISTER_XZR"}, {REGISTER_SP, "REGISTER_SP"},

    {INSTRUCTION_HLT, "INSTRUCTION_HLT"},
    {INSTRUCTION_ADD, "INSTRUCTION_ADD"}, {INSTRUCTION_SUB,"INSTRUCTION_SUB"}, {INSTRUCTION_RSB, "INSTRUCTION_RSB"},
    {INSTRUCTION_ADC, "INSTRUCTION_ADC"}, {INSTRUCTION_SBC, "INSTRUCTION_SBC"}, {INSTRUCTION_RSC, "INSTRUCTION_RSC"},
    {INSTRUCTION_MUL, "INSTRUCTION_MUL"}, {INSTRUCTION_UMULL, "INSTRUCTION_UMULL"}, {INSTRUCTION_SMULL, "INSTRUCTION_SMULL"},
    {INSTRUCTION_VABS, "INSTRUCTION_VABS"}, {INSTRUCTION_VNEG, "INSTRUCTION_VNEG"}, {INSTRUCTION_VSQRT, "INSTRUCTION_VSQRT"},
    {INSTRUCTION_VADD, "INSTRUCTION_VADD"}, {INSTRUCTION_VSUB, "INSTRUCTION_VSUB"}, {INSTRUCTION_VDIV, "INSTRUCTION_VDIV"},
    {INSTRUCTION_VMUL, "INSTRUCTION_VMUL"}, {INSTRUCTION_VCMP, "INSTRUCTION_VCMP"}, {INSTRUCTION_VSEL, "INSTRUCTION_VSEL"},
    {INSTRUCTION_VCINT, "INSTRUCTION_VCINT"}, {INSTRUCTION_VCFLO, "INSTRUCTION_VCFLO"},
    {INSTRUCTION_VMOV, "INSTRUCTION_VMOV"},
    {INSTRUCTION_AND, "INSTRUCTION_AND"}, {INSTRUCTION_ORR, "INSTRUCTION_ORR"}, {INSTRUCTION_EOR, "INSTRUCTION_EOR"}, {INSTRUCTION_BIC, "INSTRUCTION_BIC"},
    {INSTRUCTION_LSL, "INSTRUCTION_LSL"}, {INSTRUCTION_LSR, "INSTRUCTION_LSR"}, {INSTRUCTION_ASR, "INSTRUCTION_ASR"}, {INSTRUCTION_ROR, "INSTRUCTION_ROR"},
    {INSTRUCTION_CMP, "INSTRUCTION_CMP"}, {INSTRUCTION_CMN, "INSTRUCTION_CMN"}, {INSTRUCTION_TST, "INSTRUCTION_TST"}, {INSTRUCTION_TEQ, "INSTRUCTION_TEQ"},
    {INSTRUCTION_MOV, "INSTRUCTION_MOV"}, {INSTRUCTION_MVN, "INSTRUCTION_MVN"},
    {INSTRUCTION_LDR, "INSTRUCTION_LDR"}, {INSTRUCTION_STR, "INSTRUCTION_STR"}, {INSTRUCTION_SWP, "INSTRUCTION_SWP"},
    {INSTRUCTION_LDRB, "INSTRUCTION_LDRB"}, {INSTRUCTION_STRB, "INSTRUCTION_STRB"}, {INSTRUCTION_SWPB, "INSTRUCTION_SWPB"},
    {INSTRUCTION_LDRH, "INSTRUCTION_LDRH"}, {INSTRUCTION_STRH, "INSTRUCTION_STRH"}, {INSTRUCTION_SWPH, "INSTRUCTION_SWPH"},
    {INSTRUCTION_B, "INSTRUCTION_B"}, {INSTRUCTION_BL, "INSTRUCTION_B"}, {INSTRUCTION_BX, "INSTRUCTION_BX"}, {INSTRUCTION_BLX, "INSTRUCTION_BLX"}, {INSTRUCTION_SWI, "INSTRUCTION_SWI"},
    {INSTRUCTION_ADRP, "INSTRUCTION_ADRP"},

    {INSTRUCTION_RET, "INSTRUCTION_RET"},

    {CONDITION_EQ, "CONDITION_EQ"}, {CONDITION_NE, "CONDITION_NE"},
    {CONDITION_CS, "CONDITION_CS"}, {CONDITION_HS, "CONDITION_HS"},
    {CONDITION_CC, "CONDITION_CC"}, {CONDITION_LO, "CONDITION_LO"},
    {CONDITION_MI, "CONDITION_MI"}, {CONDITION_PL, "CONDITION_PL"},
    {CONDITION_VS, "CONDITION_VS"}, {CONDITION_VC, "CONDITION_VC"},
    {CONDITION_HI, "CONDITION_HI"}, {CONDITION_LS, "CONDITION_LS"},
    {CONDITION_GE, "CONDITION_GE"}, {CONDITION_LT, "CONDITION_LT"}, {CONDITION_GT, "CONDITION_GT"}, {CONDITION_LE, "CONDITION_LE"},
    {CONDITION_AL, "CONDITION_AL"}, {CONDITION_NV, "CONDITION_NV"},

    {NUMBER_SIGN, "NUMBER_SIGN"},
    {LITERAL_FLOAT_32, "LITERAL_FLOAT_32"},
    {LITERAL_NUMBER_BINARY, "LITERAL_NUMBER_BINARY"}, {LITERAL_NUMBER_OCTAL, "LITERAL_NUMBER_OCTAL"},
    {LITERAL_NUMBER_DECIMAL, "LITERAL_NUMBER_DECIMAL"}, {LITERAL_NUMBER_HEXADECIMAL, "LITERAL_NUMBER_HEXADECIMAL"},
    {LITERAL_CHAR, "LITERAL_CHAR"}, {LITERAL_STRING, "LITERAL_STRING"},
    {SYMBOL, "SYMBOL"},
    {COLON, "COLON"}, {COMMA, "COMMA"}, {PERIOD, "PERIOD"}, {SEMICOLON, "SEMICOLON"},
    {OPEN_PARANTHESIS, "OPEN_PARANTHESIS"}, {CLOSE_PARANTHESIS, "CLOSE_PARANTHESIS"},
    {OPEN_BRACKET, "OPEN_BRACKET"}, {CLOSE_BRACKET, "CLOSE_BRACKET"},
    {OPEN_BRACE, "OPEN_BRACE"}, {CLOSE_BRACE, "CLOSE_BRACE"},

    {OPERATOR_ADDITION, "OPERATOR_ADDITION"}, {OPERATOR_SUBTRACTION, "OPERATOR_SUBTRACTION"},
    {OPERATOR_MULTIPLICATION, "OPERATOR_MULTIPLICATION"}, {OPERATOR_DIVISION, "OPERATOR_DIVISION"},
    {OPERATOR_MODULUS, "OPERATOR_MODULUS"}, {OPERATOR_BITWISE_LEFT_SHIFT, "OPERATOR_BITWISE_LEFT_SHIFT"},
    {OPERATOR_BITWISE_RIGHT_SHIFT, "OPERATOR_BITWISE_RIGHT_SHIFT"}, {OPERATOR_BITWISE_XOR, "OPERATOR_BITWISE_XOR"},
    {OPERATOR_BITWISE_AND, "OPERATOR_BITWISE_AND"}, {OPERATOR_BITWISE_OR, "OPERATOR_BITWISE_OR"},
    {OPERATOR_BITWISE_COMPLEMENT, "OPERATOR_BITWISE_COMPLEMENT"}, {OPERATOR_LOGICAL_NOT, "OPERATOR_LOGICAL_NOT"},
    {OPERATOR_LOGICAL_EQUAL, "OPERATOR_LOGICAL_EQUAL"}, {OPERATOR_LOGICAL_NOT_EQUAL, "OPERATOR_LOGICAL_NOT_EQUAL"},
    {OPERATOR_LOGICAL_LESS_THAN, "OPERATOR_LOGICAL_LESS_THAN"}, {OPERATOR_LOGICAL_GREATER_THAN, "OPERATOR_LOGICAL_GREATER_THAN"},
    {OPERATOR_LOGICAL_LESS_THAN_OR_EQUAL, "OPERATOR_LOGICAL_LESS_THAN_OR_EQUAL"}, {OPERATOR_LOGICAL_GREATER_THAN_OR_EQUAL, "OPERATOR_LOGICAL_GREATER_THAN_OR_EQUAL"},
    {OPERATOR_LOGICAL_OR, "OPERATOR_LOGICAL_OR"}, {OPERATOR_LOGICAL_AND, "OPERATOR_LOGICAL_AND"},
};

const std::set<Tokenizer::Type> Tokenizer::WHITESPACES =
{
    WHITESPACE_SPACE, WHITESPACE_TAB, WHITESPACE_NEWLINE
};

const std::set<Tokenizer::Type> Tokenizer::COMMENTS =
{
    COMMENT_SINGLE_LINE, COMMENT_MULTI_LINE
};

const std::set<Tokenizer::Type> Tokenizer::PREPROCESSOR_DIRECTIVES =
{
    PREPROCESSOR_INCLUDE, PREPROCESSOR_MACRO, PREPROCESSOR_MACRET, PREPROCESSOR_MACEND, PREPROCESSOR_INVOKE,
    PREPROCESSOR_DEFINE, PREPROCESSOR_UNDEF, PREPROCESSOR_IFDEF, PREPROCESSOR_IFNDEF, PREPROCESSOR_IFEQU,
    PREPROCESSOR_IFNEQU, PREPROCESSOR_IFLESS, PREPROCESSOR_IFMORE, PREPROCESSOR_ELSE,
    PREPROCESSOR_ELSEDEF, PREPROCESSOR_ELSENDEF,
    PREPROCESSOR_ELSEEQU, PREPROCESSOR_ELSENEQU, PREPROCESSOR_ELSELESS, PREPROCESSOR_ELSEMORE,
    PREPROCESSOR_ENDIF
};

const std::set<Tokenizer::Type> Tokenizer::ASSEMBLER_DIRECTIVES =
{
    ASSEMBLER_GLOBAL, ASSEMBLER_EXTERN,
    ASSEMBLER_ORG,
    ASSEMBLER_SCOPE, ASSEMBLER_SCEND,
    ASSEMBLER_ADVANCE, ASSEMBLER_FILL,
    ASSEMBLER_ALIGN,
    ASSEMBLER_SECTION,
    ASSEMBLER_BSS,
    ASSEMBLER_DATA,
    ASSEMBLER_TEXT,
    ASSEMBLER_STOP,
    ASSEMBLER_BYTE, ASSEMBLER_DBYTE, ASSEMBLER_WORD, ASSEMBLER_DWORD,
    ASSEMBLER_SBYTE, ASSEMBLER_SDBYTE, ASSEMBLER_SWORD, ASSEMBLER_SDWORD,
    ASSEMBLER_CHAR, ASSEMBLER_ASCII, ASSEMBLER_ASCIZ,
};

const std::set<Tokenizer::Type> Tokenizer::RELOCATIONS =
{
    RELOCATION_EMU32_O_LO12, RELOCATION_EMU32_ADRP_HI20,
    RELOCATION_EMU32_MOV_LO19, RELOCATION_EMU32_MOV_HI13,
};

const std::set<Tokenizer::Type> Tokenizer::REGISTERS =
{
    REGISTER_X0, REGISTER_X1,
    REGISTER_X2, REGISTER_X3,
    REGISTER_X4, REGISTER_X5,
    REGISTER_X6, REGISTER_X7,
    REGISTER_X8, REGISTER_X9,
    REGISTER_X10, REGISTER_X11,
    REGISTER_X12, REGISTER_X13,
    REGISTER_X14, REGISTER_X15,
    REGISTER_X16, REGISTER_X17,
    REGISTER_X18, REGISTER_X19,
    REGISTER_X20, REGISTER_X21,
    REGISTER_X22, REGISTER_X23,
    REGISTER_X24, REGISTER_X25,
    REGISTER_X26, REGISTER_X27,
    REGISTER_X28, REGISTER_X29,
    REGISTER_XZR, REGISTER_SP,
};

const std::set<Tokenizer::Type> Tokenizer::INSTRUCTIONS =
{
    INSTRUCTION_HLT,
    INSTRUCTION_ADD, INSTRUCTION_SUB, INSTRUCTION_RSB,
    INSTRUCTION_ADC, INSTRUCTION_SBC, INSTRUCTION_RSC,
    INSTRUCTION_MUL, INSTRUCTION_UMULL, INSTRUCTION_SMULL,
    INSTRUCTION_VABS, INSTRUCTION_VNEG, INSTRUCTION_VSQRT,
    INSTRUCTION_VADD, INSTRUCTION_VSUB, INSTRUCTION_VDIV,
    INSTRUCTION_VMUL, INSTRUCTION_VCMP, INSTRUCTION_VSEL,
    INSTRUCTION_VCINT, INSTRUCTION_VCFLO,
    INSTRUCTION_VMOV,
    INSTRUCTION_AND, INSTRUCTION_ORR, INSTRUCTION_EOR, INSTRUCTION_BIC,
    INSTRUCTION_LSL, INSTRUCTION_LSR, INSTRUCTION_ASR, INSTRUCTION_ROR,
    INSTRUCTION_CMP, INSTRUCTION_CMN, INSTRUCTION_TST, INSTRUCTION_TEQ,
    INSTRUCTION_MOV, INSTRUCTION_MVN,
    INSTRUCTION_LDR, INSTRUCTION_STR, INSTRUCTION_SWP,
    INSTRUCTION_LDRB, INSTRUCTION_STRB, INSTRUCTION_SWPB,
    INSTRUCTION_LDRH, INSTRUCTION_STRH, INSTRUCTION_SWPH,
    INSTRUCTION_B, INSTRUCTION_BL, INSTRUCTION_BX, INSTRUCTION_BLX, INSTRUCTION_SWI,
    INSTRUCTION_ADRP,

    INSTRUCTION_RET,
};

const std::set<Tokenizer::Type> Tokenizer::CONDITIONS =
{
    CONDITION_EQ, CONDITION_NE,
    CONDITION_CS, CONDITION_HS,
    CONDITION_CC, CONDITION_LO,
    CONDITION_MI, CONDITION_PL,
    CONDITION_VS, CONDITION_VC,
    CONDITION_HI, CONDITION_LS,
    CONDITION_GE, CONDITION_LT, CONDITION_GT, CONDITION_LE,
    CONDITION_AL, CONDITION_NV,
};

const std::set<Tokenizer::Type> Tokenizer::LITERAL_NUMBERS =
{
    LITERAL_FLOAT_32,
    LITERAL_NUMBER_BINARY, LITERAL_NUMBER_OCTAL, LITERAL_NUMBER_DECIMAL, LITERAL_NUMBER_HEXADECIMAL
};

const std::set<Tokenizer::Type> Tokenizer::LITERAL_VALUES =
{
    LITERAL_FLOAT_32,
    LITERAL_NUMBER_BINARY, LITERAL_NUMBER_OCTAL, LITERAL_NUMBER_DECIMAL, LITERAL_NUMBER_HEXADECIMAL,
    LITERAL_CHAR, LITERAL_STRING
};

const std::set<Tokenizer::Type> Tokenizer::OPERATORS =
{
    OPERATOR_ADDITION, OPERATOR_SUBTRACTION, OPERATOR_MULTIPLICATION, OPERATOR_DIVISION, OPERATOR_MODULUS,
    OPERATOR_BITWISE_LEFT_SHIFT, OPERATOR_BITWISE_RIGHT_SHIFT, OPERATOR_BITWISE_XOR, OPERATOR_BITWISE_AND,
    OPERATOR_BITWISE_OR, OPERATOR_BITWISE_COMPLEMENT, OPERATOR_LOGICAL_NOT, OPERATOR_LOGICAL_EQUAL,
    OPERATOR_LOGICAL_NOT_EQUAL, OPERATOR_LOGICAL_LESS_THAN, OPERATOR_LOGICAL_GREATER_THAN,
    OPERATOR_LOGICAL_LESS_THAN_OR_EQUAL, OPERATOR_LOGICAL_GREATER_THAN_OR_EQUAL, OPERATOR_LOGICAL_OR,
    OPERATOR_LOGICAL_AND
};

const std::vector<std::pair<std::string, Tokenizer::Type>> Tokenizer::TOKEN_SPEC =
{
    {"^ ", WHITESPACE_SPACE}, {"^\\t", WHITESPACE_TAB}, {"^\\n", WHITESPACE_NEWLINE},
    {"^[^\\S\\n\\t]+", WHITESPACE},
    {"^;\\*[\\s\\S]*?\\*;", COMMENT_MULTI_LINE}, {"^;.*", COMMENT_SINGLE_LINE},
    {"^:lo12:\\b", RELOCATION_EMU32_O_LO12}, {"^:hi20:\\b", RELOCATION_EMU32_ADRP_HI20},
    {"^:lo19:\\b", RELOCATION_EMU32_MOV_LO19}, {"^:hi13:\\b", RELOCATION_EMU32_MOV_HI13},

    {"^[a-zA-Z_][a-zA-Z0-9_]*:", LABEL},
    {"^\\\\", BACK_SLASH}, {"^/", FORWARD_SLASH},
    {"^\\{", OPEN_BRACE}, {"^\\}", CLOSE_BRACE},
    {"^\\[", OPEN_BRACKET}, {"^\\]", CLOSE_BRACKET},
    {"^\\(", OPEN_PARANTHESIS},{"^\\)", CLOSE_PARANTHESIS},

    {"^#", NUMBER_SIGN},
    {"^[0-9]*\\.[0-9]+", LITERAL_FLOAT_32},
    {"^%[0-1]+", LITERAL_NUMBER_BINARY},
    {"^@[0-7]+", LITERAL_NUMBER_OCTAL},
    {"^[0-9]+", LITERAL_NUMBER_DECIMAL},
    {"^\\$[0-9a-fA-F]+", LITERAL_NUMBER_HEXADECIMAL},

    {"^\'.\'", LITERAL_CHAR}, {"^\"([^\"\\\\]|\\\\.)*\"", LITERAL_STRING},
    {"^[a-zA-Z_][a-zA-Z0-9_]*", SYMBOL},

    {"^,", COMMA}, {"^:", COLON}, {"^\\.", PERIOD}, {"^;", SEMICOLON},

    {"^\\+", OPERATOR_ADDITION}, {"^\\-", OPERATOR_SUBTRACTION},
    {"^\\*", OPERATOR_MULTIPLICATION}, {"^\\/", OPERATOR_DIVISION},
    {"^\\%", OPERATOR_MODULUS},
    {"^\\|\\|", OPERATOR_LOGICAL_OR}, {"^\\&\\&", OPERATOR_LOGICAL_AND},
    {"^\\<\\<", OPERATOR_BITWISE_LEFT_SHIFT}, {"^\\>\\>", OPERATOR_BITWISE_RIGHT_SHIFT},
    {"^\\^", OPERATOR_BITWISE_XOR}, {"^\\&", OPERATOR_BITWISE_AND},
    {"^\\|", OPERATOR_BITWISE_OR}, {"^~", OPERATOR_BITWISE_COMPLEMENT},
    {"^==", OPERATOR_LOGICAL_EQUAL}, {"^!=", OPERATOR_LOGICAL_NOT_EQUAL},
    {"^!", OPERATOR_LOGICAL_NOT},
    {"^\\<=", OPERATOR_LOGICAL_LESS_THAN_OR_EQUAL}, {"^\\>=", OPERATOR_LOGICAL_GREATER_THAN_OR_EQUAL},
    {"^\\<", OPERATOR_LOGICAL_LESS_THAN}, {"^\\>", OPERATOR_LOGICAL_GREATER_THAN},
};