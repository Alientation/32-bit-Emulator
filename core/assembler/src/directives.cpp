#include "assembler/assembler.h"
#include "util/logger.h"

#include <string>

/**
 * @brief
 * @todo				Implement full expression parser
 *
 * @param 				tok_i: Reference to current token index
 * @return 				value of expression
 */
dword Assembler::parse_expression(size_t& tok_i)
{
	/* For now, only parse expressions sequentially, without care of precedence */
	dword exp_value = 0;
	skip_tokens(tok_i, "[ \t]");
	Tokenizer::Token *operator_token = nullptr;
	do {
		Tokenizer::Token token = consume(tok_i);

		dword value = 0;
		if (token.type == Tokenizer::LITERAL_NUMBER_DECIMAL) {
			value = std::stoull(token.value);
		} else if (token.type == Tokenizer::LITERAL_NUMBER_HEXADECIMAL) {
			value = std::stoull(token.value.substr(1), nullptr, 16);
		} else if (token.type == Tokenizer::LITERAL_NUMBER_BINARY) {
			value = std::stoull(token.value.substr(1), nullptr, 2);
		} else if (token.type == Tokenizer::LITERAL_NUMBER_OCTAL) {
			value = std::stoull(token.value.substr(1), nullptr, 8);
		}

		if (operator_token != nullptr) {
			switch(operator_token->type) {
				case Tokenizer::OPERATOR_ADDITION:
					exp_value += value;
					break;
				case Tokenizer::OPERATOR_SUBTRACTION:
					exp_value -= value;
					break;
				case Tokenizer::OPERATOR_DIVISION:
					exp_value /= value;
					break;
				case Tokenizer::OPERATOR_MULTIPLICATION:
					exp_value *= value;
					break;
				default:
					ERROR("Expected operator token but got %s.", operator_token->value.c_str());
			}
			operator_token = nullptr;
		} else {
			exp_value = value;
		}
		skip_tokens(tok_i, "[ \t]");

		/* Temporary only support 4 operations */
		if (is_token(tok_i, {Tokenizer::OPERATOR_ADDITION, Tokenizer::OPERATOR_DIVISION,
				Tokenizer::OPERATOR_MULTIPLICATION, Tokenizer::OPERATOR_SUBTRACTION})) {
			operator_token = &consume(tok_i);
		} else {
			break;
		}
	} while(!is_token(tok_i, {Tokenizer::WHITESPACE_NEWLINE, Tokenizer::COMMA}));

	return exp_value;
}

/**
 * @brief 				Declares a symbol to be global outside this compilation unit.
 * 						Must be declared outside any defined sections like .text, .bss, and .data.
 * USAGE:				.global <symbol>
 *
 * @param 					tok_i: reference to current token index
 */
void Assembler::_global(size_t& tok_i)
{
	if (current_section != Section::NONE) {
		ERROR("Assembler::_global() - Cannot declare symbol as global "
				"inside a section. Must be declared outside of .text, .bss, and .data.");
		m_state = State::ASSEMBLER_ERROR;
		return;
	}

	consume(tok_i);
	skip_tokens(tok_i, Tokenizer::WHITESPACES);

	std::string symbol = consume(tok_i).value;
	m_obj.add_symbol(symbol, 0, ObjectFile::SymbolTableEntry::BindingInfo::GLOBAL, -1);
}

/**
 * @brief				Declares a symbol to exist in another compilation unit but not defined here.
 * 						Symbol's binding info will be marked as weak.
 * 						Must be declared outside any defined sections like .text, .bss, and .data.
 * USAGE:				.extern <symbol>
 *
 * @param 				tok_i: reference to current token index
 */
void Assembler::_extern(size_t& tok_i)
{
	if (current_section != Section::NONE) {
		ERROR("Assembler::_extern() - Cannot "
				"declare symbol as extern inside a section. Must be declared outside of "
				".text, .bss, and .data.");
		m_state = State::ASSEMBLER_ERROR;
		return;
	}

	consume(tok_i);
	skip_tokens(tok_i, Tokenizer::WHITESPACES);

	std::string symbol = consume(tok_i).value;
	m_obj.add_symbol(symbol, 0, ObjectFile::SymbolTableEntry::BindingInfo::WEAK, -1);
}

