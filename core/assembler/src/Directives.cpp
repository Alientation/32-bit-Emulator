#include "assembler/Assembler.h"
#include "util/Logger.h"

#include <string>

/**
 * @brief
 * @todo					Implement full expression parser
 *
 * @param 					tokenI: Reference to current token index
 * @return 					value of expression
 */
dword Assembler::parse_expression(int& tokenI) {
	/* For now, only parse expressions sequentially, without care of precedence */
	dword exp_value = 0;
	skipTokens(tokenI, "[ \t]");
	Tokenizer::Token *operator_token = nullptr;
	do {
		Tokenizer::Token token = consume(tokenI);

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
			}
			operator_token = nullptr;
		} else {
			exp_value = value;
		}
		skipTokens(tokenI, "[ \t]");

		/* Temporary only support 4 operations */
		if (isToken(tokenI, {Tokenizer::OPERATOR_ADDITION, Tokenizer::OPERATOR_DIVISION, Tokenizer::OPERATOR_MULTIPLICATION, Tokenizer::OPERATOR_SUBTRACTION})) {
			operator_token = &consume(tokenI);
		} else {
			break;
		}
	} while(!isToken(tokenI, {Tokenizer::WHITESPACE_NEWLINE, Tokenizer::COMMA}));

	return exp_value;
}

/**
 * @brief 					Declares a symbol to be global outside this compilation unit. Must be declared outside any defined sections like .text, .bss, and .data.
 * USAGE:					.global <symbol>
 *
 * @param 					tokenI: reference to current token index
 */
void Assembler::_global(int& tokenI) {
	if (current_section != Section::NONE) {
		lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Assembler::_global() - Cannot declare symbol as global inside a section. Must be declared outside of .text, .bss, and .data.");
		m_state = State::ASSEMBLER_ERROR;
		return;
	}

	consume(tokenI);
	skipTokens(tokenI, Tokenizer::WHITESPACES);

	std::string symbol = consume(tokenI).value;
	m_obj.add_symbol(symbol, 0, ObjectFile::SymbolTableEntry::BindingInfo::GLOBAL, -1);
}

/**
 * @brief					Declares a symbol to exist in another compilation unit but not defined here. Symbol's binding info will be marked as weak.
 * 								Must be declared outside any defined sections like .text, .bss, and .data.
 * USAGE:					.extern <symbol>
 *
 * @param 					tokenI: reference to current token index
 */
void Assembler::_extern(int& tokenI) {
	if (current_section != Section::NONE) {
		lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Assembler::_extern() - Cannot declare symbol as extern inside a section. Must be declared outside of .text, .bss, and .data.");
		m_state = State::ASSEMBLER_ERROR;
		return;
	}

	consume(tokenI);
	skipTokens(tokenI, Tokenizer::WHITESPACES);

	std::string symbol = consume(tokenI).value;
	m_obj.add_symbol(symbol, 0, ObjectFile::SymbolTableEntry::BindingInfo::WEAK, -1);
}

/**
 * @brief 					Moves where the assembler is in a section. Can only move forward, not backward.
 * USAGE:					.org <expression>
 *
 * @param 					tokenI: reference to current token index
 */
