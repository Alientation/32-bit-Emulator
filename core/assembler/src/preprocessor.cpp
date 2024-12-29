#include "assembler/preprocessor.h"
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

/**
 * Destructs a preprocessor object.
 */
Preprocessor::~Preprocessor()
{
    for (std::pair<std::string, Macro*> macro_pair : m_macros)
	{
        delete macro_pair.second;
    }
}

/**
 * Preprocesses the file.
 */
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

	// parses the tokens
	int cur_indent_level = 0;
	int target_indent_level = 0;
	while (tokenizer.has_next())
	{
		Tokenizer::Token& token = tokenizer.get_token();

        // skip back to back newlines
        if (token.type == Tokenizer::WHITESPACE_NEWLINE && writer.last_byte_written() == '\n')
		{
			tokenizer.skip_next();
            continue;
        }

		// update current indent level
		if (token.type == Tokenizer::WHITESPACE_TAB)
		{
			cur_indent_level++;
		}
		else if (token.type == Tokenizer::WHITESPACE_NEWLINE)
		{
			cur_indent_level = 0;
		}

		// update target indent level
		if (token.type == Tokenizer::ASSEMBLER_SCEND)
		{
			target_indent_level--;
		}

		// format the output with improved indents
		if (cur_indent_level < target_indent_level && token.type == Tokenizer::WHITESPACE_SPACE)
		{
			// don't output whitespaces if a tab is expected
			continue;
		}
		else if (cur_indent_level < target_indent_level
				&& token.type != Tokenizer::WHITESPACE_TAB && token.type != Tokenizer::WHITESPACE_NEWLINE)
		{
			// append tabs
			while (cur_indent_level < target_indent_level)
			{
				writer.write("\t");
				cur_indent_level++;
			}
		}

		// if token is valid preprocessor, call the preprocessor function
		if (preprocessors.find(token.type) != preprocessors.end())
		{
			(this->*preprocessors[token.type])();
		}
		else
		{
            // check if this is a defined symbol
            if (token.type == Tokenizer::SYMBOL && m_def_symbols.find(token.value) != m_def_symbols.end())
			{
                // replace symbol with value
                std::string symbol = token.value;
				tokenizer.consume();
                tokenizer.skip_next("[ \t]");

                // check if the symbol has parameters
                std::vector<std::vector<Tokenizer::Token>> parameters;
                if (tokenizer.is_next({Tokenizer::OPEN_PARANTHESIS}))
				{
                    tokenizer.consume(); // '('
                    while (!tokenizer.is_next({Tokenizer::CLOSE_PARANTHESIS}))
					{
                        std::vector<Tokenizer::Token> parameter;
                        while (!tokenizer.is_next({Tokenizer::COMMA, Tokenizer::CLOSE_PARANTHESIS}))
						{
                            parameter.push_back(tokenizer.consume());
                        }
                        parameters.push_back(parameter);
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
                    if (definition[j].type == Tokenizer::SYMBOL)
					{
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
                }

                // insert the definition into the tokens list
                tokenizer.m_tokens.insert(tokenizer.m_tokens.begin() + tokenizer.get_toki(),
						definition.begin(), definition.end());
            }
			else
			{
                writer.write(tokenizer.consume().value);
            }
		}

		// update target indent level
		if (token.type == Tokenizer::ASSEMBLER_SCOPE)
		{
			target_indent_level++;
		}
	}

	m_state = State::PROCESSED_SUCCESS;
	writer.close();

	DEBUG("Preprocessor::preprocess() - Preprocessed file: %s", m_input_file.get_name().c_str());

    // log macros
    for (std::pair<std::string, Macro*> macro_pair : m_macros)
	{
        DEBUG("Preprocessor::preprocess() - Macro: %s", macro_pair.second->to_string().c_str());
    }

	return m_output_file;
}


/**
 * Returns the macros that match the given macro name and arguments list.
 *
 * @param macro_name the name of the macro.
 * @param arguments the arguments passed to the macro.
 *
 * TODO: possibly in the future should consider filtering for macros that have the same order of argument types.
 * 		This would require us knowing the types of symbols and expressions in the preprocessor state
 * 		which is not ideal
 *
 * @return the macros with the given name and number of arguments.
 */
std::vector<Preprocessor::Macro*>
Preprocessor::macros_with_header(std::string macro_name,
								 std::vector<std::vector<Tokenizer::Token>> args)
{
	std::vector<Macro*> possible_macros;
	for (std::pair<std::string, Macro*> macro_pair : m_macros)
	{
		if (macro_pair.second->name == macro_name && macro_pair.second->args.size() == args.size())
		{
			possible_macros.push_back(macro_pair.second);
		}
	}
	return possible_macros;
}


/**
 * Inserts the file contents into the current file.
 *
 * USAGE: #include "filepath"|<filepath>
 *
 * "filepath": looks for files located in the current directory.
 * <filepath>: prioritizes files located in the include directory, if not found, looks in the
 * current directory.
 */
void Preprocessor::_include()
{
	tokenizer.consume(); // '#include'
	tokenizer.skip_next("[ \t]");

	// the path to the included file
	std::string full_path_from_working_dir;

	bool found_file = false;
    if (tokenizer.is_next({Tokenizer::LITERAL_STRING}, "Preprocessor::_include() - Missing include filename."))
	{
        // local include
		std::string loc_path = tokenizer.consume().value;
		loc_path = m_input_file.get_dir() + File::SEPARATOR + loc_path.substr(1, loc_path.length() - 2);
		full_path_from_working_dir = trim_dir_path(loc_path);
		found_file = true;
    }

	if (!found_file)
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

	// process included file
	DEBUG("Preprocessor::_include() - include path: %s", full_path_from_working_dir.c_str());
	File include_file = File(full_path_from_working_dir);
	EXPECT_TRUE_SS(include_file.exists(), std::stringstream()
			<< "Preprocessor::_include() - Include file does not exist: " << full_path_from_working_dir);

	// instead of writing all the contents to the output file, simply
	// tokenize the file and insert into the current token list
	Preprocessor included_preprocessor(m_process, include_file, m_output_file.get_path());

	// yoink the tokens from the included file and insert
	tokenizer.m_tokens.insert(tokenizer.m_tokens.begin() + tokenizer.get_toki(),
			included_preprocessor.tokenizer.m_tokens.begin(), included_preprocessor.tokenizer.m_tokens.end());
}

/**
 * Defines a macro symbol with n arguments and optionally a return type.
 *
 * USAGE: #macro [symbol]([arg1 ?: TYPE, arg2 ?: TYPE,..., argn ?: TYPE]) ?: TYPE
 *
 * If a return type is specified and the macro definition does not return a value an error is thrown.
 * There cannot be a macro definition within this macro definition.
 * Note that the macro symbol is separate from label symbols and will not be present after preprocessing.
 */
void Preprocessor::_macro()
{
	tokenizer.consume(); // '#macro'
	tokenizer.skip_next("[ \t]");

	// parse macro name
	std::string macro_name = tokenizer.consume({Tokenizer::SYMBOL},
			"Preprocessor::_macro() - Expected macro name.").value;
    Macro* macro = new Macro(macro_name);

    // start of invoked arguments
	tokenizer.skip_next("[ \t\n]");
	tokenizer.consume({Tokenizer::OPEN_PARANTHESIS}, "Preprocessor::_macro() - Expected '('.");

    // parse arguments
    while (!tokenizer.is_next({Tokenizer::CLOSE_PARANTHESIS},
			"Preprocessor::_macro() - Expected macro header."))
	{
        tokenizer.skip_next("[ \t\n]");
        std::string argName = tokenizer.consume({Tokenizer::SYMBOL},
				"Preprocessor::_macro() - Expected argument name.").value;

        tokenizer.skip_next("[ \t\n]");
        macro->args.push_back(Argument(argName));


        // parse comma or expect closing parenthesis
        tokenizer.skip_next("[ \t\n]");
        if (tokenizer.is_next({Tokenizer::COMMA}))
		{
            tokenizer.consume();
        }
    }

    // consume the closing parenthesis
    tokenizer.consume({Tokenizer::CLOSE_PARANTHESIS}, "Preprocessor::_macro() - Expected ')'.");
    tokenizer.skip_next("[ \t\n]");

    // parse macro definition
    tokenizer.skip_next("[ \t\n]");
    while (!tokenizer.is_next({Tokenizer::PREPROCESSOR_MACEND},
			"Preprocessor::_macro() - Expected macro definition." ))
	{
        macro->definition.push_back(tokenizer.consume());
    }
    tokenizer.consume({Tokenizer::PREPROCESSOR_MACEND}, "Preprocessor::_macro() - Expected '#macend'.");

    // check if macro declaration is unique
	EXPECT_TRUE_SS(m_macros.find(macro->header()) == m_macros.end(), std::stringstream()
			<< "Preprocessor::_macro() - Macro already defined: " << macro->header());

    // add macro to list of macros
    m_macros.insert(std::pair<std::string,Macro*>(macro->header(), macro));
}

/**
 * Stops processing the macro and returns the value of the expression.
 *
 * USAGE: #macret [?expression]
 *
 * If the macro does not have a return type the macret must return nothing.
 * If the macro has a return type the macret must return a value of that type
 */
void Preprocessor::_macret()
{
	tokenizer.consume(); // '#macret'
	tokenizer.skip_next("[ \t]");

	std::vector<Tokenizer::Token> return_value;
	if (m_macro_stack.empty())
	{
		ERROR("Preprocessor::_macret() - Unexpected macret token.");
	}

	// macro contains a return value
	bool does_macro_return = m_macro_stack.top().second->return_type != Tokenizer::UNKNOWN;
	if (does_macro_return)
	{
		while (!tokenizer.is_next({Tokenizer::WHITESPACE_NEWLINE}))
		{
			return_value.push_back(tokenizer.consume());
		}
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

	// add'.equ current_macro_output_symbol expression' to tokens
	if (does_macro_return)
	{
		std::vector<Tokenizer::Token> set_return_statement;
		vector_util::append(set_return_statement, Tokenizer::tokenize(string_util::format(".equ {} ",
				m_macro_stack.top().first)));
		vector_util::append(set_return_statement, return_value);
		tokenizer.m_tokens.insert(tokenizer.m_tokens.begin() + tokenizer.get_toki(),
				set_return_statement.begin(), set_return_statement.end());
	}

	// pop the macro from the stack
	m_macro_stack.pop();
}

/**
 * Closes a macro definition.
 *
 * USAGE: #macend
 *
 * If a macro is not closed an error is thrown.
 */
void Preprocessor::_macend()
{
    // should never reach this. This should be consumed by the _macro function.
    ERROR("Preprocessor::_macend() - Unexpected macro end token.");
}

/**
 * Invokes the macro with the given arguments.
 *
 * USAGE: #invoke [symbol]([arg1, arg2,..., argn]) [?symbol]
 *
 * If provided an output symbol, the symbol will be associated with the return value of the macro.
 * If the macro does not return a value but an output symbol is provided, an error is thrown.
 */
void Preprocessor::_invoke()
{
	tokenizer.consume();
	tokenizer.skip_next("[ \t]");

	// parse macro name
	std::string macro_name = tokenizer.consume({Tokenizer::SYMBOL},
			"Preprocessor::_invoke() - Expected macro name.").value;

	// parse arguments
	tokenizer.skip_next("[ \t\n]");
	tokenizer.consume({Tokenizer::OPEN_PARANTHESIS}, "Preprocessor::_invoke() - Expected '('.");
	std::vector<std::vector<Tokenizer::Token>> arguments;
	while (!tokenizer.is_next({Tokenizer::CLOSE_PARANTHESIS}, "Preprocessor::_invoke() - Expected ')'."))
	{
		tokenizer.skip_next("[ \t\n]");

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
	tokenizer.skip_next("[ \t\n]");

	// parse the output symbol if there is one
	bool has_output = tokenizer.is_next({Tokenizer::SYMBOL});
	std::string output_symbol = "";
	if (has_output)
	{
		output_symbol = tokenizer.consume({Tokenizer::SYMBOL},
				"Preprocessor::_invoke() - Expected output symbol.").value;
	}

	// check if macro exists
	std::vector<Macro*> possibleMacros = macros_with_header(macro_name, arguments);
	if (possibleMacros.size() == 0)
	{
		ERROR("Preprocessor::_invoke() - Macro does not exist: %s", macro_name.c_str());
	}
	else if (possibleMacros.size() > 1)
	{
		ERROR("Preprocessor::_invoke() - Multiple macros with the same name and number of arguments: %s",
				macro_name.c_str());
	}
	Macro* macro = possibleMacros[0];

	// replace the '_invoke symbol(arg1, arg2,..., argn) ?symbol' with the macro definition
	std::vector<Tokenizer::Token> expanded_macro_invoke;

	// check if the macro returns something, if so add a equate statement to store the output
	if (has_output)
	{
		vector_util::append(expanded_macro_invoke,
				Tokenizer::tokenize(string_util::format("#define {} 0\n", output_symbol)));
	}

	// then for each argument, add an '#define argname argval' statement
	// if the symbol has already been defined, store previous definition
	std::vector<Symbol> previously_defined;
	for (size_t i = 0; i < arguments.size(); i++)
	{
		if (is_symbol_def (macro->args[i].name, 0))
		{
			previously_defined.push_back(m_def_symbols.at(macro->args[i].name).at(0));
		}
		vector_util::append(expanded_macro_invoke, Tokenizer::tokenize(string_util::format("#define {} #",
				macro->args[i].name)));
		vector_util::append(expanded_macro_invoke, arguments[i]);
		expanded_macro_invoke.push_back(Tokenizer::Token(Tokenizer::WHITESPACE_NEWLINE, "\n"));
	}

	// then append the macro definition
	expanded_macro_invoke.insert(expanded_macro_invoke.end(), macro->definition.begin(),
			macro->definition.end());
	expanded_macro_invoke.push_back(Tokenizer::Token(Tokenizer::WHITESPACE_NEWLINE, "\n"));

	// push the macro and output symbol if any onto the macro stack
	m_macro_stack.push(std::pair<std::string, Macro*>(output_symbol, macro));

	for (Symbol &symbol : previously_defined)
	{
		vector_util::append(expanded_macro_invoke, Tokenizer::tokenize(string_util::format("#define {} ",
			symbol.name)));
		vector_util::append(expanded_macro_invoke, symbol.value);
		expanded_macro_invoke.push_back(Tokenizer::Token(Tokenizer::WHITESPACE_NEWLINE, "\n"));
	}

	// print out expanded macro
	std::stringstream ss;
	for (Tokenizer::Token& token : expanded_macro_invoke)
	{
		ss << token.value;
	}
	DEBUG("Preprocessor::_invoke() - Expanded macro: %s", ss.str().c_str());

	// insert into the tokens list
	tokenizer.m_tokens.insert(tokenizer.m_tokens.begin() + tokenizer.get_toki(),
			expanded_macro_invoke.begin(), expanded_macro_invoke.end());
}

/**
 * Associates the symbol with a value
 *
 * USAGE: #define [symbol] [?value]
 *
 * Replaces all instances of symbol with the value.
 * If value is not specified, the default is empty.
 */
void Preprocessor::_define()
{
    tokenizer.consume(); // '#define'
    tokenizer.skip_next("[ \t]");

    // symbol
    std::string symbol = tokenizer.consume({Tokenizer::SYMBOL},
			"Preprocessor::_define() - Expected symbol.").value;
    tokenizer.skip_next("[ \t]");

    // check for parameter declaration
    std::vector<std::string> parameters;
    std::set<std::string> ensure_unique_params;
    if (tokenizer.is_next({Tokenizer::OPEN_PARANTHESIS}))
	{
        tokenizer.consume(); // '('

        // parse parameters
        while (!tokenizer.is_next({Tokenizer::CLOSE_PARANTHESIS}))
		{
            tokenizer.skip_next("[ \t]");
            std::string parameter = tokenizer.consume({Tokenizer::SYMBOL},
					"Preprocessor::_define() - Expected parameter.").value;

            // ensure the parameter symbol has not been used before in this definition parameters
            EXPECT_TRUE_SS(ensure_unique_params.find(parameter) == ensure_unique_params.end(),
					std::stringstream() << "Preprocessor::_define() - Duplicate parameter: " << parameter);
            parameters.push_back(parameter);
            ensure_unique_params.insert(parameter);

            // parse comma or expect closing parenthesis
            tokenizer.skip_next("[ \t]");
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
        tokenizer.skip_next("[ \t]");
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

    // add to symbols mapping
    m_def_symbols.insert(std::pair<std::string, std::map<int, Symbol>>(symbol, std::map<int, Symbol>()));
	if (m_def_symbols.at(symbol).find(parameters.size()) != m_def_symbols.at(symbol).end())
	{
		m_def_symbols.at(symbol).erase(parameters.size());
	}
	m_def_symbols.at(symbol).insert(std::pair<int, Symbol>(parameters.size(), Symbol(symbol, parameters, tokens)));
}

/**
 *
 */
void Preprocessor::cond_block(bool cond_met)
{
    int rel_scope_level = 0;
    int prev_tok_i = tokenizer.get_toki();
    int next_block_tok_i = -1;
    int end_if_tok_i = -1;
    while (tokenizer.has_next())
	{
        if (rel_scope_level == 0 && tokenizer.is_next({Tokenizer::PREPROCESSOR_ENDIF}))
		{
            if (next_block_tok_i == -1)
			{
                next_block_tok_i = tokenizer.get_toki();
            }

            end_if_tok_i = tokenizer.get_toki();
            break;
        }
		else if (rel_scope_level == 0 &&
				tokenizer.is_next({Tokenizer::PREPROCESSOR_ELSE, Tokenizer::PREPROCESSOR_ELSEDEF,
								   Tokenizer::PREPROCESSOR_ELSENDEF,
								   Tokenizer::PREPROCESSOR_ELSEEQU, Tokenizer::PREPROCESSOR_ELSENEQU,
								   Tokenizer::PREPROCESSOR_ELSELESS, Tokenizer::PREPROCESSOR_ELSEMORE}))
		{
            if (next_block_tok_i == -1)
			{
                next_block_tok_i = tokenizer.get_toki();
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

	tokenizer.set_toki(prev_tok_i);

    if ((cond_met && end_if_tok_i == -1) || (!cond_met && next_block_tok_i == -1))
	{
        DEBUG("condition=%d | endIf=%d | next_block_tok_i=%d", (int) cond_met, end_if_tok_i, next_block_tok_i);
        ERROR("Preprocessor::cond_block() - Unclosed conditional block." );
    }

    if (cond_met)
	{
        DEBUG(" | endIf=%d | next_block_tok_i=%d", end_if_tok_i, next_block_tok_i);
        if (next_block_tok_i != -1)
		{
            // remove all tokens from the next block to the endif
            tokenizer.m_tokens.erase(tokenizer.m_tokens.begin() + next_block_tok_i,
					tokenizer.m_tokens.begin() + end_if_tok_i);
        }
		else
		{
			// assert, end_if_tok_i != -1
            // don't need to do anything, because there are no linked conditional blocks after this
        }
    }
	else
	{
        // move token index to the start of the next conditional block (or endif)
        tokenizer.set_toki(next_block_tok_i);
    }
}

/**
 *
 */
bool Preprocessor::is_symbol_def(std::string symbol_name, int num_params)
{
    return m_def_symbols.find(symbol_name) != m_def_symbols.end() &&
			m_def_symbols.at(symbol_name).find(num_params) != m_def_symbols.at(symbol_name).end();
}

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
void Preprocessor::_cond_on_def()
{
    Tokenizer::Token cond_tok = tokenizer.consume();
    tokenizer.skip_next("[ \t]");

    // symbol
    std::string symbol = tokenizer.consume({Tokenizer::SYMBOL}, "Preprocessor::_" +
			cond_tok.value.substr(1) + "() - Expected symbol.").value;
    tokenizer.skip_next("[ \t]");

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
void Preprocessor::_cond_on_value()
{
    Tokenizer::Token cond_tok = tokenizer.consume();
    tokenizer.skip_next("[ \t]");

    // symbol
    std::string symbol = tokenizer.consume({Tokenizer::SYMBOL}, "Preprocessor::_" +
			cond_tok.value.substr(1) + "() - Expected symbol.").value;
    tokenizer.skip_next("[ \t]");

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

/**
 * Closure of a top or lower conditional block, only includes the following text if all previous
 * conditional blocks were not included.
 *
 * USAGE: #else
 *
 * Must be preceded by a top or inner conditional block.
 * Must not be proceeded by an inner conditional block or closure.
 */
void Preprocessor::_else()
{
    tokenizer.consume(); // '#else'
    tokenizer.skip_next("[ \t]");
}

/**
 * Closes a #ifdef, #ifndef, #else, #elsedef, or #elsendef.
 *
 * USAGE: #endif
 *
 * Must be preceded by a #ifdef, #ifndef, #else, #elsedef, or #elsendef.
 */
void Preprocessor::_endif()
{
    tokenizer.consume(); // '#endif'
    tokenizer.skip_next("[ \t]");
}

/**
 * Undefines a symbol defined by #define.
 *
 * USAGE: #undefine [symbol]
 *
 * This will still work if the symbol was never defined previously.
 */
void Preprocessor::_undefine()
{
    tokenizer.consume(); // '#define'
    tokenizer.skip_next("[ \t]");

    // symbol
    std::string symbol = tokenizer.consume({Tokenizer::SYMBOL},
			"Preprocessor::_define() - Expected symbol.").value;
    tokenizer.skip_next("[ \t]");

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

/**
 * Returns the state of the preprocessor.
 *
 * @return the state of the preprocessor.
 */
Preprocessor::State Preprocessor::get_state()
{
	return m_state;
}