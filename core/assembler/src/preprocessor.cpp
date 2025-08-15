#include "assembler/preprocessor.h"
#include "emulator32bit/emulator32bit.h"
#include "util/logger.h"
#include "util/string_util.h"
#include "util/vector_util.h"

#include <regex>
#include <fstream>
#include <filesystem>

#define UNUSED(x) (void)(x)

Preprocessor::Argument::Argument(std::string name, Tokenizer::Type type) :
    name(name),
    type(type)
{

}
Preprocessor::Argument::Argument(std::string name) :
    name(name),
    type(Tokenizer::UNKNOWN)
{

}

Preprocessor::Macro::Macro(std::string name) :
    name(name),
    return_type(Tokenizer::UNKNOWN)
{

}

std::string Preprocessor::Macro::to_string()
{
    std::string to_string = header() + "\n";
    for (size_t i = 0; i < args.size(); i++)
    {
        to_string += "[" + std::to_string(i) + "]: " + args[i].name + ": " +
                Tokenizer::TYPE_TO_NAME_MAP.at(args[i].type);
    }
    to_string += "-> " + Tokenizer::TYPE_TO_NAME_MAP.at(return_type) + "\n{\n";

    for (size_t i = 0; i < definition.size(); i++)
    {
        to_string += definition[i].value;
    }

    return to_string + "\n}";
}

std::string Preprocessor::Macro::header()
{
    std::string header = name + "@(";

    for (size_t i = 0; i < args.size(); i++)
    {
        header += Tokenizer::TYPE_TO_NAME_MAP.at(args[i].type);
        if (i < args.size() - 1)
        {
            header += ",";
        }
    }

    return header + "):" + Tokenizer::TYPE_TO_NAME_MAP.at(return_type);
}

Preprocessor::Symbol::Symbol(std::string name, std::vector<std::string> params,
                             std::vector<Tokenizer::Token> value) :
    name(name),
    parameters(params),
    value(value)
{

}

Preprocessor::Preprocessor(Process* process, const File& input_file, const std::string& output_file_path) :
    m_process(process),
    m_input_file(input_file),
    tokenizer(input_file)
{
    // default output file path if not supplied in the constructor
    if (output_file_path.empty())
    {
        m_output_file = File(m_input_file.get_name(), PROCESSED_EXTENSION, m_input_file.get_dir(), true);
    }
    else
    {
        m_output_file = File(output_file_path, true);
    }

    EXPECT_TRUE_SS(m_process->valid_src_file(input_file), std::stringstream()
            << "Preprocessor::Preprocessor() - Invalid source file: " << input_file.get_extension());

    m_state = State::UNPROCESSED;
}

Preprocessor::~Preprocessor()
{

}

Preprocessor::BadTokenException::BadTokenException(const std::string &msg, const Tokenizer::Token &tok) :
    msg(msg),
    tok(tok)
{

}

const char *Preprocessor::BadTokenException::what() const noexcept
{
    return "";
}

