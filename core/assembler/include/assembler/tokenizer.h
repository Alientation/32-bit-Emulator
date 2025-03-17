#pragma once
#ifndef TOKENIZER_H
#define TOKENIZER_H

#include "util/file.h"

#include <vector>
#include <string>
#include <unordered_map>
#include <set>


// TODO create a macro that will generate the token spec
class Tokenizer
{
    public:
        struct State
        {
            size_t toki = 0;
            int prev_indent = 0;
            int cur_indent = 0;
            int target_indent = 0;
        };

        enum Type
        {
            UNKNOWN,

            LABEL,
            TEXT, WHITESPACE_SPACE, WHITESPACE_TAB, WHITESPACE_NEWLINE, WHITESPACE,
            COMMENT_SINGLE_LINE, COMMENT_MULTI_LINE, BACK_SLASH, FORWARD_SLASH,

            // PREPROCESSOR DIRECTIVES
            PREPROCESSOR_INCLUDE,
            PREPROCESSOR_MACRO, PREPROCESSOR_MACRET, PREPROCESSOR_MACEND, PREPROCESSOR_INVOKE,
            PREPROCESSOR_DEFINE, PREPROCESSOR_UNDEF,
            PREPROCESSOR_IFDEF, PREPROCESSOR_IFNDEF,
            PREPROCESSOR_IFEQU, PREPROCESSOR_IFNEQU, PREPROCESSOR_IFLESS, PREPROCESSOR_IFMORE,
            PREPROCESSOR_ELSE, PREPROCESSOR_ELSEDEF, PREPROCESSOR_ELSENDEF,
            PREPROCESSOR_ELSEEQU, PREPROCESSOR_ELSENEQU, PREPROCESSOR_ELSELESS, PREPROCESSOR_ELSEMORE,
            PREPROCESSOR_ENDIF,

            // ASSEMBLER DIRECTIVES
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


            // relocation
            RELOCATION_EMU32_O_LO12, RELOCATION_EMU32_ADRP_HI20,
            RELOCATION_EMU32_MOV_LO19, RELOCATION_EMU32_MOV_HI13,

            // registers
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
            REGISTER_SP, REGISTER_XZR,

            // instructions
            INSTRUCTION_HLT, INSTRUCTION_NOP,
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
            INSTRUCTION_MSR, INSTRUCTION_MRS, INSTRUCTION_TLBI,
            INSTRUCTION_LDADD, INSTRUCTION_LDADDB, INSTRUCTION_LDADDH,
            INSTRUCTION_LDCLR, INSTRUCTION_LDCLRB, INSTRUCTION_LDCLRH,
            INSTRUCTION_LDSET, INSTRUCTION_LDSETB, INSTRUCTION_LDSETH,

            INSTRUCTION_B, INSTRUCTION_BL, INSTRUCTION_BX, INSTRUCTION_BLX, INSTRUCTION_SWI,
            INSTRUCTION_ADRP,

            // PSEUDO INSTRUCTION
            INSTRUCTION_RET,

            // conditions for branch instructions
            CONDITION_EQ, CONDITION_NE,
            CONDITION_CS, CONDITION_HS,
            CONDITION_CC, CONDITION_LO,
            CONDITION_MI, CONDITION_PL,
            CONDITION_VS, CONDITION_VC,
            CONDITION_HI, CONDITION_LS,
            CONDITION_GE, CONDITION_LT, CONDITION_GT, CONDITION_LE,
            CONDITION_AL, CONDITION_NV,

            // expressions
            LITERAL_FLOAT_32,
            LITERAL_NUMBER_BINARY, LITERAL_NUMBER_OCTAL, LITERAL_NUMBER_DECIMAL, LITERAL_NUMBER_HEXADECIMAL,
            LITERAL_CHAR, LITERAL_STRING,

            SYMBOL,
            COLON, COMMA, PERIOD, SEMICOLON,
            OPEN_PARANTHESIS, CLOSE_PARANTHESIS, OPEN_BRACKET, CLOSE_BRACKET, OPEN_BRACE, CLOSE_BRACE,

            OPERATOR_ADDITION, OPERATOR_SUBTRACTION, OPERATOR_MULTIPLICATION, OPERATOR_DIVISION, OPERATOR_MODULUS,
            OPERATOR_BITWISE_LEFT_SHIFT, OPERATOR_BITWISE_RIGHT_SHIFT,
            OPERATOR_BITWISE_XOR, OPERATOR_BITWISE_AND, OPERATOR_BITWISE_OR, OPERATOR_BITWISE_COMPLEMENT,
            OPERATOR_LOGICAL_NOT, OPERATOR_LOGICAL_EQUAL, OPERATOR_LOGICAL_NOT_EQUAL,
            OPERATOR_LOGICAL_LESS_THAN, OPERATOR_LOGICAL_GREATER_THAN,
            OPERATOR_LOGICAL_LESS_THAN_OR_EQUAL, OPERATOR_LOGICAL_GREATER_THAN_OR_EQUAL,
            OPERATOR_LOGICAL_OR, OPERATOR_LOGICAL_AND,
        };