/**
 * @brief 				Moves where the assembler is in a section. Can only move forward, not backward.
 * USAGE:				.org <expression>
 *
 * @param 				tok_i: reference to current token index
 */
void Assembler::_org(size_t& tok_i)
{
	consume(tok_i);
	skip_tokens(tok_i, Tokenizer::WHITESPACES);

	word val = parse_expression(tok_i);

	if (val >= 0xffffff) {											/* Safety exit. Likely unintentional behavior */
		WARN("Assembler::_org() - new value is large and likely unintentional. (%d).", val);
		m_state = State::ASSEMBLER_WARNING;
		return;
	}

	switch (current_section) {
		case Section::BSS:
			if (val < m_obj.bss_section) {
				ERROR("Assembler::_org() - .org directive cannot move "
						"assembler pc backwards. Expected >= %u. Got %u.", m_obj.bss_section, val);
				m_state = State::ASSEMBLER_ERROR;
				return;
			}
			m_obj.bss_section = val;
			break;
		case Section::DATA:
			if (val < m_obj.data_section.size()) {
				ERROR("Assembler::_org() - .org directive cannot move "
						"assembler pc backwards. Expected >= %llu. Got %u.", m_obj.data_section.size(), val);
				m_state = State::ASSEMBLER_ERROR;
				return;
			}
			for (size_t i = m_obj.data_section.size(); i < val; i++) {
				m_obj.data_section.push_back(0);
			}
			break;
		case Section::TEXT:						/*
													It is likely not very useful to allow .org to
													move pc in a text section, comparatively to .data
													and .bss
												*/
			if (val < m_obj.text_section.size() * 4) {
				ERROR("Assembler::_org() - .org directive cannot move "
						"assembler pc backwards. Expected >= %llu. Got %u.",
						m_obj.text_section.size() * 4, val);
				m_state = State::ASSEMBLER_ERROR;
				return;
			}

			if (val % 4 != 0) {
				ERROR("Assembler::_org() - .org directive cannot move "
						"assembler pc to a non-word aligned byte in .text section. Expected aligned "
						"4 byte. Got %u.", val);
				m_state = State::ASSEMBLER_ERROR;
				return;
			}

			for (size_t i = m_obj.text_section.size() * 4; i < val; i += 4) {
				m_obj.text_section.push_back(0);
			}
			break;
		case Section::NONE:
			ERROR("Assembler::_org() - Not defined inside section. Cannot move section pointer.");
			m_state = State::ASSEMBLER_ERROR;
			return;
	}
}

/**
 * @brief 				Defines a local scope. Any symbol defined inside will be marked as local and will not be able to be marked as global.
 * 						Symbols defined here will be postfixed with a special identifier <symbol>:<scope_id>. Local symbols defined at
 * 						current scope level or above will have higher precedence over globally defined symbols.
 * USAGE:				.scope
 *
 * @param 				tok_i: Reference to current token index
 */
void Assembler::_scope(size_t& tok_i)
{
	consume(tok_i);
	scopes.push_back(total_scopes++);
}

/**
 * @brief 				Ends a local scope.
 * USAGE:				.scend
 *
 * @param 				tok_i: Reference to current token index
 */
void Assembler::_scend(size_t& tok_i)
{
	if (scopes.empty()) {
		ERROR("Assembler::_scend() - .scend directive must have a matching .scope directive.");
		m_state = State::ASSEMBLER_ERROR;
		return;
	}

	scopes.pop_back();
	consume(tok_i);
}

/**
 * @brief 				Moves where the assembler is in a section forward by a certain amount of bytes.
 * USAGE:				.advance <expression>
 *
 * @param 				tok_i: Reference to current token index
 */