void Assembler::_org(int& tokenI) {
	if (current_section == Section::NONE) {
		lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Assembler::_org() - Not defined inside section. Cannot move section pointer.");
		m_state = State::ASSEMBLER_ERROR;
		return;
	}

	consume(tokenI);
	skipTokens(tokenI, Tokenizer::WHITESPACES);

	word val = parse_expression(tokenI);

	if (val >= 0xffffff) {											/*! Safety exit. Likely unintentional behavior */
		lgr::log(lgr::Logger::LogType::WARN, std::stringstream() << "Assembler::_org() - new value is large and likely unintentional. (" << std::to_string(val) << ")");
		m_state = State::ASSEMBLER_WARNING;
		return;
	}

	switch (current_section) {
		case Section::BSS:
			if (val < m_obj.bss_section) {
				lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Assembler::_org() - .org directive cannot move assembler pc backwards. Expected >= "
						<< std::to_string(m_obj.bss_section) << ". Got " << val << ".");
				m_state = State::ASSEMBLER_ERROR;
				return;
			}
			m_obj.bss_section = val;
			break;
		case Section::DATA:
			if (val < m_obj.data_section.size()) {
				lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Assembler::_org() - .org directive cannot move assembler pc backwards. Expected >= "
						<< std::to_string(m_obj.data_section.size()) << ". Got " << val << ".");
				m_state = State::ASSEMBLER_ERROR;
				return;
			}
			for (int i = m_obj.data_section.size(); i < val; i++) {
				m_obj.data_section.push_back(0);
			}
			break;
		case Section::TEXT:											/* It is likely not very useful to allow .org to move pc in a text section, comparatively to .data and .bss */
			if (val < m_obj.text_section.size() * 4) {
				lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Assembler::_org() - .org directive cannot move assembler pc backwards. Expected >= "
						<< std::to_string(m_obj.text_section.size() * 4) << ". Got " << val << ".");
				m_state = State::ASSEMBLER_ERROR;
				return;
			}

			if (val % 4 != 0) {
				lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Assembler::_org() - .org directive cannot move assembler pc to a non-word aligned byte in .text section. Expected aligned 4 byte."
						<< " Got " << val << ".");
				m_state = State::ASSEMBLER_ERROR;
				return;
			}

			for (int i = m_obj.text_section.size() * 4; i < val; i += 4) {
				m_obj.text_section.push_back(0);
			}
			break;
	}
}

/**
 * @brief 					Defines a local scope. Any symbol defined inside will be marked as local and will not be able to be marked as global.
 * 								Symbols defined here will be postfixed with a special identifier <symbol>:<scope_id>. Local symbols defined at
 * 								current scope level or above will have higher precedence over globally defined symbols.
 * USAGE:					.scope
 *
 * @param 					tokenI: Reference to current token index
 */
void Assembler::_scope(int& tokenI) {
	consume(tokenI);
	scopes.push_back(total_scopes++);
}

/**
 * @brief 					Ends a local scope.
 * USAGE:					.scend
 *
 * @param 					tokenI: Reference to current token index
 */
void Assembler::_scend(int& tokenI) {
	if (scopes.empty()) {
		lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Assembler::_scend() - .scend directive must have a matching .scope directive.");
		m_state = State::ASSEMBLER_ERROR;
		return;
	}

	scopes.pop_back();
	consume(tokenI);
}

/**
 * @brief 					Moves where the assembler is in a section forward by a certain amount of bytes.
 * USAGE:					.advance <expression>
 *
 * @param 					tokenI: Reference to current token index
 */
void Assembler::_advance(int& tokenI) {
	if (current_section == Section::NONE) {
		lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Assembler::_advance() - Not defined inside section. Cannot move section pointer.");
		m_state = State::ASSEMBLER_ERROR;
		return;
	}

	consume(tokenI);
	skipTokens(tokenI, Tokenizer::WHITESPACES);

	word val = parse_expression(tokenI);

	if (val >= 0xffffff) {											/*! Safety exit. Likely unintentional behavior */
		lgr::log(lgr::Logger::LogType::WARN, std::stringstream() << "Assembler::_advance() - offset value is large and likely unintentional. (" << std::to_string(val) << ")");
		m_state = State::ASSEMBLER_WARNING;
		return;
	}

	switch (current_section) {
		case Section::BSS:
			m_obj.bss_section += val;
			break;
		case Section::DATA:
			for (int i = 0; i < val; i++) {
				m_obj.data_section.push_back(0);
			}
			break;
		case Section::TEXT:											/* It is likely not very useful to allow .org to move pc in a text section, comparatively to .data and .bss */
			if (val % 4 != 0) {
				lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Assembler::_advance() - .advance directive cannot move assembler pc to a non-word aligned byte in .text section. Expected aligned 4 byte."
						<< " Got " << val << ".");
				m_state = State::ASSEMBLER_ERROR;
				return;
			}

			for (int i = 0; i < val; i += 4) {
				m_obj.text_section.push_back(0);
			}
			break;
	}
}

/**
 * @brief 					Aligns where the assembler is in the current section.
 * @note					This is useless unless we can specify in the program header of the object file the alignment of the whole program
 * USAGE:					.align <expression>
 *
 * @param 					tokenI: Reference to the current token index
 */