File Preprocessor::preprocess()
{
    DEBUG("Preprocessor::preprocess() - Preprocessing file: %s", m_input_file.get_name().c_str());

    EXPECT_TRUE_SS(m_state == State::UNPROCESSED, std::stringstream()
            << "Preprocessor::preprocess() - Preprocessor is not in the UNPROCESSED state");
    m_state = State::PROCESSING;

    // clearing intermediate output file
    std::ofstream ofs;
    ofs.open(m_output_file.get_path(), std::ofstream::out | std::ofstream::trunc);
    ofs.close();

    // create writer for intermediate output file
    FileWriter writer = FileWriter(m_output_file);

    // remove all comments before processing
    tokenizer.filter_all(Tokenizer::COMMENTS);

    // parse the tokens
    while (tokenizer.has_next())
    {
        Tokenizer::Token& token = tokenizer.get_token();

        // if token is valid preprocessor, call the preprocessor function
        if (preprocessors.find(token.type) != preprocessors.end())
        {
            (this->*preprocessors[token.type])();
            continue;
        }

        // check if this is not a defined symbol
        if (token.type != Tokenizer::SYMBOL || m_def_symbols.find(token.value) == m_def_symbols.end())
        {
            writer.write(tokenizer.consume().value);
            continue;
        }

        // replace symbol with value
        std::string symbol = tokenizer.consume().value;

        // check if the symbol has parameters
        std::vector<std::vector<Tokenizer::Token>> parameters;
        if (tokenizer.is_next({Tokenizer::OPEN_PARANTHESIS}))
        {
            tokenizer.consume(); // '('
            while (!tokenizer.is_next({Tokenizer::CLOSE_PARANTHESIS}))
            {
                tokenizer.skip_next(Tokenizer::WHITESPACES);
                std::vector<Tokenizer::Token> parameter;

                // find all tokens that make up the current parameter
                while (!tokenizer.is_next({Tokenizer::COMMA, Tokenizer::CLOSE_PARANTHESIS}))
                {
                    parameter.push_back(tokenizer.consume());
                }

                // remove whitespace from the end of the parameter
                while (parameter.size() > 0 && parameter.back().is(Tokenizer::WHITESPACES))
                {
                    parameter.pop_back();
                }

                // add to parameters list
                parameters.push_back(parameter);

                // if the next is a comma, expect another parameter
                if (tokenizer.is_next({Tokenizer::COMMA}))
                {
                    tokenizer.consume();
                }
                else
                {
                    tokenizer.expect_next({Tokenizer::CLOSE_PARANTHESIS},
                            "Preprocessor::preprocess() - Expected ')' in symbol parameters.");
                }
            }
            tokenizer.consume({Tokenizer::CLOSE_PARANTHESIS}, "Preprocessor::preprocess() - Expected ')'.");
        }

        // check if the symbol has a definition with the same number of parameters
        if (m_def_symbols.at(symbol).find(parameters.size()) == m_def_symbols.at(symbol).end())
        {
            ERROR("Preprocessor::preprocess() - Undefined symbol: %s", symbol.c_str());
        }

        // replace all occurances of a parameter with the value passed in as the parameter
        std::vector<Tokenizer::Token> definition = m_def_symbols.at(symbol).at(parameters.size()).value;
        for (size_t j = 0; j < definition.size(); j++)
        {
            if (definition[j].type != Tokenizer::SYMBOL)
            {
                continue;
            }

            // check if the symbol is a parameter
            for (size_t k = 0; k < parameters.size(); k++)
            {
                if (definition[j].value == m_def_symbols.at(symbol).at(parameters.size()).parameters[k])
                {
                    // replace the symbol with the parameter value
                    definition.erase(definition.begin() + j);
                    definition.insert(definition.begin() + j, parameters[k].begin(), parameters[k].end());
                    break;
                }
            }
        }

        // insert the definition into the tokens list
        tokenizer.insert_tokens(definition, tokenizer.get_toki());
    }

    m_state = State::PROCESSED_SUCCESS;
    writer.close();

    DEBUG("Preprocessor::preprocess() - Preprocessed file: %s", m_input_file.get_name().c_str());

    // log macros
    for (std::pair<std::string, Macro> macro_pair : m_macros)
    {
        DEBUG("Preprocessor::preprocess() - Macro: %s", macro_pair.second.to_string().c_str());
    }

    return m_output_file;
}


std::vector<Preprocessor::Macro>
Preprocessor::macros_with_header(const std::string &macro_name,
                                 const std::vector<std::vector<Tokenizer::Token>> &args)
{
    std::vector<Macro> possible_macros;
    for (const std::pair<std::string, Macro> macro_pair : m_macros)
    {
        if (macro_pair.second.name == macro_name && macro_pair.second.args.size() == args.size())
        {
            possible_macros.push_back(macro_pair.second);
        }
    }
    return possible_macros;
}