void Assembler::_advance(size_t& tok_i)
{
	consume(tok_i);
	skip_tokens(tok_i, Tokenizer::WHITESPACES);

	word val = parse_expression(tok_i);

	if (val >= 0xffffff) {									/* Safety exit. Likely unintentional behavior */
		WARN("Assembler::_advance() - offset value is large and likely unintentional. (%u).", val);
		m_state = State::ASSEMBLER_WARNING;
		return;
	}

	switch (current_section) {
		case Section::BSS:
			m_obj.bss_section += val;
			break;
		case Section::DATA:
			for (word i = 0; i < val; i++) {
				m_obj.data_section.push_back(0);
			}
			break;
		case Section::TEXT:									/*
																It is likely not very useful to allow .org to
																move pc in a text section, comparatively to .data and .bss
															*/
			if (val % 4 != 0) {
				ERROR("Assembler::_advance() - .advance directive cannot"
						" move assembler pc to a non-word aligned byte in .text section. Expected aligned 4 byte."
						" Got %u.", val);
				m_state = State::ASSEMBLER_ERROR;
				return;
			}

			for (word i = 0; i < val; i += 4) {
				m_obj.text_section.push_back(0);
			}
			break;
		case Section::NONE:
			ERROR("Assembler::_advance() - Not defined inside section. Cannot move section pointer.");
			m_state = State::ASSEMBLER_ERROR;
			return;
	}
}

/**
 * @brief 				Aligns where the assembler is in the current section.
 * @note				This is useless unless we can specify in the program header of the object file the alignment of the whole program
 * USAGE:				.align <expression>
 *
 * @param 				tok_i: Reference to the current token index
 */
void Assembler::_align(size_t& tok_i)
{
	consume(tok_i);
	skip_tokens(tok_i, Tokenizer::WHITESPACES);

	word val = parse_expression(tok_i);
	if (val >= 0xffff) {											/* Safety exit. Likely unintentional behavior */
		WARN("Assembler::_align() - Alignment value is large and likely unintentional. (%u).", val);
		m_state = State::ASSEMBLER_WARNING;
		return;
	}

	switch (current_section) {
		case Section::BSS:
			m_obj.bss_section += (val - (m_obj.bss_section%val)) % val;
			break;
		case Section::DATA:
			while (m_obj.data_section.size() % val != 0) {
				m_obj.data_section.push_back(0);
			}
			break;
		case Section::TEXT:											/*
																		It is likely not very useful to allow
																		.org to move pc in a text section,
																		comparatively to .data and .bss
																	*/
			if (val % 4 != 0) {
				ERROR("Assembler::_advance() - .advance directive cannot "
						"move assembler pc to a non-word aligned byte in .text section. Expected aligned 4 byte."
						" Got %u.", val);
				m_state = State::ASSEMBLER_ERROR;
				return;
			}

			while (m_obj.text_section.size() * 4 % val != 0) {
				m_obj.text_section.push_back(0);
			}
			break;
		case Section::NONE:
			ERROR("Assembler::_align() - Not defined inside a section. Cannot align section pointer.");
			m_state = State::ASSEMBLER_ERROR;
			return;
	}
}

/**
 * @brief 				Creates a new section.
 * @warning				Not implemented yet.
 * USAGE:				.section <string>, <flags>
 *
 * @param 				tok_i: Reference to the current token index
 */
void Assembler::_section(size_t& tok_i)
{
	consume(tok_i);
	skip_tokens(tok_i, Tokenizer::WHITESPACES);

	expect_token(tok_i, {Tokenizer::LITERAL_STRING}, "Assembler::_section() - .section expects a "
			"string argument to follow.");

	ERROR("Assembler::_section() - .section directive is not implemented yet.");
	m_state = State::ASSEMBLER_ERROR;
	return;
}

/**
 * @brief				Creates a new text section.
 * @warning				Currently will simply add on to the previously defined text section if it exists.
 * USAGE:				.text
 *
 * @param 				tok_i: Reference to the current token index
 */
void Assembler::_text(size_t& tok_i)
{
	consume(tok_i);

	current_section = Section::TEXT;
	current_section_index = m_obj.section_table[".text"];
}

/**
 * @brief				Creates a new data section.
 * @warning				Currently will simply add on to the previously defined data section if it exists
 * USAGE:				.data
 *
 * @param 				tok_i: Reference to the current token index
 */
void Assembler::_data(size_t& tok_i)
{
	consume(tok_i);

	current_section = Section::DATA;
	current_section_index = m_obj.section_table[".data"];
}

