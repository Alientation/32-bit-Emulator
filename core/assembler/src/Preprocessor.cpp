#include "assembler/Preprocessor.h"
#include "util/Logger.h"
#include "util/StringUtil.h"
#include "util/VectorUtil.h"

#include <regex>
#include <fstream>
#include <filesystem>

Preprocessor::Argument::Argument(std::string name, Tokenizer::Type type) : name(name), type(type) { }
Preprocessor::Argument::Argument(std::string name) : name(name), type(Tokenizer::UNKNOWN) { }

Preprocessor::Macro::Macro(std::string name) : name(name), return_type(Tokenizer::UNKNOWN) { }
std::string Preprocessor::Macro::to_string() {
	std::string toString = header() + "\n";
	for (auto i = 0; i < args.size(); i++) {
		toString += "[" + std::to_string(i) + "]: " + args[i].name + ": " + Tokenizer::TYPE_TO_NAME_MAP.at(args[i].type);
	}
	toString += "-> " + Tokenizer::TYPE_TO_NAME_MAP.at(return_type) + "\n{\n";

	for (int i = 0; i < definition.size(); i++) {
		toString += definition[i].value;
	}

	return toString + "\n}";
}

std::string Preprocessor::Macro::header() {
	std::string header;
	header += name + "@(";

	for (auto i = 0; i < args.size(); i++) {
		header += Tokenizer::TYPE_TO_NAME_MAP.at(args[i].type);
		if (i < args.size() - 1) {
			header += ",";
		}
	}

	return header + "):" + Tokenizer::TYPE_TO_NAME_MAP.at(return_type);
}

Preprocessor::Symbol::Symbol(std::string name, std::vector<std::string> params, std::vector<Tokenizer::Token> value)
	: name(name), parameters(params), value(value) {  }

/**
 * Constructs a preprocessor object with the given file.
 *
 * @param process the build process object.
 * @param file the file to preprocess.
 * @param outputFilePath the path to the output file, default is the inputfile path with .bi extension.
 */
Preprocessor::Preprocessor(Process* process, const File& input_file, const std::string& output_file_path)
		: m_process(process), m_input_file(input_file) {
	// default output file path if not supplied in the constructor
	if (output_file_path.empty()) {
        m_output_file = File(m_input_file.get_name(), PROCESSED_EXTENSION, m_input_file.get_dir(), true);
	} else {
        m_output_file = File(output_file_path, true);
	}

	lgr::EXPECT_TRUE(m_process->valid_src_file(input_file), lgr::Logger::LogType::ERROR, std::stringstream()
			<< "Preprocessor::Preprocessor() - Invalid source file: " << input_file.get_extension());

	m_state = State::UNPROCESSED;
	m_tokens = Tokenizer::tokenize(input_file);
}

/**
 * Destructs a preprocessor object.
 */
Preprocessor::~Preprocessor() {
    for (std::pair<std::string, Macro*> macro_pair : m_macros) {
        delete macro_pair.second;
    }
}

/**
 * Preprocesses the file.
 */