void Preprocessor::_include()
{
    tokenizer.consume(); // '#include'
    tokenizer.skip_next_regex("[ \t]");

    // the path to the included file
    std::string full_path_from_working_dir;

    if (tokenizer.is_next({Tokenizer::LITERAL_STRING}, "Preprocessor::_include() - Missing include filename."))
    {
        // local include
        std::string loc_path = tokenizer.consume().value;
        loc_path = m_input_file.get_dir() + File::SEPARATOR + loc_path.substr(1, loc_path.length() - 2);
        full_path_from_working_dir = trim_dir_path(loc_path);
    }
    else
    {
        // expect <"...">
        tokenizer.consume({Tokenizer::OPERATOR_LOGICAL_LESS_THAN}, "Preprocessor::_include() - Missing '<'.");
        std::string sys_file_path = tokenizer.consume({Tokenizer::LITERAL_STRING},
                "Preprocessor::_include() - Expected string literal.").value;
        sys_file_path = sys_file_path.substr(1, sys_file_path.length() - 2);
        tokenizer.consume({Tokenizer::OPERATOR_LOGICAL_GREATER_THAN}, "Preprocessor::_include() - Missing '>'.");

        // check if file exists in system include directories
        bool found_sys_file = false;
        for (Directory dir : m_process->get_system_dirs())
        {
            if (dir.subfile_exists(sys_file_path))
            {
                if (found_sys_file)
                {
                    // already found file
                    ERROR("Preprocessor::_include() - Multiple matching files found in system include directories: %s",
                            sys_file_path.c_str());
                }

                full_path_from_working_dir = dir.get_path() + File::SEPARATOR + sys_file_path;
                found_sys_file = true;
            }
        }

        if (!found_sys_file)
        {
            ERROR("Preprocessor::_include() - File not found in system include directories: %s",
                    sys_file_path.c_str());
        }
    }

    tokenizer.skip_next({Tokenizer::WHITESPACE_SPACE, Tokenizer::WHITESPACE_TAB});
    tokenizer.consume({Tokenizer::WHITESPACE_NEWLINE},
            "Preprocessor::_include() - #include should be on it's own line");

    // process included file
    DEBUG("Preprocessor::_include() - include path: %s", full_path_from_working_dir.c_str());
    File include_file = File(full_path_from_working_dir);
    EXPECT_TRUE_SS(include_file.exists(), std::stringstream()
            << "Preprocessor::_include() - Include file does not exist: " << full_path_from_working_dir);

    // instead of writing all the contents to the output file, simply
    // tokenize the file and insert into the current token list
    Preprocessor included_preprocessor(m_process, include_file, m_output_file.get_path());

    // yoink the tokens from the included file and insert
    tokenizer.insert_tokens(included_preprocessor.tokenizer.get_tokens(), tokenizer.get_toki());
}

void Preprocessor::_macro()
{
    tokenizer.consume(); // '#macro'
    tokenizer.skip_next_regex("[ \t]");

    // parse macro name
    std::string macro_name = tokenizer.consume({Tokenizer::SYMBOL},
            "Preprocessor::_macro() - Expected macro name.").value;
    Macro macro(macro_name);

    // start of invoked arguments
    tokenizer.skip_next_regex("[ \t]");
    tokenizer.consume({Tokenizer::OPEN_PARANTHESIS}, "Preprocessor::_macro() - Expected '('.");

    // parse arguments
    while (!tokenizer.is_next({Tokenizer::CLOSE_PARANTHESIS},
            "Preprocessor::_macro() - Expected macro header."))
    {
        tokenizer.skip_next_regex("[ \t]");
        std::string argName = tokenizer.consume({Tokenizer::SYMBOL},
                "Preprocessor::_macro() - Expected argument name.").value;

        tokenizer.skip_next_regex("[ \t]");
        macro.args.push_back(Argument(argName));


        // parse comma or expect closing parenthesis
        tokenizer.skip_next_regex("[ \t]");
        if (tokenizer.is_next({Tokenizer::COMMA}))
        {
            tokenizer.consume();
        }
    }

    // consume the closing parenthesis
    tokenizer.consume({Tokenizer::CLOSE_PARANTHESIS}, "Preprocessor::_macro() - Expected ')'.");
    tokenizer.skip_next_regex("[ \t\n]");

    // parse macro definition
    while (!tokenizer.is_next({Tokenizer::PREPROCESSOR_MACEND},
            "Preprocessor::_macro() - Expected macro definition." ))
    {
        macro.definition.push_back(tokenizer.consume());
    }
    tokenizer.consume({Tokenizer::PREPROCESSOR_MACEND}, "Preprocessor::_macro() - Expected '#macend'.");
    tokenizer.skip_next_regex("[ \t]");
    tokenizer.consume({Tokenizer::WHITESPACE_NEWLINE},
            "Preprocessor::_macro() - #macend should be on it's own line.");

    // check if macro declaration is unique
    EXPECT_TRUE(m_macros.find(macro.header()) == m_macros.end(),
            "Preprocessor::_macro() - Macro already defined: %s", macro.header().c_str());

    // add macro to list of macros
    m_macros.insert(std::pair<std::string,Macro>(macro.header(), macro));
}

