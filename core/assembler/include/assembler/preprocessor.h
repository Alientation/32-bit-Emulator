#pragma once

#include "assembler/options.h"
#include "assembler/tokenizer.h"
#include "util/file.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

/// basm preprocessor. Runs on the tokens of basm::lex. Produces the preprocessed tokens
/// (take_result) and also writes them as source text (.bi) to the output file.
///
/// Input is a stack of Frames. The main file is the bottom frame. #include pushes the included
/// file and a macro or symbol expansion pushes its expanded tokens. A frame is popped when its
/// tokens run out. Nothing is ever inserted into or removed from a token list, expansions are
/// simply new frames, and conditional blocks are tracked with a stack of Cond so that the tokens
/// of a block that is not taken are dropped as they are read.
///
/// Errors are reported with file, line and column of the offending token and terminate through
/// AEMU_FATAL.
class Preprocessor
{
  public:
    /// How far preprocess() got.
    enum State
    {
        UNPROCESSED,
        PROCESSING,
        PROCESSED_SUCCESS,
        PROCESSED_ERROR
    };

    /// Constructs a preprocessor object with the given file. The output file is created now if the
    /// options ask for it to be written. The extension of the input has to be .basm or .binc.
    ///
    /// @param[in] input_file The file to preprocess.
    /// @param[in] output_file_path The path to the output file, default is the input file path with
    ///     the .bi extension.
    /// @param[in] options Include directories etc. of the build.
    explicit Preprocessor(const File &input_file, const std::string &output_file_path = "",
                          PreprocessorOptions options = {});

    /// Preprocesses the input file and writes the .bi file if the options ask for it.
    ///
    /// @return The output file.
    File preprocess();

    /// @return How far preprocess() got.
    State get_state() const;

    /// The tokens preprocess() produced. Call once, after preprocess().
    ///
    /// @return The preprocessed tokens.
    basm::PreprocessedSource take_result();

  private:
    using Tokens = std::vector<basm::Token>;

    /// `#define NAME[(params)] value`. One symbol can have a definition per parameter count.
    struct Symbol
    {
        std::vector<std::string> params;
        Tokens value;
    };

    /// `#macro name(params)` ... `#macend`. Keyed by "name/<parameter count>".
    struct Macro
    {
        std::string name;
        std::vector<std::string> params;
        Tokens body;
    };

    /// One source of tokens. Expansions own their tokens, so a frame never refers to tokens
    /// that can move.
    struct Frame
    {
        Tokens tokens;
        basm::TokenCursor cursor;

        /// Source file frames only: the directory #include "..." is resolved against.
        bool is_file = false;
        std::string dir;

        /// Size of the conditional stack when the frame was pushed. A conditional block cannot
        /// begin in one frame and end in another.
        std::size_t cond_base = 0;

        /// Set for a macro expansion. `output` is the symbol #macret assigns, `tail` is the index
        /// of the `.scend` token that #macret jumps to.
        const Macro *macro = nullptr;
        std::string output;
        std::size_t tail = 0;
    };

    /// One open #if* block.
    struct Cond
    {
        /// Whether the enclosing block was being taken. If not, nothing in this one is.
        bool parent_active;

        /// Whether some branch of the chain was already taken.
        bool taken;

        /// Whether tokens are being kept right now.
        bool active;

        /// #else was seen, so no further branch is allowed.
        bool seen_else;

        basm::SourceLocation loc;
    };

    /// How many frames can be open at once, so that a file that includes itself or a symbol that
    /// expands to itself ends with an error.
    static constexpr std::size_t kMaxFrames = 256;

    PreprocessorOptions m_options;

    // The .basm or .binc file being preprocessed.
    File m_input_file;

    // The output file of the processed file, usually a .bi file.
    File m_output_file;
    State m_state;

    std::shared_ptr<basm::SourceManager> m_sources;
    std::vector<std::unique_ptr<Frame>> m_frames;
    std::vector<Cond> m_conds;

    std::map<std::string, std::map<std::size_t, Symbol>> m_symbols;
    std::map<std::string, Macro> m_macros;

    std::string m_out;
    char m_last_char = '\n';
    Tokens m_out_tokens;

    /// Logs the error at the token (file, line, column, source line) and terminates.
    ///
    /// @param[in] at The token the error is about.
    /// @param[in] message What is wrong.
    [[noreturn]] void fail(const basm::Token &at, const std::string &message);

    /// @return Whether tokens are being kept, as opposed to dropped by an #if* that is not taken.
    bool active() const;

    /// @param[in] name The name of a symbol.
    /// @param[in] num_params The number of parameters.
    /// @return Whether `#define` made a symbol with the name and that many parameters.
    bool is_symbol_def(const std::string &name, std::size_t num_params) const;

    /// Pushes a new source of tokens on the stack of frames.
    ///
    /// @param[in] tokens The tokens, which the frame owns.
    /// @return The frame.
    Frame &push_frame(Tokens tokens);

    /// Pushes a source file (`#include`, or the input file).
    ///
    /// @param[in] path The path of the file.
    /// @param[in] include_site The `#include` token for the error messages, null for the input
    ///     file.
    void push_file(const std::string &path, const basm::Token *include_site);

    /// Reads the frames until none is left.
    void run();

    /// Pops a frame whose tokens ran out, after checking that it left nothing open.
    ///
    /// @param[in] frame The frame.
    void finish_frame(const Frame &frame);

