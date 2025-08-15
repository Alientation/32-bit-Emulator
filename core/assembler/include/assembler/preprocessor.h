#pragma once

#include "assembler/build.h"
#include "assembler/tokenizer.h"
#include "util/file.h"

#include <string>
#include <stack>
#include <map>
#include <functional>

/**
 * Plan
 *
 * Improve errors
 *  - supply more information like the problematic line, a specific error and maybe a way to resolve the error?
 *  - throw an exception for errors
 *  - unit test these exceptions
 */
class Preprocessor
{
    public:
        enum State
        {
            UNPROCESSED, PROCESSING, PROCESSED_SUCCESS, PROCESSED_ERROR
        };

        class BadTokenException : public std::exception
        {
            private:
                std::string msg;
                const Tokenizer::Token &tok;

            public:
                BadTokenException(const std::string &msg, const Tokenizer::Token &tok);
                const char *what() const noexcept override;
        };

        /**
         * Constructs a preprocessor object with the given file.
         *
         * @param process the build process object.
         * @param file the file to preprocess.
         * @param outputFilePath the path to the output file, default is the inputfile path with .bi extension.
         */
        Preprocessor(Process *process, const File &input_file, const std::string &output_file_path = "");
        ~Preprocessor();

        File preprocess();
        State get_state();

    private:
        struct Argument
        {
            std::string name;
            Tokenizer::Type type;

            Argument(std::string name, Tokenizer::Type type);
            Argument(std::string name);
        };

        struct Macro
        {
            std::string name;
            std::vector<Argument> args;
            Tokenizer::Type return_type;

            std::vector<Tokenizer::Token> definition;

            Macro(std::string name);

            std::string to_string();
            std::string header();
        };

        struct Symbol
        {
            std::string name;
            std::vector<std::string> parameters;
            std::vector<Tokenizer::Token> value;

            Symbol(std::string name, std::vector<std::string> parameters, std::vector<Tokenizer::Token> value);
        };


        Process *m_process;

        // the .basm or .binc file being preprocessed
        File m_input_file;

        Tokenizer tokenizer;

        // the output file of the processed file, usually a .bi file
        File m_output_file;
        State m_state;

        // the current processing macro stack with the output symbol and macro
        std::stack<std::pair<std::string, Macro>> m_macro_stack;

        std::map<std::string, std::map<int, Symbol>> m_def_symbols;
        std::map<std::string, Macro> m_macros;


        /**
         * Returns the macros that match the given macro name and arguments list.
         *
         * @param macro_name the name of the macro.
         * @param arguments the arguments passed to the macro.
         *
         * TODO:
         * possibly in the future should consider filtering for macros that have the same order of argument types.
         * This would require us knowing the types of symbols and expressions in the preprocessor state
         * which is not ideal
         *
         * @return the macros with the given name and number of arguments.
         */
        std::vector<Macro> macros_with_header(const std::string &macro_name,
                                              const std::vector<std::vector<Tokenizer::Token>> &args);


        /**
         * Inserts the file contents into the current file.
         *
         * USAGE: #include "filepath"|<"filepath">
         *
         * TODO:
         * make angled brackets <...> capture everything inside as a token to
         * not have to surround inside with a string
         *
         * "filepath": looks for files located in the current directory.
         * <filepath>: prioritizes files located in the include directory, if not found, looks in the
         * current directory.
         */
        void _include();

        /**
         * Defines a macro symbol with n arguments and optionally a return type.
         *
         * USAGE: #macro [symbol]([arg1 ?: TYPE, arg2 ?: TYPE,..., argn ?: TYPE]) ?: TYPE
         *
         * If a return type is specified and the macro definition does not return a value an error is thrown.
         * There cannot be a macro definition within this macro definition.
         * Note that the macro symbol is separate from label symbols and will not be present after preprocessing.
         */
        void _macro();

        /**
         * Stops processing the macro and returns the value of the expression.
         *
         * USAGE: #macret [?expression]
         *
         * If the macro does not have a return type the macret must return nothing.
         * If the macro has a return type the macret must return a value of that type
         */
        void _macret();

        /**
         * Closes a macro definition.
         *
         * USAGE: #macend
         *
         * If a macro is not closed an error is thrown.
         */
        void _macend();

        /**
         * Invokes the macro with the given arguments.
         *
         * USAGE: #invoke [symbol]([arg1, arg2,..., argn]) [?symbol]
         *
         * If provided an output symbol, the symbol will be associated with the return value of the macro.
         * If the macro does not return a value but an output symbol is provided, an error is thrown.
         */
        void _invoke();