void Preprocessor::_macret()
{
    tokenizer.consume(); // '#macret'
    tokenizer.skip_next_regex("[ \t]");

    std::vector<Tokenizer::Token> return_value;
    if (m_macro_stack.empty())
    {
        ERROR("Preprocessor::_macret() - Unexpected macret token.");
    }

    // macro contains a return value
    while (!tokenizer.is_next({Tokenizer::WHITESPACE_NEWLINE}))
    {
        return_value.push_back(tokenizer.consume());
    }

    // skip all the tokens after this till the end of the current macro's definition
    // we can achieve this by counting the number of scope levels,
    // incrementing if we reach a .scope token and decrementing if we reach a .scend token.
    // If we reach 0, we know we have reached the end of the macro definition.
    int cur_rel_scope_level = 0;
    while (tokenizer.has_next())
    {
        if (tokenizer.is_next({Tokenizer::ASSEMBLER_SCOPE}))
        {
            cur_rel_scope_level++;
        }
        else if (tokenizer.is_next({Tokenizer::ASSEMBLER_SCEND}))
        {
            cur_rel_scope_level--;
        }
        tokenizer.consume();

        if (cur_rel_scope_level == 0)
        {
            break;
        }
    }

    if (cur_rel_scope_level != 0)
    {
        ERROR("Preprocessor::_macret() - Unclosed scope.");
    }

    // add '#define current_macro_output_symbol expression' to tokens
    std::vector<Tokenizer::Token> set_return_statement;
    vector_util::append(set_return_statement, Tokenizer::tokenize(string_util::format("#define {} ",
            m_macro_stack.top().first), false));
    vector_util::append(set_return_statement, return_value);
    tokenizer.insert_tokens(set_return_statement, tokenizer.get_toki());

    // pop the macro from the stack
    m_macro_stack.pop();
}

void Preprocessor::_macend()
{
    // should never reach this. This should be consumed by the _macro function.
    ERROR("Preprocessor::_macend() - Unexpected macro end token.");
}