/**
 * @brief				Creates a new bss section.
 * @warning				Currently will simply add on to the previously defined bss section if it exists
 * USAGE:				.bss
 *
 * @param 				tok_i: Reference to the current token index
 */
void Assembler::_bss(size_t& tok_i)
{
	consume(tok_i);

	current_section = Section::BSS;
	current_section_index = m_obj.section_table[".bss"];
}

/**
 * @brief 				Stops assembling
 * USAGE:				.stop
 *
 * @param 				tok_i: Reference to the current token index
 */
void Assembler::_stop(size_t& tok_i)
{
	tok_i = m_tokens.size();
}


std::vector<dword> Assembler::parse_arguments(size_t& tok_i) {
	skip_tokens(tok_i, "[ \t]");

	std::vector<dword> args;
	while (!is_token(tok_i, {Tokenizer::WHITESPACE_NEWLINE})) {
		args.push_back(parse_expression(tok_i));
		skip_tokens(tok_i, "[ \t]");
		if (is_token(tok_i, {Tokenizer::COMMA})) {
			consume(tok_i);
			skip_tokens(tok_i, "[ \t]");
		} else {
			break;
		}
	}
	return args;
}

std::vector<byte> convert_little_endian(std::vector<dword> data, int n_bytes)
{
	std::vector<byte> little_endian_data;

	for (size_t i = 0; i < data.size(); i++) {
		for (int j = 0; j < n_bytes; j++) {
			little_endian_data.push_back(data.at(i) & 0xFF);
			data.at(i) >>= 8;
		}
	}

	return little_endian_data;
}

void Assembler::_byte(size_t& tok_i)
{
	EXPECT_TRUE_SS(current_section == Section::DATA, std::stringstream()
			<< "Assembler::_byte() - Can only define data in .data section.");

	consume(tok_i);

	std::vector<byte> data = convert_little_endian(parse_arguments(tok_i), 1);
	for (size_t i = 0; i < data.size(); i++) {
		m_obj.data_section.push_back(data.at(i));
	}
}

void Assembler::_dbyte(size_t& tok_i)
{
	EXPECT_TRUE_SS(current_section == Section::DATA, std::stringstream()
			<< "Assembler::_dbyte() - Can only define data in .data section.");

	consume(tok_i);

	std::vector<byte> data = convert_little_endian(parse_arguments(tok_i), 2);
	for (size_t i = 0; i < data.size(); i++) {
		m_obj.data_section.push_back(data.at(i));
	}
}

void Assembler::_word(size_t& tok_i)
{
	EXPECT_TRUE_SS(current_section == Section::DATA, std::stringstream()
			<< "Assembler::_word() - Can only define data in .data section.");

	consume(tok_i);

	std::vector<byte> data = convert_little_endian(parse_arguments(tok_i), 4);
	for (size_t i = 0; i < data.size(); i++) {
		m_obj.data_section.push_back(data.at(i));
	}
}

void Assembler::_dword(size_t& tok_i)
{
	EXPECT_TRUE_SS(current_section == Section::DATA, std::stringstream()
			<< "Assembler::_dword() - Can only define data in .data section.");

	consume(tok_i);

	std::vector<byte> data = convert_little_endian(parse_arguments(tok_i), 8);
	for (size_t i = 0; i < data.size(); i++) {
		m_obj.data_section.push_back(data.at(i));
	}
}

/* this is pointless, same as .byte */
void Assembler::_sbyte(size_t& tok_i)
{
	EXPECT_TRUE_SS(current_section == Section::DATA, std::stringstream()
			<< "Assembler::_sbyte() - Can only define data in .data section.");

	consume(tok_i);

	std::vector<byte> data = convert_little_endian(parse_arguments(tok_i), 1);
	for (size_t i = 0; i < data.size(); i++) {
		m_obj.data_section.push_back(data.at(i));
	}
}