    /// Appends a token to the output, as text and as a token.
    ///
    /// @param[in] token The token to keep.
    void emit(const basm::Token &token);

    /// Parses `( a, b, c )` with the cursor on the open parenthesis. Commas inside nested
    /// brackets do not split. `()` has no arguments, and `(,)` has two empty ones.
    ///
    /// @param[in,out] args Receives the tokens of each argument.
    /// @param[in,out] cursor The cursor of the line.
    /// @return `false` (cursor position unspecified) if the closing parenthesis is not on the same
    ///     line.
    static bool parse_call_args(std::vector<Tokens> &args, basm::TokenCursor &cursor);

    /// Parses `( a, b )` of a definition header into parameter names. Fatal if a name is not a
    /// symbol or is repeated.
    ///
    /// @param[in,out] line The cursor of the line, on the open parenthesis.
    /// @return The names.
    std::vector<std::string> parse_params(basm::TokenCursor &line);

    /// Copy of `body` with every use of a parameter replaced by the matching argument. The
    /// tokens that come from `body` are marked as produced by expansion `expansion`, the ones
    /// from the arguments keep the location they were written at.
    ///
    /// @param[in] body The tokens of the definition.
    /// @param[in] params The names of the parameters.
    /// @param[in] args The tokens of each argument.
    /// @param[in] expansion The number of the expansion, for the error messages.
    /// @return The tokens with the arguments put in.
    Tokens substitute(const Tokens &body, const std::vector<std::string> &params,
                      const std::vector<Tokens> &args, U32 expansion) const;

    /// Replaces a use of a defined symbol with its value.
    ///
    /// @param[in,out] frame The frame the token was read from.
    /// @param[in] token The token that may be a symbol.
    /// @return `false`, consuming nothing, if the token is to be emitted as it is.
    bool expand_symbol(Frame &frame, const basm::Token &token);

    /// Handles the directive at the cursor of the frame (`#include`, `#define`, ...).
    ///
    /// @param[in,out] frame The frame that is being read.
    void handle_directive(Frame &frame);

    /// Inserts the file contents into the current file.
    ///
    /// USAGE: #include "filepath"|<"filepath">
    ///
    /// "filepath": looks for files relative to the directory of the including file.
    /// <"filepath">: looks for files in the include directories (-I).
    ///
    /// @param[in,out] line The cursor of the line, after the directive.
    void _include(basm::TokenCursor &line);

    /// Defines a macro with n parameters.
    ///
    /// USAGE: #macro [name]([param1, param2,..., paramn])
    ///
    /// The body is everything up to #macend. It cannot contain another #macro. The same name can be
    /// defined with another number of parameters.
    ///
    /// @param[in,out] frame The frame that is being read.
    /// @param[in,out] line The cursor of the line, after the directive.
    void _macro(Frame &frame, basm::TokenCursor &line);

    /// Stops expanding the macro and assigns the value of the expression to the output symbol of
    /// the #invoke.
    ///
    /// USAGE: #macret [?expression]
    ///
    /// @param[in,out] frame The frame that is being read.
    /// @param[in,out] line The cursor of the line, after the directive.
    void _macret(Frame &frame, basm::TokenCursor &line);

    /// Expands the macro with the given arguments inside a `.scope` / `.scend` pair.
    ///
    /// USAGE: #invoke [name]([arg1, arg2,..., argn]) [?output symbol]
    ///
    /// @param[in,out] line The cursor of the line, after the directive.
    void _invoke(basm::TokenCursor &line);

    /// Associates the symbol with a value. Uses of the symbol are replaced by the value. If the
    /// value is not specified it is empty. Parameters must start directly after the symbol, so
    /// `#define F(x) x` takes a parameter and `#define F (x)` is the value `(x)`.
    ///
    /// USAGE: #define [symbol][?(param1, ..., paramn)] [?value]
    ///
    /// @param[in,out] line The cursor of the line, after the directive.
    void _define(basm::TokenCursor &line);

    /// Defines the symbols of PreprocessorOptions::defines.
    void define_from_options();

    /// Undefines a symbol defined by #define. Works if the symbol was never defined.
    ///
    /// USAGE: #undef [symbol] [?number of parameters]
    ///
    /// Without a number of parameters every definition of the symbol is removed.
    ///
    /// @param[in,out] line The cursor of the line, after the directive.
    void _undef(basm::TokenCursor &line);

    /// Conditional blocks.
    ///
    /// USAGE: #ifdef [symbol], #ifndef [symbol]
    /// #ifequ [symbol] [value], #ifnequ, #ifless, #ifmore (compares as numbers when both sides
    /// are numbers, `9` is less than `10`, and as text otherwise. The left side can also be a
    /// single token that is not a symbol, such as the number a macro parameter was replaced
    /// with)
    /// #elsedef, #elsendef, #elseequ, #elsenequ, #elseless, #elsemore, #else
    /// #endif
    ///
    /// @param[in,out] frame The frame that is being read.
    /// @param[in,out] line The cursor of the line, after the directive.
    void _conditional(Frame &frame, basm::TokenCursor &line);

    /// Evaluates the condition of an #if* / #else* directive.
    ///
    /// @param[in] directive The directive token.
    /// @param[in,out] line The cursor of the line, after the directive.
    /// @return Whether the condition holds.
    bool evaluate_condition(const basm::Token &directive, basm::TokenCursor &line);
};