void Preprocessor::_invoke()
{
    tokenizer.consume();
    tokenizer.skip_next_regex("[ \t]");

    // parse macro name
    std::string macro_name = tokenizer.consume({Tokenizer::SYMBOL},
            "Preprocessor::_invoke() - Expected macro name.").value;

    // parse arguments
    tokenizer.skip_next_regex("[ \t]");
    tokenizer.consume({Tokenizer::OPEN_PARANTHESIS}, "Preprocessor::_invoke() - Expected '('.");
    std::vector<std::vector<Tokenizer::Token>> arguments;
    while (!tokenizer.is_next({Tokenizer::CLOSE_PARANTHESIS}, "Preprocessor::_invoke() - Expected ')'."))
    {
        tokenizer.skip_next_regex("[ \t]");

        std::vector<Tokenizer::Token> argumentValues;
        while (!tokenizer.is_next({Tokenizer::COMMA, Tokenizer::CLOSE_PARANTHESIS, Tokenizer::WHITESPACE_NEWLINE},
                "Preprocessor::_invoke() - Expected ')'."))
        {
            argumentValues.push_back(tokenizer.consume());
        }
        arguments.push_back(argumentValues);

        if (tokenizer.is_next({Tokenizer::COMMA}))
        {
            tokenizer.consume();
        }
    }
    tokenizer.consume({Tokenizer::CLOSE_PARANTHESIS}, "Preprocessor::_invoke() - Expected ')'.");
    tokenizer.skip_next_regex("[ \t]");

    // parse the output symbol if there is one
    bool has_output = tokenizer.is_next({Tokenizer::SYMBOL});
    std::string output_symbol = "";
    if (has_output)
    {
        output_symbol = tokenizer.consume({Tokenizer::SYMBOL},
                "Preprocessor::_invoke() - Expected output symbol.").value;
    }

    tokenizer.skip_next_regex("[ \t]");
    tokenizer.consume({Tokenizer::WHITESPACE_NEWLINE},
            "Preprocessor::_invoke() - Macro preprocessors must be on it's own line.");

    // check if macro exists
    std::vector<Macro> possibleMacros = macros_with_header(macro_name, arguments);
    if (possibleMacros.size() == 0)
    {
        ERROR("Preprocessor::_invoke() - Macro does not exist: %s", macro_name.c_str());
    }
    else if (possibleMacros.size() > 1)
    {
        ERROR("Preprocessor::_invoke() - Multiple macros with the same name and number of arguments: %s",
                macro_name.c_str());
    }
    Macro &macro = possibleMacros[0];

    // replace the '#invoke symbol(arg1, arg2,..., argn) ?symbol' with the macro definition
    std::vector<Tokenizer::Token> expanded_macro_invoke;

    // append a new '.scope' symbol to the tokens list
    vector_util::append(expanded_macro_invoke, tokenizer.tokenize(".scope\n" +
            string_util::repeat("\t", 1 + tokenizer.get_state().prev_indent), false));


    // then for each argument, add an '#define argname argval' statement
    // if the symbol has already been defined, store previous definition
    std::vector<Symbol> previously_defined;
    for (size_t i = 0; i < arguments.size(); i++)
    {
        if (is_symbol_def (macro.args[i].name, 0))
        {
            previously_defined.push_back(m_def_symbols.at(macro.args[i].name).at(0));
        }
        vector_util::append(expanded_macro_invoke, Tokenizer::tokenize(string_util::format("#define {} ",
                macro.args[i].name), false));
        vector_util::append(expanded_macro_invoke, arguments[i]);
        expanded_macro_invoke.push_back(Tokenizer::Token(Tokenizer::WHITESPACE_NEWLINE, "\n"));
    }

    // then append the macro definition
    for (const Tokenizer::Token &tok : macro.definition)
    {
        expanded_macro_invoke.push_back(tok);

        if (tok.type == Tokenizer::WHITESPACE_NEWLINE)
        {
            for (int i = 0; i < tokenizer.get_state().prev_indent + 1; i++)
            {
                expanded_macro_invoke.push_back(Tokenizer::Token(Tokenizer::WHITESPACE_TAB, "\t"));
            }
        }
    }

    // finally end with a '.scend' symbol
    vector_util::append(expanded_macro_invoke, tokenizer.tokenize("\n" +
            string_util::repeat("\t", tokenizer.get_state().prev_indent) + ".scend\n", false));

    // push the macro and output symbol if any onto the macro stack
    m_macro_stack.push(std::pair<std::string, Macro>(output_symbol, macro));

    for (const Symbol &symbol : previously_defined)
    {
        vector_util::append(expanded_macro_invoke, Tokenizer::tokenize(string_util::format("#define {} ",
            symbol.name), false));
        vector_util::append(expanded_macro_invoke, symbol.value);
        expanded_macro_invoke.push_back(Tokenizer::Token(Tokenizer::WHITESPACE_NEWLINE, "\n"));
    }

    // print out expanded macro
    std::stringstream ss;
    for (const Tokenizer::Token& token : expanded_macro_invoke)
    {
        ss << token.value;
    }
    DEBUG("Preprocessor::_invoke() - Expanded macro: %s", ss.str().c_str());

    // insert into the tokens list
    tokenizer.insert_tokens(expanded_macro_invoke, tokenizer.get_toki());
}