        /**
         * Associates the symbol with a value
         *
         * USAGE: #define [symbol] [?value]
         *
         * Replaces all instances of symbol with the value.
         * If value is not specified, the default is empty.
         */
        void _define();

        /**
         * Handles a condition block
         * Depending on whether the condition was met, keep/discard blocks
         *
         * @param cond_met
         */
        void cond_block(bool cond_met);

        /**
         * Returns whether the symbol is a defined symbol with the same number of parameters
         *
         * @param symbol_name
         * @param num_params
         */
        bool is_symbol_def(const std::string &symbol_name, int num_params);

        /**
         * Begins a conditional block.
         * Determines whether to include the following text block depending on whether the symbol is defined.
         *
         * USAGE: #ifdef [symbol], #ifndef [symbol] (top conditional blocks)
         * USAGE: #elsedef [symbol], #elsendef [symbol] (lower conditional blocks)
         *
         * The top conditional block must be closed by a lower conditional block or an #endif.
         * The lower conditional block must be closed by an #endif.
         */
        void _cond_on_def();

        /**
         * Begins a conditional block.
         * Determines whether to include the following text block based on the symbol's value
         * lexicographically ordering to a value.
         *
         * USAGE: #ifequ [symbol] [value], #ifnequ [symbol] [value], #ifless [symbol] [value], #ifmore [symbol] [value]
         * USAGE: #elseequ [symbol] [value], #elsenequ [symbol] [value], #elseless [symbol] [value], #elsemore [symbol] [value]
         *
         * The top conditional block must be closed by a lower conditional block or an #endif.
         * The lower conditional block must be closed by an #endif.
         */
        void _cond_on_value();

        /**
         * Closure of a top or lower conditional block, only includes the following text if all previous
         * conditional blocks were not included.
         *
         * USAGE: #else
         *
         * Must be preceded by a top or inner conditional block.
         * Must not be proceeded by an inner conditional block or closure.
         */
        void _else();

        /**
         * Closes a #ifdef, #ifndef, #else, #elsedef, or #elsendef.
         *
         * USAGE: #endif
         *
         * Must be preceded by a #ifdef, #ifndef, #else, #elsedef, or #elsendef.
         */
        void _endif();


        /**
         * Undefines a symbol defined by #define.
         *
         * USAGE: #undefine [symbol]
         *
         * This will still work if the symbol was never defined previously.
         */
        void _undefine();

        typedef void (Preprocessor::*PreprocessorFunction)();
        std::map<Tokenizer::Type,PreprocessorFunction> preprocessors = {
            {Tokenizer::PREPROCESSOR_INCLUDE, &Preprocessor::_include},
            {Tokenizer::PREPROCESSOR_MACRO, &Preprocessor::_macro},
            {Tokenizer::PREPROCESSOR_MACRET, &Preprocessor::_macret},
            {Tokenizer::PREPROCESSOR_MACEND, &Preprocessor::_macend},
            {Tokenizer::PREPROCESSOR_INVOKE, &Preprocessor::_invoke},
            {Tokenizer::PREPROCESSOR_DEFINE, &Preprocessor::_define},

            {Tokenizer::PREPROCESSOR_IFDEF, &Preprocessor::_cond_on_def},
            {Tokenizer::PREPROCESSOR_IFNDEF, &Preprocessor::_cond_on_def},

            {Tokenizer::PREPROCESSOR_IFEQU, &Preprocessor::_cond_on_value},
            {Tokenizer::PREPROCESSOR_IFNEQU, &Preprocessor::_cond_on_value},
            {Tokenizer::PREPROCESSOR_IFLESS, &Preprocessor::_cond_on_value},
            {Tokenizer::PREPROCESSOR_IFMORE, &Preprocessor::_cond_on_value},

            {Tokenizer::PREPROCESSOR_ELSE, &Preprocessor::_else},

            {Tokenizer::PREPROCESSOR_ELSEDEF, &Preprocessor::_cond_on_def},
            {Tokenizer::PREPROCESSOR_ELSENDEF, &Preprocessor::_cond_on_def},

            {Tokenizer::PREPROCESSOR_ELSEEQU, &Preprocessor::_cond_on_value},
            {Tokenizer::PREPROCESSOR_ELSENEQU, &Preprocessor::_cond_on_value},
            {Tokenizer::PREPROCESSOR_ELSELESS, &Preprocessor::_cond_on_value},
            {Tokenizer::PREPROCESSOR_ELSEMORE, &Preprocessor::_cond_on_value},

            {Tokenizer::PREPROCESSOR_ENDIF, &Preprocessor::_endif},
            {Tokenizer::PREPROCESSOR_UNDEF, &Preprocessor::_undefine}
        };
};
