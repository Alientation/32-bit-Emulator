#pragma once

#include "util/file.h"

#include <set>
#include <string>
#include <unordered_map>
#include <vector>

/// @brief              Tokenizer.
///
/// @todo               TODO: create a macro that will generate the token spec
class Tokenizer
{
  public:
    /// @brief          State of tokenizer.
    struct State
    {
        /// @brief      Token pointer. Index of the next token to be consumed.
        ///             If all tokens have been consumed, points one past the last token.
        size_t toki = 0;

        /// @brief      Indent information for auto indentation.
        int prev_indent = 0;

        /// @brief      Indent information for auto indentation.
        int cur_indent = 0;

        /// @brief      Indent information for auto indentation.
        int target_indent = 0;
    };

    /// @brief          Type of token.
    enum Type
    {
        UNKNOWN,

        LABEL,
        TEXT,
        WHITESPACE_SPACE,
        WHITESPACE_TAB,
        WHITESPACE_NEWLINE,
        WHITESPACE,
        COMMENT_SINGLE_LINE,
        COMMENT_MULTI_LINE,
        BACK_SLASH,
        FORWARD_SLASH,

        // Preprocessor directives.
        PREPROCESSOR_INCLUDE,
        PREPROCESSOR_MACRO,
        PREPROCESSOR_MACRET,
        PREPROCESSOR_MACEND,
        PREPROCESSOR_INVOKE,
        PREPROCESSOR_DEFINE,
        PREPROCESSOR_UNDEF,
        PREPROCESSOR_IFDEF,
        PREPROCESSOR_IFNDEF,
        PREPROCESSOR_IFEQU,
        PREPROCESSOR_IFNEQU,
        PREPROCESSOR_IFLESS,
        PREPROCESSOR_IFMORE,
        PREPROCESSOR_ELSE,
        PREPROCESSOR_ELSEDEF,
        PREPROCESSOR_ELSENDEF,
        PREPROCESSOR_ELSEEQU,
        PREPROCESSOR_ELSENEQU,
        PREPROCESSOR_ELSELESS,
        PREPROCESSOR_ELSEMORE,
        PREPROCESSOR_ENDIF,

        // Assembler directives.
        ASSEMBLER_GLOBAL,
        ASSEMBLER_EXTERN,
        ASSEMBLER_ORG,
        ASSEMBLER_SCOPE,
        ASSEMBLER_SCEND,
        ASSEMBLER_ADVANCE,
        ASSEMBLER_FILL,
        ASSEMBLER_ALIGN,
        ASSEMBLER_SECTION,
        ASSEMBLER_BSS,
        ASSEMBLER_DATA,
        ASSEMBLER_TEXT,
        ASSEMBLER_STOP,
        ASSEMBLER_BYTE,
        ASSEMBLER_DBYTE,
        ASSEMBLER_WORD,
        ASSEMBLER_DWORD,
        ASSEMBLER_SBYTE,
        ASSEMBLER_SDBYTE,
        ASSEMBLER_SWORD,
        ASSEMBLER_SDWORD,
        ASSEMBLER_CHAR,
        ASSEMBLER_ASCII,
        ASSEMBLER_ASCIZ,

        // Relocation specifiers.
        RELOCATION_EMU32_O_LO12,
        RELOCATION_EMU32_ADRP_HI20,
        RELOCATION_EMU32_MOV_LO19,
        RELOCATION_EMU32_MOV_HI13,

        // Registers.
        REGISTER_X0,
        REGISTER_X1,
        REGISTER_X2,
        REGISTER_X3,
        REGISTER_X4,
        REGISTER_X5,
        REGISTER_X6,
        REGISTER_X7,
        REGISTER_X8,
        REGISTER_X9,
        REGISTER_X10,
        REGISTER_X11,
        REGISTER_X12,
        REGISTER_X13,
        REGISTER_X14,
        REGISTER_X15,
        REGISTER_X16,
        REGISTER_X17,
        REGISTER_X18,
        REGISTER_X19,
        REGISTER_X20,
        REGISTER_X21,
        REGISTER_X22,
        REGISTER_X23,
        REGISTER_X24,
        REGISTER_X25,
        REGISTER_X26,
        REGISTER_X27,
        REGISTER_X28,
        REGISTER_X29,
        REGISTER_SP,
        REGISTER_XZR,