void Preprocessor::_define()
{
    tokenizer.consume(); // '#define'
    tokenizer.skip_next_regex("[ \t]");

    // symbol
    std::string symbol = tokenizer.consume({Tokenizer::SYMBOL},
            "Preprocessor::_define() - Expected symbol.").value;
    tokenizer.skip_next_regex("[ \t]");

    // check for parameter declaration
    std::vector<std::string> parameters;
    std::set<std::string> ensure_unique_params;
    if (tokenizer.is_next({Tokenizer::OPEN_PARANTHESIS}))
    {
        tokenizer.consume(); // '('

        // parse parameters
        while (!tokenizer.is_next({Tokenizer::CLOSE_PARANTHESIS}))
        {
            tokenizer.skip_next_regex("[ \t]");
            std::string parameter = tokenizer.consume({Tokenizer::SYMBOL},
                    "Preprocessor::_define() - Expected parameter.").value;

            // ensure the parameter symbol has not been used before in this definition parameters
            EXPECT_TRUE_SS(ensure_unique_params.find(parameter) == ensure_unique_params.end(),
                    std::stringstream() << "Preprocessor::_define() - Duplicate parameter: " << parameter);
            parameters.push_back(parameter);
            ensure_unique_params.insert(parameter);

            // parse comma or expect closing parenthesis
            tokenizer.skip_next_regex("[ \t]");
            if (tokenizer.is_next({Tokenizer::COMMA}))
            {
                tokenizer.consume();
            }
            else
            {
                tokenizer.expect_next({Tokenizer::CLOSE_PARANTHESIS},
                        "Preprocessor::_define() - Expected ')'.");
            }
        }

        // expect ')'
        tokenizer.consume({Tokenizer::CLOSE_PARANTHESIS}, "Preprocessor::_define() - Expected ')'.");
        tokenizer.skip_next_regex("[ \t]");
    }

    // value
    std::vector<Tokenizer::Token> tokens;
    bool read_next_line = false;
    while (!tokenizer.is_next({Tokenizer::WHITESPACE_NEWLINE}) || read_next_line)
    {
        read_next_line = false;
        tokens.push_back(tokenizer.consume());

        // check if we should read the nextline provided the next token is a newline
        // and the previous token read was a '\' character
        if (tokenizer.is_next({Tokenizer::WHITESPACE_NEWLINE}) && tokens.back().type == Tokenizer::BACK_SLASH)
        {
            read_next_line = true;

            // remove the '\' character
            tokens.pop_back();
        }
    }

    tokenizer.consume({Tokenizer::WHITESPACE_NEWLINE},
            "Preprocessor::_define() - Definition preprocessors must be on it's own line.");

    // add to symbols mapping
    m_def_symbols.insert(std::pair<std::string, std::map<int, Symbol>>(symbol, std::map<int, Symbol>()));
    if (m_def_symbols.at(symbol).find(parameters.size()) != m_def_symbols.at(symbol).end())
    {
        m_def_symbols.at(symbol).erase(parameters.size());
    }
    m_def_symbols.at(symbol).insert(std::pair<int, Symbol>(parameters.size(), Symbol(symbol, parameters, tokens)));
}