File Preprocessor::preprocess() {
	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Preprocessor::preprocess() - Preprocessing file: "
			<< m_input_file.get_name());

	lgr::EXPECT_TRUE(m_state == State::UNPROCESSED, lgr::Logger::LogType::ERROR, std::stringstream()
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
	for (int i = 0; i < m_tokens.size(); ) {
		Tokenizer::Token& token = m_tokens[i];

        // skip back to back newlines
        if (token.type == Tokenizer::WHITESPACE_NEWLINE && writer.last_byte_written() == '\n') {
            i++;
            continue;
        }

		// update current indent level
		if (token.type == Tokenizer::WHITESPACE_TAB) {
			cur_indent_level++;
		} else if (token.type == Tokenizer::WHITESPACE_NEWLINE) {
			cur_indent_level = 0;
		}

		// update target indent level
		if (token.type == Tokenizer::ASSEMBLER_SCEND) {
			target_indent_level--;
		}

		// format the output with improved indents
		if (cur_indent_level < target_indent_level && token.type == Tokenizer::WHITESPACE_SPACE) {
			// don't output whitespaces if a tab is expected
			continue;
		} else if (cur_indent_level < target_indent_level
				&& token.type != Tokenizer::WHITESPACE_TAB && token.type != Tokenizer::WHITESPACE_NEWLINE) {
			// append tabs
			while (cur_indent_level < target_indent_level) {
				writer.write("\t");
				cur_indent_level++;
			}
		}

		// if token is valid preprocessor, call the preprocessor function
		if (preprocessors.find(token.type) != preprocessors.end()) {
			(this->*preprocessors[token.type])(i);
		} else {
            // check if this is a defined symbol
            if (token.type == Tokenizer::SYMBOL && m_def_symbols.find(token.value) != m_def_symbols.end()) {
                // replace symbol with value
                std::string symbol = token.value;
                consume(i);
                skip_tokens(i, "[ \t]");

                // check if the symbol has parameters
                std::vector<std::vector<Tokenizer::Token>> parameters;
                if (is_token(i, {Tokenizer::OPEN_PARANTHESIS})) {
                    consume(i); // '('
                    while (!is_token(i, {Tokenizer::CLOSE_PARANTHESIS})) {
                        std::vector<Tokenizer::Token> parameter;
                        while (!is_token(i, {Tokenizer::COMMA, Tokenizer::CLOSE_PARANTHESIS})) {
                            parameter.push_back(consume(i));
                        }
                        parameters.push_back(parameter);
                        if (is_token(i, {Tokenizer::COMMA})) {
                            consume(i);
                        } else {
                            expect_token(i, {Tokenizer::CLOSE_PARANTHESIS}, "Preprocessor::preprocess() - Expected ')' in symbol parameters.");
                        }
                    }
                    consume(i, {Tokenizer::CLOSE_PARANTHESIS}, "Preprocessor::preprocess() - Expected ')'.");
                }

                // check if the symbol has a definition with the same number of parameters
                if (m_def_symbols.at(symbol).find(parameters.size()) == m_def_symbols.at(symbol).end()) {
                    lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Preprocessor::preprocess() - Undefined symbol: " << symbol);
                }

                // replace all occurances of a parameter with the value passed in as the parameter
                std::vector<Tokenizer::Token> definition = m_def_symbols.at(symbol).at(parameters.size()).value;
                for (int j = 0; j < definition.size(); j++) {
                    if (definition[j].type == Tokenizer::SYMBOL) {
                        // check if the symbol is a parameter
                        for (int k = 0; k < parameters.size(); k++) {
                            if (definition[j].value == m_def_symbols.at(symbol).at(parameters.size()).parameters[k]) {
                                // replace the symbol with the parameter value
                                definition.erase(definition.begin() + j);
                                definition.insert(definition.begin() + j, parameters[k].begin(), parameters[k].end());
                                break;
                            }
                        }
                    }
                }

                // insert the definition into the tokens list
                m_tokens.insert(m_tokens.begin() + i, definition.begin(), definition.end());
            } else {
                writer.write(consume(i).value);
            }
		}

		// update target indent level
		if (token.type == Tokenizer::ASSEMBLER_SCOPE) {
			target_indent_level++;
		}
	}

	m_state = State::PROCESSED_SUCCESS;
	writer.close();

	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Preprocessor::preprocess() - Preprocessed file: " << m_input_file.get_name());

    // log macros
    for (std::pair<std::string, Macro*> macro_pair : m_macros) {
        lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Preprocessor::preprocess() - Macro: " << macro_pair.second->to_string());
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
std::vector<Preprocessor::Macro*> Preprocessor::macros_with_header(std::string macro_name, std::vector<std::vector<Tokenizer::Token>> args) {
	std::vector<Macro*> possible_macros;
	for (std::pair<std::string, Macro*> macro_pair : m_macros) {
		if (macro_pair.second->name == macro_name && macro_pair.second->args.size() == args.size()) {
			possible_macros.push_back(macro_pair.second);
		}
	}
	return possible_macros;
}


/**
 * Skips tokens that match the given regex.
 *
 * @param regex matches tokens to skip.
 * @param tok_i the index of the current token.
 */
void Preprocessor::skip_tokens(int& tok_i, const std::string& regex) {
	while (in_bounds(tok_i) && std::regex_match(m_tokens[tok_i].value, std::regex(regex))) {
		tok_i++;
	}
}

/**
 * Skips tokens that match the given types.
 *
 * @param tok_i the index of the current token.
 * @param tok_types the types to match.
 */
void Preprocessor::skip_tokens(int& tok_i, const std::set<Tokenizer::Type>& tok_types) {
    while (in_bounds(tok_i) && tok_types.find(m_tokens[tok_i].type) != tok_types.end()) {
        tok_i++;
    }
}

/**
 * Expects the current token to exist.
 *
 * @param tok_i the index of the expected token.
 * @param error_msg the error message to throw if the token does not exist.
 */
bool Preprocessor::expect_token(int tok_i, const std::string& error_msg) {
	lgr::EXPECT_TRUE(in_bounds(tok_i), lgr::Logger::LogType::ERROR, std::stringstream(error_msg));
    return true;
}

bool Preprocessor::expect_token(int tok_i, const std::set<Tokenizer::Type>& expected_types, const std::string& error_msg) {
	lgr::EXPECT_TRUE(in_bounds(tok_i), lgr::Logger::LogType::ERROR, std::stringstream(error_msg));
	lgr::EXPECT_TRUE(expected_types.find(m_tokens[tok_i].type) != expected_types.end(), lgr::Logger::LogType::ERROR, std::stringstream(error_msg));
    return true;
}

/**
 * Returns whether the current token matches the given types.
 *
 * @param tok_i the index of the current token.
 * @param tok_types the types to match.
 *
 * @return true if the current token matches the given types.
 */
bool Preprocessor::is_token(int tok_i, const std::set<Tokenizer::Type>& tok_types, const std::string& error_msg) {
    expect_token(tok_i, error_msg);
    return tok_types.find(m_tokens[tok_i].type) != tok_types.end();
}

/**
 * Returns whether the current token index is within the bounds of the tokens list.
 *
 * @param tok_i the index of the current token
 *
 * @return true if the token index is within the bounds of the tokens list.
 */
bool Preprocessor::in_bounds(int tok_i) {
    return tok_i < m_tokens.size();
}

/**
 * Consumes the current token.
 *
 * @param tok_i the index of the current token.
 * @param error_msg the error message to throw if the token does not exist.
 *
 * @returns the value of the consumed token.
 */
Tokenizer::Token& Preprocessor::consume(int& tok_i, const std::string& error_msg) {
    expect_token(tok_i, error_msg);
    return m_tokens[tok_i++];
}

/**
 * Consumes the current token and checks it matches the given types.
 *
 * @param tok_i the index of the current token.
 * @param expected_types the expected types of the token.
 * @param error_msg the error message to throw if the token does not have the expected type.
 *
 * @returns the value of the consumed token.
 */
Tokenizer::Token& Preprocessor::consume(int& tok_i, const std::set<Tokenizer::Type>& expected_types, const std::string& error_msg) {
    expect_token(tok_i, error_msg);
	lgr::EXPECT_TRUE(expected_types.find(m_tokens[tok_i].type) != expected_types.end(), lgr::Logger::LogType::ERROR, std::stringstream() << error_msg << " - Unexpected end of file.");
    return m_tokens[tok_i++];
}


/**
 * Inserts the file contents into the current file.
 *
 * USAGE: #include "filepath"|<filepath>
 *
 * "filepath": looks for files located in the current directory.
 * <filepath>: prioritizes files located in the include directory, if not found, looks in the
 * current directory.
 *
 * @param tok_i the index of the include token.
 */
void Preprocessor::_include(int& tok_i) {
	consume(tok_i); // '#include'
	skip_tokens(tok_i, "[ \t]");

	// the path to the included file
	std::string full_path_from_working_dir;

	bool found_file = false;
    if (is_token(tok_i, {Tokenizer::LITERAL_STRING}, "Preprocessor::_include() - Missing include filename.")) {
        // local include
		std::string local_file_path = consume(tok_i).value;
		local_file_path = m_input_file.get_dir() + File::SEPARATOR + local_file_path.substr(1, local_file_path.length() - 2);
		full_path_from_working_dir = trim_dir_path(local_file_path);
		found_file = true;
    }

	if (!found_file) {
        // expect <"...">
        consume(tok_i, {Tokenizer::OPERATOR_LOGICAL_LESS_THAN}, "Preprocessor::_include() - Missing '<'.");
        std::string sys_file_path = consume(tok_i, {Tokenizer::LITERAL_STRING}, "Preprocessor::_include() - Expected string literal.").value;
		sys_file_path = sys_file_path.substr(1, sys_file_path.length() - 2);
        consume(tok_i, {Tokenizer::OPERATOR_LOGICAL_GREATER_THAN}, "Preprocessor::_include() - Missing '>'.");

        // check if file exists in system include directories
		bool found_sys_file = false;
        for (Directory dir : m_process->get_system_dirs()) {
            if (dir.subfile_exists(sys_file_path)) {
				if (found_sys_file) {
					// already found file
					lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Preprocessor::_include() - Multiple matching files found in system include directories: " << sys_file_path);
				}

                full_path_from_working_dir = dir.get_path() + File::SEPARATOR + sys_file_path;
				found_sys_file = true;
			}
        }

		if (!found_sys_file) {
			lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Preprocessor::_include() - File not found in system include directories: " << sys_file_path);
		}
	}

	// process included file
	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Preprocessor::_include() - include path: " << full_path_from_working_dir);
	File include_file = File(full_path_from_working_dir);
	lgr::EXPECT_TRUE(include_file.exists(), lgr::Logger::LogType::ERROR, std::stringstream() << "Preprocessor::_include() - Include file does not exist: " << full_path_from_working_dir);

	// instead of writing all the contents to the output file, simply tokenize the file and insert into the current token list
	Preprocessor included_preprocessor(m_process, include_file, m_output_file.get_path());

	// yoink the tokens from the included file and insert
	m_tokens.insert(m_tokens.begin() + tok_i, included_preprocessor.m_tokens.begin(), included_preprocessor.m_tokens.end());
}

/**
 * Defines a macro symbol with n arguments and optionally a return type.
 *
 * USAGE: #macro [symbol]([arg1 ?: TYPE, arg2 ?: TYPE,..., argn ?: TYPE]) ?: TYPE
 *
 * If a return type is specified and the macro definition does not return a value an error is thrown.
 * There cannot be a macro definition within this macro definition.
 * Note that the macro symbol is separate from label symbols and will not be present after preprocessing.
 *
 * @param tok_i The index of the macro token.
 */
void Preprocessor::_macro(int& tok_i) {
	consume(tok_i); // '#macro'
	skip_tokens(tok_i, "[ \t]");

	// parse macro name
	std::string macro_name = consume(tok_i, {Tokenizer::SYMBOL}, "Preprocessor::_macro() - Expected macro name.").value;
    Macro* macro = new Macro(macro_name);

    // start of invoked arguments
	skip_tokens(tok_i, "[ \t\n]");
	consume(tok_i, {Tokenizer::OPEN_PARANTHESIS}, "Preprocessor::_macro() - Expected '('.");

    // parse arguments
    while (!is_token(tok_i, {Tokenizer::CLOSE_PARANTHESIS}, "Preprocessor::_macro() - Expected macro header.")) {
        skip_tokens(tok_i, "[ \t\n]");
        std::string argName = consume(tok_i, {Tokenizer::SYMBOL}, "Preprocessor::_macro() - Expected argument name.").value;

        skip_tokens(tok_i, "[ \t\n]");
        macro->args.push_back(Argument(argName));


        // parse comma or expect closing parenthesis
        skip_tokens(tok_i, "[ \t\n]");
        if (is_token(tok_i, {Tokenizer::COMMA})) {
            consume(tok_i);
        }
    }

    // consume the closing parenthesis
    consume(tok_i, {Tokenizer::CLOSE_PARANTHESIS}, "Preprocessor::_macro() - Expected ')'.");
    skip_tokens(tok_i, "[ \t\n]");

    // parse macro definition
    skip_tokens(tok_i, "[ \t\n]");
    while (!is_token(tok_i, {Tokenizer::PREPROCESSOR_MACEND}, "Preprocessor::_macro() - Expected macro definition." )) {
        macro->definition.push_back(consume(tok_i));
    }
    consume(tok_i, {Tokenizer::PREPROCESSOR_MACEND}, "Preprocessor::_macro() - Expected '#macend'.");

    // check if macro declaration is unique
	lgr::EXPECT_TRUE(m_macros.find(macro->header()) == m_macros.end(), lgr::Logger::LogType::ERROR, std::stringstream() << "Preprocessor::_macro() - Macro already defined: " << macro->header());

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
 *
 * @param tok_i The index of the macro return token
 */
void Preprocessor::_macret(int& tok_i) {
	consume(tok_i); // '#macret'
	skip_tokens(tok_i, "[ \t]");

	std::vector<Tokenizer::Token> return_value;
	if (m_macro_stack.empty()) {
		lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Preprocessor::_macret() - Unexpected macret token.");
	}

	// macro contains a return value
	bool does_macro_return = m_macro_stack.top().second->return_type != Tokenizer::UNKNOWN;
	if (does_macro_return) {
		while (!is_token(tok_i, {Tokenizer::WHITESPACE_NEWLINE})) {
			return_value.push_back(consume(tok_i));
		}
	}

	// skip all the tokens after this till the end of the current macro's definition
	// we can achieve this by counting the number of scope levels, incrementing if we reach a .scope token and decrementing
	// if we reach a .scend token. If we reach 0, we know we have reached the end of the macro definition.

	int cur_rel_scope_level = 0;
	while (in_bounds(tok_i)) {
		if (is_token(tok_i, {Tokenizer::ASSEMBLER_SCOPE})) {
			cur_rel_scope_level++;
		} else if (is_token(tok_i, {Tokenizer::ASSEMBLER_SCEND})) {
			cur_rel_scope_level--;
		}
		consume(tok_i);

		if (cur_rel_scope_level == 0) {
			break;
		}
	}

	if (cur_rel_scope_level != 0) {
		lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Preprocessor::_macret() - Unclosed scope.");
	}

	// add'.equ current_macro_output_symbol expression' to tokens
	if (does_macro_return) {
		std::vector<Tokenizer::Token> set_return_statement;
		vector_util::append(set_return_statement, Tokenizer::tokenize(string_util::format(".equ {} ", m_macro_stack.top().first)));
		vector_util::append(set_return_statement, return_value);
		m_tokens.insert(m_tokens.begin() + tok_i, set_return_statement.begin(), set_return_statement.end());
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
 *
 * @param tok_i The index of the macro end token
 */
void Preprocessor::_macend(int& tok_i) {
    // should never reach this. This should be consumed by the _macro function.
    lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Preprocessor::_macend() - Unexpected macro end token.");
}

/**
 * Invokes the macro with the given arguments.
 *
 * USAGE: #invoke [symbol]([arg1, arg2,..., argn]) [?symbol]
 *
 * If provided an output symbol, the symbol will be associated with the return value of the macro.
 * If the macro does not return a value but an output symbol is provided, an error is thrown.
 *
 * @param tok_i The index of the invoke token
 */
void Preprocessor::_invoke(int& tok_i) {
	consume(tok_i);
	skip_tokens(tok_i, "[ \t]");

	// parse macro name
	std::string macro_name = consume(tok_i, {Tokenizer::SYMBOL}, "Preprocessor::_invoke() - Expected macro name.").value;

	// parse arguments
	skip_tokens(tok_i, "[ \t\n]");
	consume(tok_i, {Tokenizer::OPEN_PARANTHESIS}, "Preprocessor::_invoke() - Expected '('.");
	std::vector<std::vector<Tokenizer::Token>> arguments;
	while (!is_token(tok_i, {Tokenizer::CLOSE_PARANTHESIS}, "Preprocessor::_invoke() - Expected ')'.")) {
		skip_tokens(tok_i, "[ \t\n]");

		std::vector<Tokenizer::Token> argumentValues;
		while (!is_token(tok_i, {Tokenizer::COMMA, Tokenizer::CLOSE_PARANTHESIS, Tokenizer::WHITESPACE_NEWLINE}, "Preprocessor::_invoke() - Expected ')'.")) {
			argumentValues.push_back(consume(tok_i));
		}
		arguments.push_back(argumentValues);

		if (is_token(tok_i, {Tokenizer::COMMA})) {
			consume(tok_i);
		}
	}
	consume(tok_i, {Tokenizer::CLOSE_PARANTHESIS}, "Preprocessor::_invoke() - Expected ')'.");
	skip_tokens(tok_i, "[ \t\n]");

	// parse the output symbol if there is one
	bool has_output = is_token(tok_i, {Tokenizer::SYMBOL});
	std::string output_symbol = "";
	if (has_output) {
		output_symbol = consume(tok_i, {Tokenizer::SYMBOL}, "Preprocessor::_invoke() - Expected output symbol.").value;
	}

	// check if macro exists
	std::vector<Macro*> possibleMacros = macros_with_header(macro_name, arguments);
	if (possibleMacros.size() == 0) {
		lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Preprocessor::_invoke() - Macro does not exist: " << macro_name);
	} else if (possibleMacros.size() > 1) {
		lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Preprocessor::_invoke() - Multiple macros with the same name and number of arguments: " << macro_name);
	}
	Macro* macro = possibleMacros[0];

	// replace the '_invoke symbol(arg1, arg2,..., argn) ?symbol' with the macro definition
	std::vector<Tokenizer::Token> expanded_macro_invoke;

	// check if the macro returns something, if so add a equate statement to store the output
	if (has_output) {
		vector_util::append(expanded_macro_invoke, Tokenizer::tokenize(string_util::format(".equ {} 0\n", output_symbol)));
	}

	// append a new '.scope' symbol to the tokens list
	expanded_macro_invoke.push_back(Tokenizer::Token(Tokenizer::ASSEMBLER_SCOPE, ".scope"));
	expanded_macro_invoke.push_back(Tokenizer::Token(Tokenizer::WHITESPACE_NEWLINE, "\n"));

	// then for each argument, add an '.equ argname argval' statement
	for (int i = 0; i < arguments.size(); i++) {
		vector_util::append(expanded_macro_invoke, Tokenizer::tokenize(string_util::format(".equ {} ", macro->args[i].name)));
		vector_util::append(expanded_macro_invoke, arguments[i]);
	}

	// then append the macro definition
	expanded_macro_invoke.insert(expanded_macro_invoke.end(), macro->definition.begin(), macro->definition.end());

	// finally end with a '.scend' symbol
	expanded_macro_invoke.push_back(Tokenizer::Token(Tokenizer::WHITESPACE_NEWLINE, "\n"));
	expanded_macro_invoke.push_back(Tokenizer::Token(Tokenizer::ASSEMBLER_SCEND, ".scend"));

	// push the macro and output symbol if any onto the macro stack
	m_macro_stack.push(std::pair<std::string, Macro*>(output_symbol, macro));

	// print out expanded macro
	std::stringstream ss;
	for (Tokenizer::Token& token : expanded_macro_invoke) {
		ss << token.value;
	}
	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Preprocessor::_invoke() - Expanded macro: " << ss.str());

	// insert into the tokens list
	m_tokens.insert(m_tokens.begin() + tok_i, expanded_macro_invoke.begin(), expanded_macro_invoke.end());
}

/**
 * Associates the symbol with a value
 *
 * USAGE: #define [symbol] [?value]
 *
 * Replaces all instances of symbol with the value.
 * If value is not specified, the default is empty.
 *
 * @param tok_i The index of the define token.
 */
void Preprocessor::_define(int& tok_i) {
    consume(tok_i); // '#define'
    skip_tokens(tok_i, "[ \t]");

    // symbol
    std::string symbol = consume(tok_i, {Tokenizer::SYMBOL}, "Preprocessor::_define() - Expected symbol.").value;
    skip_tokens(tok_i, "[ \t]");

    // check for parameter declaration
    std::vector<std::string> parameters;
    std::set<std::string> ensure_unique_params;
    if (is_token(tok_i, {Tokenizer::OPEN_PARANTHESIS})) {
        consume(tok_i); // '('

        // parse parameters
        while (!is_token(tok_i, {Tokenizer::CLOSE_PARANTHESIS})) {
            skip_tokens(tok_i, "[ \t]");
            std::string parameter = consume(tok_i, {Tokenizer::SYMBOL}, "Preprocessor::_define() - Expected parameter.").value;

            // ensure the parameter symbol has not been used before in this definition parameters
            lgr::EXPECT_TRUE(ensure_unique_params.find(parameter) == ensure_unique_params.end(), lgr::Logger::LogType::ERROR, std::stringstream() << "Preprocessor::_define() - Duplicate parameter: " << parameter);
            parameters.push_back(parameter);
            ensure_unique_params.insert(parameter);

            // parse comma or expect closing parenthesis
            skip_tokens(tok_i, "[ \t]");
            if (is_token(tok_i, {Tokenizer::COMMA})) {
                consume(tok_i);
            } else {
                expect_token(tok_i, {Tokenizer::CLOSE_PARANTHESIS}, "Preprocessor::_define() - Expected ')'.");
            }
        }

        // expect ')'
        consume(tok_i, {Tokenizer::CLOSE_PARANTHESIS}, "Preprocessor::_define() - Expected ')'.");
        skip_tokens(tok_i, "[ \t]");
    }

    // value
    std::vector<Tokenizer::Token> tokens;
    bool read_next_line = false;
    while (!is_token(tok_i, {Tokenizer::WHITESPACE_NEWLINE}) || read_next_line) {
        read_next_line = false;
        tokens.push_back(consume(tok_i));

        // check if we should read the nextline provided the next token is a newline
        // and the previous token read was a '\' character
        if (is_token(tok_i, {Tokenizer::WHITESPACE_NEWLINE}) && tokens.back().type == Tokenizer::BACK_SLASH) {
            read_next_line = true;

            // remove the '\' character
            tokens.pop_back();
        }
    }

    // add to symbols mapping
    if (m_def_symbols.find(symbol) == m_def_symbols.end()) {
        m_def_symbols.insert(std::pair<std::string, std::map<int, Symbol>>(symbol, std::map<int, Symbol>()));
    }
    m_def_symbols.at(symbol).insert(std::pair<int, Symbol>(parameters.size(), Symbol(symbol, parameters, tokens)));
}

/**
 *
 */
void Preprocessor::cond_block(int& tok_i, bool cond_met) {
    int rel_scope_level = 0;
    int cur_tok_i = tok_i;
    int next_block_tok_i = -1;
    int end_if_tok_i = -1;
    while (in_bounds(cur_tok_i)) {
        // std::cout << rel_scope_level << " " << m_tokens[cur_tok_i].value << std::endl;

        if (rel_scope_level == 0 && is_token(cur_tok_i, {Tokenizer::PREPROCESSOR_ENDIF})) {
            if (next_block_tok_i == -1) {
                next_block_tok_i = cur_tok_i;
            }

            end_if_tok_i = cur_tok_i;
            break;
        } else if (rel_scope_level == 0 && is_token(cur_tok_i, {Tokenizer::PREPROCESSOR_ELSE, Tokenizer::PREPROCESSOR_ELSEDEF, Tokenizer::PREPROCESSOR_ELSENDEF,
                                                        Tokenizer::PREPROCESSOR_ELSEEQU, Tokenizer::PREPROCESSOR_ELSENEQU,
                                                        Tokenizer::PREPROCESSOR_ELSELESS, Tokenizer::PREPROCESSOR_ELSEMORE})) {
            if (next_block_tok_i == -1) {
                next_block_tok_i = cur_tok_i;
            }

            // start of next conditional block that should be checked if the current conditional block was
            // not entered
            if (!cond_met) {
                break;
            }
        }

        if (is_token(cur_tok_i, {Tokenizer::PREPROCESSOR_IFDEF, Tokenizer::PREPROCESSOR_IFNDEF,
                Tokenizer::PREPROCESSOR_IFEQU, Tokenizer::PREPROCESSOR_IFNEQU, Tokenizer::PREPROCESSOR_IFLESS, Tokenizer::PREPROCESSOR_IFMORE})) {
            rel_scope_level++;
        } else if (is_token(cur_tok_i, {Tokenizer::PREPROCESSOR_ENDIF})) {
            rel_scope_level--;
        }
        cur_tok_i++;
    }

    if ((cond_met && end_if_tok_i == -1) || (!cond_met && next_block_tok_i == -1)) {
        lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "condition=" << cond_met << " | endIf=" << end_if_tok_i << " | next_block_tok_i=" << next_block_tok_i);
        lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Preprocessor::cond_block() - Unclosed conditional block." );
    }

    if (cond_met) {
        lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << " | endIf=" << end_if_tok_i << " | next_block_tok_i=" << next_block_tok_i);

        if (next_block_tok_i != -1) {
            // remove all tokens from the next block to the endif
            m_tokens.erase(m_tokens.begin() + next_block_tok_i, m_tokens.begin() + end_if_tok_i);
        } else { // assert, end_if_tok_i != -1
            // don't need to do anything, because there are no linked conditional blocks after this
        }

        // m_tokens.insert(m_tokens.begin() + tok_i, Tokenizer::Token(Tokenizer::COMMENT_SINGLE_LINE, "; conditional"));
    } else {
        // move token index to the start of the next conditional block (or endif)
        tok_i = next_block_tok_i;
    }
}

/**
 *
 */
bool Preprocessor::is_symbol_def(std::string symbol_name, int num_params) {
    return m_def_symbols.find(symbol_name) != m_def_symbols.end() && m_def_symbols.at(symbol_name).find(num_params) != m_def_symbols.at(symbol_name).end();
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
 *
 * @param tok_i The index of the conditional token.
 */
void Preprocessor::_cond_on_def(int& tok_i) {
    Tokenizer::Token cond_tok = consume(tok_i);
    skip_tokens(tok_i, "[ \t]");

    // symbol
    std::string symbol = consume(tok_i, {Tokenizer::SYMBOL}, "Preprocessor::_" + cond_tok.value.substr(1) + "() - Expected symbol.").value;
    skip_tokens(tok_i, "[ \t]");

    if (cond_tok.type == Tokenizer::PREPROCESSOR_IFDEF || cond_tok.type == Tokenizer::PREPROCESSOR_ELSEDEF) {
        cond_block(tok_i, is_symbol_def(symbol, 0));
    } else if (cond_tok.type == Tokenizer::PREPROCESSOR_IFNDEF || cond_tok.type == Tokenizer::PREPROCESSOR_ELSENDEF) {
        cond_block(tok_i, !is_symbol_def(symbol, 0));
    } else {
        lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Preprocessor::_cond_on_def() - Unexpected conditional token: " << cond_tok.value);
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
 *
 * @param tok_i The index of the conditional token.
 */
void Preprocessor::_cond_on_value(int& tok_i) {
    Tokenizer::Token cond_tok = consume(tok_i);
    skip_tokens(tok_i, "[ \t]");

    // symbol
    std::string symbol = consume(tok_i, {Tokenizer::SYMBOL}, "Preprocessor::_" + cond_tok.value.substr(1) + "() - Expected symbol.").value;
    skip_tokens(tok_i, "[ \t]");

    // extract symbol's string value
    std::string symbol_val;
    if (is_symbol_def(symbol, 0)) {
        for (Tokenizer::Token& token : m_def_symbols.at(symbol).at(0).value) {
            symbol_val += token.value;
        }
    }

    // value
    std::string value;
    bool read_next_line = false;
    while (!is_token(tok_i, {Tokenizer::WHITESPACE_NEWLINE}) || read_next_line) {
        read_next_line = false;
        value += consume(tok_i).value;

        // check if we should read the nextline provided the next token is a newline
        // and the previous token read was a '\' character
        if (is_token(tok_i, {Tokenizer::WHITESPACE_NEWLINE}) && value.back() == '\\') {
            read_next_line = true;

            // remove the '\' character
            value.pop_back();
        }
    }

    if (cond_tok.type == Tokenizer::PREPROCESSOR_IFEQU || cond_tok.type == Tokenizer::PREPROCESSOR_ELSEEQU) {
        cond_block(tok_i, value == symbol_val);
    } else if (cond_tok.type == Tokenizer::PREPROCESSOR_IFNEQU || cond_tok.type == Tokenizer::PREPROCESSOR_ELSENEQU) {
        cond_block(tok_i, value != symbol_val);
    } else if (cond_tok.type == Tokenizer::PREPROCESSOR_IFLESS || cond_tok.type == Tokenizer::PREPROCESSOR_ELSELESS) {
        cond_block(tok_i, symbol_val < value);
    } else if (cond_tok.type == Tokenizer::PREPROCESSOR_IFMORE || cond_tok.type == Tokenizer::PREPROCESSOR_ELSEMORE) {
        cond_block(tok_i, symbol_val > value);
    } else {
        lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Preprocessor::_cond_on_value() - Unexpected conditional token: " << cond_tok.value);
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
 *
 * @param tok_i The index of the else token.
 */
void Preprocessor::_else(int& tok_i) {
    consume(tok_i); // '#else'
    skip_tokens(tok_i, "[ \t]");
}

/**
 * Closes a #ifdef, #ifndef, #else, #elsedef, or #elsendef.
 *
 * USAGE: #endif
 *
 * Must be preceded by a #ifdef, #ifndef, #else, #elsedef, or #elsendef.
 *
 * @param tok_i The index of the endif token.
 */
void Preprocessor::_endif(int& tok_i) {
    consume(tok_i); // '#endif'
    skip_tokens(tok_i, "[ \t]");
}

/**
 * Undefines a symbol defined by #define.
 *
 * USAGE: #undefine [symbol]
 *
 * This will still work if the symbol was never defined previously.
 *
 * @param tok_i The index of the undefine token.
 */
void Preprocessor::_undefine(int& tok_i) {
    consume(tok_i); // '#define'
    skip_tokens(tok_i, "[ \t]");

    // symbol
    std::string symbol = consume(tok_i, {Tokenizer::SYMBOL}, "Preprocessor::_define() - Expected symbol.").value;
    skip_tokens(tok_i, "[ \t]");

    // if a number of parameters was specified, remove that definition otherwise remove all definitions
    if (is_token(tok_i, {Tokenizer::LITERAL_NUMBER_DECIMAL})) {
        int num_params = std::stoi(consume(tok_i, {Tokenizer::LITERAL_NUMBER_DECIMAL}, "Preprocessor::_undefine() - Expected number of parameters.").value);
        m_def_symbols[symbol].erase(num_params);
    } else {
        m_def_symbols.erase(symbol);
    }
}

/**
 * Returns the state of the preprocessor.
 *
 * @return the state of the preprocessor.
 */
Preprocessor::State Preprocessor::get_state() {
	return m_state;
}