void Assembler::_align(int& tokenI) {
	if (current_section == Section::NONE) {
		lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Assembler::_align() - Not defined inside a section. Cannot align section pointer.");
		m_state = State::ASSEMBLER_ERROR;
		return;
	}

	consume(tokenI);
	skipTokens(tokenI, Tokenizer::WHITESPACES);

	word val = parse_expression(tokenI);
	if (val >= 0xffff) {											/*! Safety exit. Likely unintentional behavior */
		lgr::log(lgr::Logger::LogType::WARN, std::stringstream() << "Assembler::_align() - Alignment value is large and likely unintentional. (" << std::to_string(val) << ")");
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
		case Section::TEXT:											/*! It is likely not very useful to allow .org to move pc in a text section, comparatively to .data and .bss */
			if (val % 4 != 0) {
				lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Assembler::_advance() - .advance directive cannot move assembler pc to a non-word aligned byte in .text section. Expected aligned 4 byte."
						<< " Got " << val << ".");
				m_state = State::ASSEMBLER_ERROR;
				return;
			}

			while (m_obj.text_section.size() * 4 % val != 0) {
				m_obj.text_section.push_back(0);
			}
			break;
	}
}

/**
 * @brief 					Creates a new section.
 * @warning					Not implemented yet.
 * USAGE:					.section <string>, <flags>
 *
 * @param 					tokenI: Reference to the current token index
 */
void Assembler::_section(int& tokenI) {
	consume(tokenI);
	skipTokens(tokenI, Tokenizer::WHITESPACES);

	expectToken(tokenI, {Tokenizer::LITERAL_STRING}, "Assembler::_section() - .section expects a string argument to follow.");

	lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Assembler::_section() - .section directive is not implemented yet.");
	m_state = State::ASSEMBLER_ERROR;
	return;
}

/**
 * @brief					Creates a new text section.
 * @warning					Currently will simply add on to the previously defined text section if it exists.
 * USAGE:					.text
 *
 * @param 					tokenI: Reference to the current token index
 */
void Assembler::_text(int& tokenI) {
	consume(tokenI);

	current_section = Section::TEXT;
	current_section_index = m_obj.section_table[".text"];
}

/**
 * @brief					Creates a new data section.
 * @warning					Currently will simply add on to the previously defined data section if it exists
 * USAGE:					.data
 *
 * @param 					tokenI: Reference to the current token index
 */
void Assembler::_data(int& tokenI) {
	consume(tokenI);

	current_section = Section::DATA;
	current_section_index = m_obj.section_table[".data"];
}

/**
 * @brief					Creates a new bss section.
 * @warning					Currently will simply add on to the previously defined bss section if it exists
 * USAGE:					.bss
 *
 * @param 					tokenI: Reference to the current token index
 */
void Assembler::_bss(int& tokenI) {
	consume(tokenI);

	current_section = Section::BSS;
	current_section_index = m_obj.section_table[".bss"];
}

/**
 * @brief 					Stops assembling
 * USAGE:					.stop
 *
 * @param 					tokenI: Reference to the current token index
 */
void Assembler::_stop(int& tokenI) {
	tokenI = m_tokens.size();
}


std::vector<dword> Assembler::parse_arguments(int& tokenI) {
	skipTokens(tokenI, "[ \t]");

	std::vector<dword> args;
	while (!isToken(tokenI, {Tokenizer::WHITESPACE_NEWLINE})) {
		args.push_back(parse_expression(tokenI));
		skipTokens(tokenI, "[ \t]");
		if (isToken(tokenI, {Tokenizer::COMMA})) {
			consume(tokenI);
			skipTokens(tokenI, "[ \t]");
		} else {
			break;
		}
	}
	return args;
}

std::vector<byte> convert_little_endian(std::vector<dword> data, int n_bytes) {
	std::vector<byte> little_endian_data;

	for (int i = 0; i < data.size(); i++) {
		for (int j = 0; j < n_bytes; j++) {
			little_endian_data.push_back(data.at(i) & 0xFF);
			data.at(i) >>= 8;
		}
	}

	return little_endian_data;
}

void Assembler::_byte(int& tokenI) {
	EXPECT_TRUE(current_section == Section::DATA, lgr::Logger::LogType::ERROR, std::stringstream()
			<< "Assembler::_byte() - Can only define data in .data section.");

	consume(tokenI);

	std::vector<byte> data = convert_little_endian(parse_arguments(tokenI), 1);
	for (int i = 0; i < data.size(); i++) {
		m_obj.data_section.push_back(data.at(i));
	}
}