void Preprocessor::cond_block(bool cond_met)
{
    int rel_scope_level = 0;
    Tokenizer::State prev_state = tokenizer.get_state();

    bool found_next_block = false;
    Tokenizer::State next_block_state;

    bool found_end_block = false;
    Tokenizer::State end_block_state;
    while (tokenizer.has_next())
    {
        if (rel_scope_level == 0 && tokenizer.is_next({Tokenizer::PREPROCESSOR_ENDIF}))
        {
            if (!found_next_block)
            {
                found_next_block = true;
                next_block_state = tokenizer.get_state();
            }

            found_end_block = true;
            end_block_state = tokenizer.get_state();
            break;
        }
        else if (rel_scope_level == 0 &&
                tokenizer.is_next({Tokenizer::PREPROCESSOR_ELSE, Tokenizer::PREPROCESSOR_ELSEDEF,
                                   Tokenizer::PREPROCESSOR_ELSENDEF,
                                   Tokenizer::PREPROCESSOR_ELSEEQU, Tokenizer::PREPROCESSOR_ELSENEQU,
                                   Tokenizer::PREPROCESSOR_ELSELESS, Tokenizer::PREPROCESSOR_ELSEMORE}))
        {
            if (!found_next_block)
            {
                found_next_block = true;
                next_block_state = tokenizer.get_state();
            }

            // start of next conditional block that should be checked if the current conditional block was
            // not entered
            if (!cond_met)
            {
                break;
            }
        }

        if (tokenizer.is_next({Tokenizer::PREPROCESSOR_IFDEF, Tokenizer::PREPROCESSOR_IFNDEF,
                               Tokenizer::PREPROCESSOR_IFEQU, Tokenizer::PREPROCESSOR_IFNEQU,
                               Tokenizer::PREPROCESSOR_IFLESS, Tokenizer::PREPROCESSOR_IFMORE}))
        {
            rel_scope_level++;
        }
        else if (tokenizer.is_next({Tokenizer::PREPROCESSOR_ENDIF}))
        {
            rel_scope_level--;
        }
        tokenizer.consume();
    }

    tokenizer.set_state(prev_state);

    if ((cond_met && !found_end_block) || (!cond_met && !found_next_block))
    {
        DEBUG("condition=%u | endIf=%u | next_block_tok_i=%u", word(cond_met), word(found_end_block), word(found_next_block));
        ERROR("Preprocessor::cond_block() - Unclosed conditional block." );
    }

    if (cond_met)
    {
        DEBUG(" | endIf=%u | next_block_tok_i=%u", word(found_end_block), word(found_next_block));
        if (found_next_block)
        {
            // remove all tokens from the next block to the endif
            tokenizer.remove_tokens(next_block_state.toki, end_block_state.toki);
        }
    }
    else
    {
        // move token index to the start of the next conditional block (or endif)
        tokenizer.set_state(next_block_state);
    }
}

bool Preprocessor::is_symbol_def(const std::string &symbol_name, int num_params)
{
    return m_def_symbols.find(symbol_name) != m_def_symbols.end() &&
            m_def_symbols.at(symbol_name).find(num_params) != m_def_symbols.at(symbol_name).end();
}

void Preprocessor::_cond_on_def()
{
    Tokenizer::Token cond_tok = tokenizer.consume();
    tokenizer.skip_next_regex("[ \t]");

    // symbol
    std::string symbol = tokenizer.consume({Tokenizer::SYMBOL}, "Preprocessor::_" +
            cond_tok.value.substr(1) + "() - Expected symbol.").value;
    tokenizer.skip_next({Tokenizer::WHITESPACE_SPACE, Tokenizer::WHITESPACE_TAB});

    tokenizer.consume({Tokenizer::WHITESPACE_NEWLINE},
            "Preprocessor::_cond_on_def() - Conditional preprocessors must be on it's own line.");

    if (cond_tok.type == Tokenizer::PREPROCESSOR_IFDEF || cond_tok.type == Tokenizer::PREPROCESSOR_ELSEDEF)
    {
        cond_block(is_symbol_def(symbol, 0));
    }
    else if (cond_tok.type == Tokenizer::PREPROCESSOR_IFNDEF ||
            cond_tok.type == Tokenizer::PREPROCESSOR_ELSENDEF)
    {
        cond_block(!is_symbol_def(symbol, 0));
    }
    else
    {
        ERROR("Preprocessor::_cond_on_def() - Unexpected conditional token: %s", cond_tok.value.c_str());
    }
}