        // Instructions.
        INSTRUCTION_HLT,
        INSTRUCTION_NOP,
        INSTRUCTION_ADD,
        INSTRUCTION_SUB,
        INSTRUCTION_RSB,
        INSTRUCTION_ADC,
        INSTRUCTION_SBC,
        INSTRUCTION_RSC,
        INSTRUCTION_MUL,
        INSTRUCTION_UMULL,
        INSTRUCTION_SMULL,
        INSTRUCTION_VABS,
        INSTRUCTION_VNEG,
        INSTRUCTION_VSQRT,
        INSTRUCTION_VADD,
        INSTRUCTION_VSUB,
        INSTRUCTION_VDIV,
        INSTRUCTION_VMUL,
        INSTRUCTION_VCMP,
        INSTRUCTION_VSEL,
        INSTRUCTION_VCINT,
        INSTRUCTION_VCFLO,
        INSTRUCTION_VMOV,
        INSTRUCTION_AND,
        INSTRUCTION_ORR,
        INSTRUCTION_EOR,
        INSTRUCTION_BIC,
        INSTRUCTION_LSL,
        INSTRUCTION_LSR,
        INSTRUCTION_ASR,
        INSTRUCTION_ROR,
        INSTRUCTION_CMP,
        INSTRUCTION_CMN,
        INSTRUCTION_TST,
        INSTRUCTION_TEQ,
        INSTRUCTION_MOV,
        INSTRUCTION_MVN,
        INSTRUCTION_LDR,
        INSTRUCTION_STR,
        INSTRUCTION_SWP,
        INSTRUCTION_LDRB,
        INSTRUCTION_STRB,
        INSTRUCTION_SWPB,
        INSTRUCTION_LDRH,
        INSTRUCTION_STRH,
        INSTRUCTION_SWPH,
        INSTRUCTION_MSR,
        INSTRUCTION_MRS,
        INSTRUCTION_TLBI,
        INSTRUCTION_LDADD,
        INSTRUCTION_LDADDB,
        INSTRUCTION_LDADDH,
        INSTRUCTION_LDCLR,
        INSTRUCTION_LDCLRB,
        INSTRUCTION_LDCLRH,
        INSTRUCTION_LDSET,
        INSTRUCTION_LDSETB,
        INSTRUCTION_LDSETH,

        INSTRUCTION_B,
        INSTRUCTION_BL,
        INSTRUCTION_BX,
        INSTRUCTION_BLX,
        INSTRUCTION_SWI,
        INSTRUCTION_ADRP,

        // Pseduo instruction. Replaced with a BX instruction.
        INSTRUCTION_RET,

        // Conditions for branch instructions.
        CONDITION_EQ,
        CONDITION_NE,
        CONDITION_CS,
        CONDITION_HS,
        CONDITION_CC,
        CONDITION_LO,
        CONDITION_MI,
        CONDITION_PL,
        CONDITION_VS,
        CONDITION_VC,
        CONDITION_HI,
        CONDITION_LS,
        CONDITION_GE,
        CONDITION_LT,
        CONDITION_GT,
        CONDITION_LE,
        CONDITION_AL,
        CONDITION_NV,

        // Expressions.
        LITERAL_FLOAT_32,
        LITERAL_NUMBER_BINARY,
        LITERAL_NUMBER_OCTAL,
        LITERAL_NUMBER_DECIMAL,
        LITERAL_NUMBER_HEXADECIMAL,
        LITERAL_CHAR,
        LITERAL_STRING,

        SYMBOL,
        COLON,
        COMMA,
        PERIOD,
        SEMICOLON,
        OPEN_PARANTHESIS,
        CLOSE_PARANTHESIS,
        OPEN_BRACKET,
        CLOSE_BRACKET,
        OPEN_BRACE,
        CLOSE_BRACE,