        static const std::unordered_map<Type, std::string> TYPE_TO_NAME_MAP;

        static const std::set<Type> WHITESPACES;
        static const std::set<Type> COMMENTS;
        static const std::set<Type> PREPROCESSOR_DIRECTIVES;
        static const std::set<Type> VARIABLE_TYPES;
        static const std::set<Type> ASSEMBLER_DIRECTIVES;
        static const std::set<Type> RELOCATIONS;
        static const std::set<Type> REGISTERS;
        static const std::set<Type> INSTRUCTIONS;
        static const std::set<Type> CONDITIONS;
        static const std::set<Type> LITERAL_NUMBERS;
        static const std::set<Type> LITERAL_VALUES;
        static const std::set<Type> OPERATORS;
        static const std::vector<std::pair<std::string, Type>> TOKEN_SPEC;

        /**
         * Base source code character set
         *
         * a-z A-Z 0-9 _ { } [ ] ( ) < > % : ; . , ? * + - / ^ & | ~ ! = " ' \ # @ $
         */
        struct Token
        {
            Type type;
            std::string value;
            int line;
            int tokenize_id;
            bool skip = false;

            Token(Type type, std::string value, int line = -1, int tokenize_id = -1) noexcept;
            Token(const Token &tok) noexcept;
            Token(Token &&tok) noexcept;
            Token &operator=(const Token &tok) noexcept;
            Token &operator=(Token &&tok) noexcept;

            std::string to_string();
            bool is(const std::set<Type> &types);
            int nlines();
        };

        Tokenizer(File src, bool keep_comments = true);
        Tokenizer(std::string src, bool keep_comments = true);

        size_t get_toki();
        struct State get_state();
        void set_state(struct State);
        bool fix_indent();
        int get_linei(size_t toki);
        std::string get_line(int linei);

        Tokenizer::Token& get_token();
        const std::vector<Token>& get_tokens();

        void insert_tokens(const std::vector<Token>& tokens, size_t loc);
        void remove_tokens(size_t start, size_t end);

        void filter_all(const std::set<Tokenizer::Type>& tok_types);

        void skip_next();

        /**
         * Skips tokens that match the given regex.
         *
         * @param regex matches tokens to skip.
         */
        void skip_next_regex(const std::string& regex);

        /**
         * Skips tokens that match the given types.
         *
         * @param tok_types the types to match.
         */
        void skip_next(const std::set<Tokenizer::Type>& tok_types);

        /**
         * Expects the current token to exist.
         *
         * @param error_msg the error message to throw if the token does not exist.
         */
        bool expect_next(const std::string& error_msg);

        /**
         * Expects the current token to exist and be of a specific type.
         *
         * @param tok_types the expected token types
         * @param error_msg the error message to throw if the token does not exist.
         */
        bool expect_next(const std::set<Tokenizer::Type>& tok_types,
                const std::string& error_msg);

        /**
         * Returns whether the current token matches the given types.
         *
         * @param tok_types the types to match.
         *
         * @return true if the current token matches the given types.
         */
        bool is_next(const std::set<Tokenizer::Type>& tok_types,
                const std::string& error_msg = "Tokenizer::is_token() - Unexpected end of file.");

        /**
         * Returns whether the current token index is within the bounds of the tokens list.
         *
         * @return true if the token index is within the bounds of the tokens list.
         */
        bool has_next();

        /**
         * Consumes the current token.
         *
         * @param error_msg the error message to throw if the token does not exist.
         *
         * @returns the value of the consumed token.
         */
        Tokenizer::Token& consume(const std::string& error_msg =
                "Tokenizer::consume() - Unexpected end of file.");

        /**
         * Consumes the current token and checks it matches the given types.
         *
         * @param expected_types the expected types of the token.
         * @param error_msg the error message to throw if the token does not have the expected type.
         *
         * @returns the value of the consumed token.
         */
        Tokenizer::Token& consume(const std::set<Tokenizer::Type>& expected_types,
                const std::string& error_msg = "Tokenizer::consume() - Unexpected token.");

        static std::vector<Token> tokenize(File srcFile, bool keep_comments = true);
        static std::vector<Token> tokenize(std::string source_code, bool keep_comments = true);

    private:
        std::vector<Tokenizer::Token> m_tokens;
        int m_tokenize_id = -1;
        struct State m_state;

        void verify();
        void move_past_skipped_tokens();
        void handle_token();
};

#endif /* TOKENIZER_H */