void Preprocessor::_cond_on_value()
{
    Tokenizer::Token cond_tok = tokenizer.consume();
    tokenizer.skip_next_regex("[ \t]");

    // symbol
    std::string symbol = tokenizer.consume({Tokenizer::SYMBOL}, "Preprocessor::_" +
            cond_tok.value.substr(1) + "() - Expected symbol.").value;
    tokenizer.skip_next_regex("[ \t]");

    // extract symbol's string value
    std::string symbol_val;
    if (is_symbol_def(symbol, 0))
    {
        for (Tokenizer::Token& token : m_def_symbols.at(symbol).at(0).value)
        {
            symbol_val += token.value;
        }
    }

    // value
    std::string value;
    bool read_next_line = false;
    while (!tokenizer.is_next({Tokenizer::WHITESPACE_NEWLINE}) || read_next_line)
    {
        read_next_line = false;
        value += tokenizer.consume().value;

        // check if we should read the nextline provided the next token is a newline
        // and the previous token read was a '\' character
        if (tokenizer.is_next({Tokenizer::WHITESPACE_NEWLINE}) && value.back() == '\\')
        {
            read_next_line = true;

            // remove the '\' character
            value.pop_back();
        }
    }

    tokenizer.consume({Tokenizer::WHITESPACE_NEWLINE},
            "Preprocessor::_cond_on_value() - Conditional preprocessors must be on it's own line.");

    switch (cond_tok.type)
    {
        case Tokenizer::PREPROCESSOR_IFEQU:
        case Tokenizer::PREPROCESSOR_ELSEEQU:
            cond_block(value == symbol_val);
            break;
        case Tokenizer::PREPROCESSOR_IFNEQU:
        case Tokenizer::PREPROCESSOR_ELSENEQU:
            cond_block(value != symbol_val);
            break;
        case Tokenizer::PREPROCESSOR_IFLESS:
        case Tokenizer::PREPROCESSOR_ELSELESS:
            cond_block(symbol_val < value);
            break;
        case Tokenizer::PREPROCESSOR_IFMORE:
        case Tokenizer::PREPROCESSOR_ELSEMORE:
            cond_block(symbol_val > value);
            break;
        default:
            ERROR("Preprocessor::_cond_on_value() - Unexpected conditional token: %s", cond_tok.value.c_str());
    }
}

void Preprocessor::_else()
{
    tokenizer.consume(); // '#else'
    tokenizer.skip_next_regex("[ \t]");

    tokenizer.consume({Tokenizer::WHITESPACE_NEWLINE},
            "Preprocessor::_else() - Conditional preprocessors must be on it's own line.");
}

void Preprocessor::_endif()
{
    tokenizer.consume(); // '#endif'
    tokenizer.skip_next_regex("[ \t]");

    tokenizer.consume({Tokenizer::WHITESPACE_NEWLINE},
            "Preprocessor::_endif() - Conditional preprocessors must be on it's own line.");
}

void Preprocessor::_undefine()
{
    tokenizer.consume(); // '#define'
    tokenizer.skip_next_regex("[ \t]");

    // symbol
    std::string symbol = tokenizer.consume({Tokenizer::SYMBOL},
            "Preprocessor::_define() - Expected symbol.").value;
    tokenizer.skip_next_regex("[ \t]");

    tokenizer.consume({Tokenizer::WHITESPACE_NEWLINE},
            "Preprocessor::_undefine() - Definition preprocessors must be on it's own line.");

    // if a number of parameters was specified, remove that definition otherwise remove all definitions
    if (tokenizer.is_next({Tokenizer::LITERAL_NUMBER_DECIMAL}))
    {
        int num_params = std::stoi(tokenizer.consume({Tokenizer::LITERAL_NUMBER_DECIMAL},
                "Preprocessor::_undefine() - Expected number of parameters.").value);
        m_def_symbols[symbol].erase(num_params);
    }
    else
    {
        m_def_symbols.erase(symbol);
    }
}

Preprocessor::State Preprocessor::get_state()
{
    return m_state;
}