void Assembler::_dbyte(int& tokenI) {
	EXPECT_TRUE(current_section == Section::DATA, lgr::Logger::LogType::ERROR, std::stringstream()
			<< "Assembler::_dbyte() - Can only define data in .data section.");

	consume(tokenI);

	std::vector<byte> data = convert_little_endian(parse_arguments(tokenI), 2);
	for (int i = 0; i < data.size(); i++) {
		m_obj.data_section.push_back(data.at(i));
	}
}

void Assembler::_word(int& tokenI) {
	EXPECT_TRUE(current_section == Section::DATA, lgr::Logger::LogType::ERROR, std::stringstream()
			<< "Assembler::_word() - Can only define data in .data section.");

	consume(tokenI);

	std::vector<byte> data = convert_little_endian(parse_arguments(tokenI), 4);
	for (int i = 0; i < data.size(); i++) {
		m_obj.data_section.push_back(data.at(i));
	}
}

void Assembler::_dword(int& tokenI) {
	EXPECT_TRUE(current_section == Section::DATA, lgr::Logger::LogType::ERROR, std::stringstream()
			<< "Assembler::_dword() - Can only define data in .data section.");

	consume(tokenI);

	std::vector<byte> data = convert_little_endian(parse_arguments(tokenI), 8);
	for (int i = 0; i < data.size(); i++) {
		m_obj.data_section.push_back(data.at(i));
	}
}

/* this is pointless, same as .byte */
void Assembler::_sbyte(int& tokenI) {
	EXPECT_TRUE(current_section == Section::DATA, lgr::Logger::LogType::ERROR, std::stringstream()
			<< "Assembler::_sbyte() - Can only define data in .data section.");

	consume(tokenI);

	std::vector<byte> data = convert_little_endian(parse_arguments(tokenI), 1);
	for (int i = 0; i < data.size(); i++) {
		m_obj.data_section.push_back(data.at(i));
	}
}

/* todo, figure out why signed versions of these data defining directives are needed */
void Assembler::_sdbyte(int& tokenI) {
	EXPECT_TRUE(current_section == Section::DATA, lgr::Logger::LogType::ERROR, std::stringstream()
			<< "Assembler::_sdbyte() - Can only define data in .data section.");

	consume(tokenI);

	std::vector<byte> data = convert_little_endian(parse_arguments(tokenI), 2);
	for (int i = 0; i < data.size(); i++) {
		m_obj.data_section.push_back(data.at(i));
	}
}

void Assembler::_sword(int& tokenI) {
	EXPECT_TRUE(current_section == Section::DATA, lgr::Logger::LogType::ERROR, std::stringstream()
			<< "Assembler::_sword() - Can only define data in .data section.");

	consume(tokenI);

	std::vector<byte> data = convert_little_endian(parse_arguments(tokenI), 4);
	for (int i = 0; i < data.size(); i++) {
		m_obj.data_section.push_back(data.at(i));
	}
}

void Assembler::_sdword(int& tokenI) {
	EXPECT_TRUE(current_section == Section::DATA, lgr::Logger::LogType::ERROR, std::stringstream()
			<< "Assembler::_sdword() - Can only define data in .data section.");

	consume(tokenI);

	std::vector<byte> data = convert_little_endian(parse_arguments(tokenI), 8);
	for (int i = 0; i < data.size(); i++) {
		m_obj.data_section.push_back(data.at(i));
	}
}

void Assembler::_char(int& tokenI) {
	EXPECT_TRUE(current_section == Section::DATA, lgr::Logger::LogType::ERROR, std::stringstream()
			<< "Assembler::_char() - Can only define data in .data section.");
	// todo
}

void Assembler::_ascii(int& tokenI) {
	EXPECT_TRUE(current_section == Section::DATA, lgr::Logger::LogType::ERROR, std::stringstream()
			<< "Assembler::_ascii() - Can only define data in .data section.");
	// todo
}

void Assembler::_asciz(int& tokenI) {
	EXPECT_TRUE(current_section == Section::DATA, lgr::Logger::LogType::ERROR, std::stringstream()
			<< "Assembler::_asciz() - Can only define data in .data section.");
	// todo
}