/* todo, figure out why signed versions of these data defining directives are needed */
void Assembler::_sdbyte(size_t& tok_i)
{
	EXPECT_TRUE_SS(current_section == Section::DATA, std::stringstream()
			<< "Assembler::_sdbyte() - Can only define data in .data section.");

	consume(tok_i);

	std::vector<byte> data = convert_little_endian(parse_arguments(tok_i), 2);
	for (size_t i = 0; i < data.size(); i++) {
		m_obj.data_section.push_back(data.at(i));
	}
}

void Assembler::_sword(size_t& tok_i)
{
	EXPECT_TRUE_SS(current_section == Section::DATA, std::stringstream()
			<< "Assembler::_sword() - Can only define data in .data section.");

	consume(tok_i);

	std::vector<byte> data = convert_little_endian(parse_arguments(tok_i), 4);
	for (size_t i = 0; i < data.size(); i++) {
		m_obj.data_section.push_back(data.at(i));
	}
}

void Assembler::_sdword(size_t& tok_i)
{
	EXPECT_TRUE_SS(current_section == Section::DATA, std::stringstream()
			<< "Assembler::_sdword() - Can only define data in .data section.");

	consume(tok_i);

	std::vector<byte> data = convert_little_endian(parse_arguments(tok_i), 8);
	for (size_t i = 0; i < data.size(); i++) {
		m_obj.data_section.push_back(data.at(i));
	}
}

void Assembler::_char(size_t& tok_i)
{
	EXPECT_TRUE_SS(current_section == Section::DATA, std::stringstream()
			<< "Assembler::_char() - Can only define data in .data section.");
	consume(tok_i);

	skip_tokens(tok_i, "[ \t]");

	while (!is_token(tok_i, {Tokenizer::WHITESPACE_NEWLINE})) {
		expect_token(tok_i, {Tokenizer::Type::LITERAL_CHAR}, "Assembler::_char() - Expected literal"
				" char. Got " + m_tokens.at(tok_i).value);
		m_obj.data_section.push_back(consume(tok_i).value.at(1));
		skip_tokens(tok_i, "[ \t]");
		if (is_token(tok_i, {Tokenizer::COMMA})) {
			consume(tok_i);
			skip_tokens(tok_i, "[ \t]");
		} else {
			break;
		}
	}
}

void Assembler::_ascii(size_t& tok_i)
{
	EXPECT_TRUE_SS(current_section == Section::DATA, std::stringstream()
			<< "Assembler::_ascii() - Can only define data in .data section.");
	consume(tok_i);

	skip_tokens(tok_i, "[ \t]");

	while (!is_token(tok_i, {Tokenizer::WHITESPACE_NEWLINE})) {
		expect_token(tok_i, {Tokenizer::Type::LITERAL_STRING}, "Assembler::_ascii() - Expected "
				"literal string. Got " + m_tokens.at(tok_i).value);

		std::string str = consume(tok_i).value;
		for (size_t i = 1; i < str.size() - 1; i++) {
			m_obj.data_section.push_back(str[i]);
		}
		m_obj.data_section.push_back('\0');

		skip_tokens(tok_i, "[ \t]");
		if (is_token(tok_i, {Tokenizer::COMMA})) {
			consume(tok_i);
			skip_tokens(tok_i, "[ \t]");
		} else {
			break;
		}
	}
}

void Assembler::_asciz(size_t& tok_i)
{
	EXPECT_TRUE_SS(current_section == Section::DATA, std::stringstream()
			<< "Assembler::_asciz() - Can only define data in .data section.");
	consume(tok_i);

	skip_tokens(tok_i, "[ \t]");

	while (!is_token(tok_i, {Tokenizer::WHITESPACE_NEWLINE})) {
		expect_token(tok_i, {Tokenizer::Type::LITERAL_STRING}, "Assembler::_ascii() - Expected "
				"literal string. Got " + m_tokens.at(tok_i).value);

		std::string str = consume(tok_i).value;
		for (size_t i = 1; i < str.size() - 1; i++) {
			m_obj.data_section.push_back(str[i]);
		}
		m_obj.data_section.push_back('\0');

		skip_tokens(tok_i, "[ \t]");
		if (is_token(tok_i, {Tokenizer::COMMA})) {
			consume(tok_i);
			skip_tokens(tok_i, "[ \t]");
		} else {
			break;
		}
	}
}