        OPERATOR_ADDITION,
        OPERATOR_SUBTRACTION,
        OPERATOR_MULTIPLICATION,
        OPERATOR_DIVISION,
        OPERATOR_MODULUS,
        OPERATOR_BITWISE_LEFT_SHIFT,
        OPERATOR_BITWISE_RIGHT_SHIFT,
        OPERATOR_BITWISE_XOR,
        OPERATOR_BITWISE_AND,
        OPERATOR_BITWISE_OR,
        OPERATOR_BITWISE_COMPLEMENT,
        OPERATOR_LOGICAL_NOT,
        OPERATOR_LOGICAL_EQUAL,
        OPERATOR_LOGICAL_NOT_EQUAL,
        OPERATOR_LOGICAL_LESS_THAN,
        OPERATOR_LOGICAL_GREATER_THAN,
        OPERATOR_LOGICAL_LESS_THAN_OR_EQUAL,
        OPERATOR_LOGICAL_GREATER_THAN_OR_EQUAL,
        OPERATOR_LOGICAL_OR,
        OPERATOR_LOGICAL_AND,
    };

    /// @brief              Maps a token type to stringified name.
    static const std::unordered_map<Type, std::string> TYPE_TO_NAME_MAP;

    /// @brief              Set of whitespace token types.
    static const std::set<Type> WHITESPACES;

    /// @brief              Set of comment token types.
    static const std::set<Type> COMMENTS;

    /// @brief              Set of preprocessor directive token types.
    static const std::set<Type> PREPROCESSOR_DIRECTIVES;

    /// @brief              Set of variable token types.
    static const std::set<Type> VARIABLE_TYPES;

    /// @brief              Set of assembler directive token types.
    static const std::set<Type> ASSEMBLER_DIRECTIVES;

    /// @brief              Set of relocation specifier token types.
    static const std::set<Type> RELOCATIONS;

    /// @brief              Set of register token types.
    static const std::set<Type> REGISTERS;

    /// @brief              Set of instruction token types.
    static const std::set<Type> INSTRUCTIONS;

    /// @brief              Set of branch conditions token types.
    static const std::set<Type> CONDITIONS;

    /// @brief              Set of literal number token types.
    static const std::set<Type> LITERAL_NUMBERS;

    /// @brief              Set of literal value token types.
    static const std::set<Type> LITERAL_VALUES;

    /// @brief              Set of operator token types.
    static const std::set<Type> OPERATORS;

    /// @brief              Specification of regex matching rules for token types.
    static const std::vector<std::pair<std::string, Type>> TOKEN_SPEC;

    ///
    /// @brief              Token representation.
    ///
    /// Base source code character set
    ///
    /// a-z A-Z 0-9 _ { } [ ] ( ) < > % : ; . , ? * + - / ^ & | ~ ! = " ' \ # @ $
    ///
    struct Token
    {
        /// @brief          Type of token.
        Type type;

        /// @brief          String value of token. Exactly as it was in the source code.
        std::string value;

        /// @brief          Line number of token. Zero indexed.
        int line;

        /// @brief          Tokenized id. For tracking original tokens compared to new inserted ones.
        int tokenize_id;

        /// @brief          TODO:
        bool skip = false;

        ///
        /// @brief              TODO:
        ///
        /// @param type
        /// @param value
        /// @param line
        /// @param tokenize_id
        ///
        Token (Type type, std::string value, int line = -1, int tokenize_id = -1) noexcept;

        ///
        /// @brief          TODO:
        ///
        /// @param tok
        ///
        Token (const Token &tok) noexcept;

        ///
        /// @brief          TODO:
        ///
        /// @param tok
        ///
        Token (Token &&tok) noexcept;

        ///
        /// @brief          TODO:
        ///
        /// @param tok
        ///
        /// @return
        Token &operator= (const Token &tok) noexcept;

        ///
        /// @brief          TODO:
        ///
        /// @param tok
        ///
        /// @return
        Token &operator= (Token &&tok) noexcept;

