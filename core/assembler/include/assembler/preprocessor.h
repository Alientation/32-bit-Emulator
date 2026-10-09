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

    /// Constructs a preprocessor object with the given file.
    ///
    /// @param input_file the file to preprocess.
    /// @param output_file_path the path to the output file, default is the input file path with
    ///     the .bi extension.
    /// @param options include directories etc. of the build.
    explicit Preprocessor(const File &input_file, const std::string &output_file_path = "",
                          PreprocessorOptions options = {});
    ~Preprocessor();

    /// Preprocesses the input file and writes the .bi file if the options ask for it.
    ///
    /// @return the output file
    File preprocess();

    /// @return how far preprocess() got
    State get_state();

    /// The tokens preprocess() produced. Call once, after preprocess().
    ///
    /// @return the preprocessed tokens
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

        /// #else was seen, no further branch is allowed.
        bool seen_else;

        basm::SourceLocation loc;
    };

    static constexpr std::size_t kMaxFrames = 256;

    PreprocessorOptions m_options;

    // the .basm or .binc file being preprocessed
    File m_input_file;

    // the output file of the processed file, usually a .bi file
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
    /// @param at the token the error is about
    /// @param message what is wrong
    [[noreturn]] void fail(const basm::Token &at, const std::string &message);

    /// @return whether tokens are being kept, as opposed to dropped by an #if* that is not taken
    bool active() const;

    /// @param name the name of a symbol
    /// @param num_params the number of parameters
    /// @return whether `#define` made a symbol with the name and that many parameters
    bool is_symbol_def(const std::string &name, std::size_t num_params) const;

    /// Pushes a new source of tokens on the stack of frames.
    ///
    /// @param tokens the tokens, which the frame owns
    /// @return the frame
    Frame &push_frame(Tokens tokens);

    /// Pushes a source file (`#include`, or the input file).
    ///
    /// @param path the path of the file
    /// @param include_site the `#include` token for the error messages, null for the input file
    void push_file(const std::string &path, const basm::Token *include_site);

    /// Reads the frames until none is left.
    void run();

    /// Pops a frame whose tokens ran out, after checking that it left nothing open.
    ///
    /// @param frame the frame
    void finish_frame(Frame &frame);

    /// Appends a token to the output, as text and as a token.
    ///
    /// @param token the token to keep
    void emit(const basm::Token &token);

    /// Parses `( a, b, c )` with the cursor on the open parenthesis. Commas inside nested
    /// brackets do not split.
    ///
    /// @param cursor the cursor of the line
    /// @param args receives the tokens of each argument
    /// @return false (cursor position unspecified) if the closing parenthesis is not on the same
    ///     line
    static bool parse_call_args(basm::TokenCursor &cursor, std::vector<Tokens> &args);

    /// Parses `( a, b )` of a definition header into parameter names.
    ///
    /// @param line the cursor of the line, on the open parenthesis
    /// @return the names
    std::vector<std::string> parse_params(basm::TokenCursor &line);

    /// Copy of `body` with every use of a parameter replaced by the matching argument. The
    /// tokens that come from `body` are marked as produced by expansion `expansion`, the ones
    /// from the arguments keep the location they were written at.
    ///
    /// @param body the tokens of the definition
    /// @param params the names of the parameters
    /// @param args the tokens of each argument
    /// @param expansion the number of the expansion, for the error messages
    /// @return the tokens with the arguments put in
    Tokens substitute(const Tokens &body, const std::vector<std::string> &params,
                      const std::vector<Tokens> &args, U32 expansion) const;

    /// Replaces a use of a defined symbol with its value.
    ///
    /// @param frame the frame the token was read from
    /// @param token the token that may be a symbol
    /// @return false, consuming nothing, if the token is to be emitted as it is
    bool expand_symbol(Frame &frame, const basm::Token &token);

    /// Handles the directive at the cursor of the frame (`#include`, `#define`, ...).
    ///
    /// @param frame the frame that is being read
    void handle_directive(Frame &frame);

    /// Inserts the file contents into the current file.
    ///
    /// USAGE: #include "filepath"|<"filepath">
    ///
    /// "filepath": looks for files relative to the directory of the including file.
    /// <"filepath">: looks for files in the include directories (-I).
    void _include(basm::TokenCursor &line);

    /// Defines a macro with n parameters.
    ///
    /// USAGE: #macro [name]([param1, param2,..., paramn])
    ///
    /// The body is everything up to #macend. It cannot contain another #macro.
    void _macro(Frame &frame, basm::TokenCursor &line);

    /// Stops expanding the macro and assigns the value of the expression to the output symbol of
    /// the #invoke.
    ///
    /// USAGE: #macret [?expression]
    void _macret(Frame &frame, basm::TokenCursor &line);

    /// Expands the macro with the given arguments inside a `.scope` / `.scend` pair.
    ///
    /// USAGE: #invoke [name]([arg1, arg2,..., argn]) [?output symbol]
    void _invoke(basm::TokenCursor &line);

    /// Associates the symbol with a value. Uses of the symbol are replaced by the value. If the
    /// value is not specified it is empty. Parameters must start directly after the symbol, so
    /// `#define F(x) x` takes a parameter and `#define F (x)` is the value `(x)`.
    ///
    /// USAGE: #define [symbol][?(param1, ..., paramn)] [?value]
    void _define(basm::TokenCursor &line);

    /// Defines the symbols of PreprocessorOptions::defines.
    void define_from_options();

    /// Undefines a symbol defined by #define. Works if the symbol was never defined.
    ///
    /// USAGE: #undef [symbol] [?number of parameters]
    ///
    /// Without a number of parameters every definition of the symbol is removed.
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
    void _conditional(Frame &frame, basm::TokenCursor &line);

    /// Evaluates the condition of an #if* / #else* directive.
    ///
    /// @param directive the directive token
    /// @param line the cursor of the line, after the directive
    /// @return whether the condition holds
    bool evaluate_condition(const basm::Token &directive, basm::TokenCursor &line);
};