        ///
        /// @brief          TODO:
        ///
        /// @return
        ///
        std::string to_string () const;

        ///
        /// @brief          TODO:
        ///
        /// @param types
        ///
        /// @return
        ///
        bool is (const std::set<Type> &types) const;

        ///
        /// @brief          TODO:
        ///
        /// @return
        ///
        int nlines () const;
    };

    /// @brief              Options controlling how to tokenize.
    struct Options
    {
        /// @brief          Whether to keep comments in the tokens list.
        bool keep_comments = false;

        /// @brief          Whether to keep whitespaces in the tokens list.
        bool keep_whitespace = true;
    };

    ///
    /// @brief              TODO:
    ///
    Tokenizer ();

    ///
    /// @brief              TODO:
    ///
    /// @param src
    /// @param option
    ///
    Tokenizer (File src,
               Options option = (Options) {.keep_comments = true, .keep_whitespace = true});

    ///
    /// @brief              TODO:
    ///
    /// @param src
    /// @param option
    ///
    Tokenizer (std::string src,
               Options option = (Options) {.keep_comments = true, .keep_whitespace = true});

    ///
    /// @brief              Get token pointer.
    ///
    /// @return             Token pointer.
    ///
    size_t get_toki () const;

    ///
    /// @brief              Update token pointer.
    ///
    /// @param toki         New token pointer.
    ///
    void set_toki (size_t toki);

    ///
    /// @brief              Get state of tokenizer.
    ///
    struct State get_state () const;

    ///
    /// @brief              Set state of tokenizer.
    ///
    /// @param state        New state.
    ///
    void set_state (struct State state);

    ///
    /// @brief              TODO:
    ///
    /// @return
    ///
    bool fix_indent ();

    ///
    /// @brief              Get line number of current token.
    ///
    /// @return             Line number.
    ///
    int get_linei () const;

    ///
    /// @brief              Get line number of token.
    ///
    /// @param toki         Token index.
    ///
    /// @return             Line number.
    ///
    int get_linei (size_t toki) const;

    ///
    /// @brief              Get stringified line containing all original tokens at the line.
    ///
    /// @param linei        Line number, zero indexed.
    ///
    /// @return             String representation of the line.
    ///
    std::string get_line (int linei) const;

    ///
    /// @brief              Get next token.
    ///
    /// @return             Next token.
    ///
    Tokenizer::Token &get_token ();

    ///
    /// @brief              Get tokens.
    ///
    /// @return             Tokens list.
    ///
    const std::vector<Token> &get_tokens ();

    ///
    /// @brief              Insert tokens into the token list.
    ///
    /// @param tokens       Tokens to insert.
    /// @param loc          Position to insert at. Token at the location will be moved.
    ///
    void insert_tokens (const std::vector<Token> &tokens, size_t loc);

    ///
    /// @brief              Remove tokens from specified range.
    ///
    /// @param start        Inclusive start position.
    /// @param end          Exclusive end position.
    ///
    void remove_tokens (size_t start, size_t end);

    ///
    /// @brief              Remove all tokens that match the token types.
    ///
    /// @param tok_types    Token types to remove.
    ///
    void filter_all (const std::set<Tokenizer::Type> &tok_types);

    ///
    /// @brief              Advances past the next token.
    ///
    void skip_next ();

    ///
    /// @brief              Advances past tokens that matches the given regex.
    ///
    /// @param regex        matches tokens to skip.
    ///
    void skip_next_regex (const std::string &regex);

    ///
    /// @brief              Advances past tokens that match the given types.
    ///
    /// @param tok_types    the types to match.
    ///
    void skip_next (const std::set<Tokenizer::Type> &tok_types);

    ///
    /// @brief              Advances past tokens that match the given type.
    ///
    /// @param tok_type     the type to match.
    ///
    void skip_next (Tokenizer::Type tok_type);

    ///
    /// @brief              Expects the current token to exist.
    ///
    /// @param error_msg    Message to throw if the token does not exist.
    ///
    void expect_next (const std::string &error_msg);

    ///
    /// @brief              Expects current token to exist and be of a specific type.
    ///
    /// @param tok_types    Expected token types.
    /// @param error_msg    Message to throw if the token does not exist or is not the correct type.
    ///
    void expect_next (const std::set<Tokenizer::Type> &tok_types, const std::string &error_msg);

    ///
    /// @brief              Expects current token to exist and be of a specific type.
    ///
    /// @param tok_type     Expected token type.
    /// @param error_msg    Message to throw if the token does not exist or is not the correct type.
    ///
    void expect_next (Tokenizer::Type tok_type, const std::string &error_msg);

    ///
    /// @brief              Returns whether the current token matches the given types.
    ///
    /// @param tok_types    The types to match.
    /// @param error_msg    Message to throw if the token does not exist.
    ///
    /// @return             If the next token matches the given types.
    ///
    bool is_next (const std::set<Tokenizer::Type> &tok_types,
                  const std::string &error_msg = "Tokenizer::is_token() - Unexpected end of file.");

    ///
    /// @brief              Returns whether the current token matches the given type.
    ///
    /// @param tok_type     The type to match.
    /// @param error_msg    Message to throw if the token does not exist.
    ///
    /// @return             If the next token matches the given type.
    ///
    bool is_next (Tokenizer::Type tok_type,
                  const std::string &error_msg = "Tokenizer::is_token() - Unexpected end of file.");

    ///
    ///                     Returns whether there is another token.
    ///
    /// @return             If there is a next token.
    ///
    bool has_next ();

    ///
    /// @brief              Consumes the current token and advances to the next token.
    ///
    /// @param error_msg    Message to throw if the token does not exist.
    ///
    /// @return             The consumed token.
    ///
    Tokenizer::Token &
    consume (const std::string &error_msg = "Tokenizer::consume() - Unexpected end of file.");

    ///
    /// @brief                  Consumes the current token and checks it matches the given types.
    ///
    /// @param expected_types   Expected types of the token.
    /// @param error_msg        Message to throw if the token does not have the expected type.
    ///
    /// @return                 Consumed token.
    ///
    Tokenizer::Token &
    consume (const std::set<Tokenizer::Type> &expected_types,
             const std::string &error_msg = "Tokenizer::consume() - Unexpected token.");

    ///
    /// @brief                  Consumes the current token and checks it matches the given type.
    ///
    /// @param expected_type    Expected type of the token.
    /// @param error_msg        Message to throw if the token does not have the expected type.
    ///
    /// @return                 Consumed token.
    ///
    Tokenizer::Token &
    consume (Tokenizer::Type expected_type,
             const std::string &error_msg = "Tokenizer::consume() - Unexpected token.");

    ///
    /// @brief              Tokenizes a file.
    ///
    /// @param src_file     File to tokenize.
    /// @param option       Options for the tokenizer.
    ///
    /// @return             Vector of the tokens.
    static std::vector<Token> tokenize (File src_file,
                                        Options option = (Options) {.keep_comments = true,
                                                                    .keep_whitespace = true});

    ///
    /// @brief              Tokenizes a string representing the source code.
    /// @param source_code  Source code as a string.
    /// @param option       Options for the tokenizer.
    /// @return             Vector of the tokens.
    static std::vector<Token>
    tokenize (std::string source_code,
              Options option = (Options{.keep_comments = true, .keep_whitespace = true}));

  private:
    /// @brief              Tokens list.
    std::vector<Tokenizer::Token> m_tokens;

    /// @brief              Unique tokenizer identification.
    ///                     Used for line number information given new inserted tokens.
    /// @todo               TODO: There should be a better way to handle this. Instead
    ///                     have each token contain a boolean representing whether it is
    ///                     an original or inserted token.
    int m_tokenize_id = -1;

    /// @brief              State of the tokenizer.
    struct State m_state;

    ///
    /// @brief              TODO:
    ///
    void verify ();

    ///
    /// @brief              TODO:
    ///
    void move_past_skipped_tokens ();

    ///
    /// @brief              TODO:
    ///
    void handle_token ();
};