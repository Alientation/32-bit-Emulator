#include "assembler/assembler.h"
#include "emulator32bit/emulator32bit.h"
#include "util/logger.h"
#include "util/types.h"

#include <fstream>
#include <regex>

Assembler::Assembler(Process *process, File processed_file, const std::string& output_path) : m_process(process), m_inputFile(processed_file)
{
	if (output_path.empty()) {
		m_outputFile = File(m_inputFile.get_name(), OBJECT_EXTENSION, processed_file.get_dir(), true);
	} else {
		m_outputFile = File(output_path, true);
	}

	EXPECT_TRUE_SS(m_process->valid_processed_file(processed_file), std::stringstream()
			<< "Assembler::Assembler() - Invalid processed file: "
			<< processed_file.get_extension());

	m_state = State::NOT_ASSEMBLED;
	m_tokens = Tokenizer::tokenize(processed_file);
}

Assembler::State Assembler::get_state()
{
	return this->m_state;
}

void add_sections(ObjectFile& m_obj)
{
	m_obj.add_section(".text", (ObjectFile::SectionHeader) {
		.section_name = 0,
		.type = ObjectFile::SectionHeader::Type::TEXT,
		.section_start = 0,
		.section_size = 0,
		.entry_size = 4,
	});

	m_obj.add_section(".data", (ObjectFile::SectionHeader) {
		.section_name = 0,
		.type = ObjectFile::SectionHeader::Type::DATA,
		.section_start = 0,
		.section_size = 0,
		.entry_size = 0,
	});

	m_obj.add_section(".bss", (ObjectFile::SectionHeader) {
		.section_name = 0,
		.type = ObjectFile::SectionHeader::Type::BSS,
		.section_start = 0,
		.section_size = 0,
		.entry_size = 0,
	});

	m_obj.add_section(".symtab", (ObjectFile::SectionHeader) {
		.section_name = 0,
		.type = ObjectFile::SectionHeader::Type::SYMTAB,
		.section_start = 0,
		.section_size = 0,
		.entry_size = 26,
	});

	m_obj.add_section(".rel.text", (ObjectFile::SectionHeader) {
		.section_name = 0,
		.type = ObjectFile::SectionHeader::Type::REL_TEXT,
		.section_start = 0,
		.section_size = 0,
		.entry_size = 28,
	});

	m_obj.add_section(".rel.data", (ObjectFile::SectionHeader) {
		.section_name = 0,
		.type = ObjectFile::SectionHeader::Type::REL_DATA,
		.section_start = 0,
		.section_size = 0,
		.entry_size = 28,
	});

	m_obj.add_section(".rel.bss", (ObjectFile::SectionHeader) {
		.section_name = 0,
		.type = ObjectFile::SectionHeader::Type::REL_BSS,
		.section_start = 0,
		.section_size = 0,
		.entry_size = 28,
	});

	m_obj.add_section(".strtab", (ObjectFile::SectionHeader) {
		.section_name = 0,
		.type = ObjectFile::SectionHeader::Type::STRTAB,
		.section_start = 0,
		.section_size = 0,
		.entry_size = 0,
	});
}

// todo, filter out all spaces and tabs
File Assembler::assemble()
{
	DEBUG("Assembler::assemble() - Assembling file: %s", m_inputFile.get_name().c_str());

	EXPECT_TRUE_SS(m_state == State::NOT_ASSEMBLED, std::stringstream()
			<< "Assembler::assemble() - Assembler is not in the NOT ASSEMBLED state");
	m_state = State::ASSEMBLING;

	// clearing object file
	std::ofstream ofs;
	ofs.open(m_outputFile.get_path(), std::ofstream::out | std::ofstream::trunc);
	ofs.close();

	add_sections(m_obj);

	// parse tokens
	DEBUG("Assembler::assemble() - Parsing tokens.");
	for (size_t i = 0; i < m_tokens.size(); ) {
		Tokenizer::Token& token = m_tokens[i];
        DEBUG("Assembler::assemble() - Assembling token %d: %s", i, token.to_string().c_str());

        // skip non code or directives
        if (is_token(i, Tokenizer::WHITESPACES) || is_token(i, Tokenizer::COMMENTS)) {
            i++;
            continue;
        }

		// perform logic on current token
		if (token.type == Tokenizer::LABEL) {
			if (current_section == Section::NONE) {
				ERROR("Assembler::assemble() - Label must be located in a section.");
				m_state = State::ASSEMBLER_ERROR;
				break;
			}

			std::string symbol = token.value.substr(0, token.value.size()-1) + (scopes.empty() ? "" : "::SCOPE:" + std::to_string(scopes.back()));
			if (current_section == Section::TEXT) {
				m_obj.add_symbol(symbol, m_obj.text_section.size() * 4, ObjectFile::SymbolTableEntry::BindingInfo::LOCAL, 0);
			} else if (current_section == Section::DATA) {
				m_obj.add_symbol(symbol, m_obj.data_section.size(), ObjectFile::SymbolTableEntry::BindingInfo::LOCAL, 1);
			} else if (current_section == Section::BSS) {
				m_obj.add_symbol(symbol, m_obj.bss_section, ObjectFile::SymbolTableEntry::BindingInfo::LOCAL, 2);
			}
			i++;
		} else if (instructions.find(token.type) != instructions.end()) {
			if (current_section != Section::TEXT) {
				ERROR("Assembler::assemble() - Code must be located in .text section.");
				m_state = State::ASSEMBLER_ERROR;
				break;
			}
			(this->*instructions[token.type])(i);
		} else if (directives.find(token.type) != directives.end()) {
			(this->*directives[token.type])(i);
		} else {
			ERROR("Assembler::assemble() - Cannot parse token %d %s", i, token.to_string().c_str());
			m_state = State::ASSEMBLER_ERROR;
			break;
		}
	}
	DEBUG("Assembler::assemble() - Finished parsing tokens.");

	/* Parse through second time to fill in local symbol values */
	fill_local();

	m_obj.write_object_file(m_outputFile);

	if (m_state == State::ASSEMBLING) {
		m_state = State::ASSEMBLED;
		DEBUG("Assembler::assemble() - Assembled file: %s", m_inputFile.get_name());
	}

	return m_outputFile;
}


void Assembler::fill_local()
{
	size_t tok_i = 0;

	DEBUG("Assembler::fill_local() - Parsing relocation entries to fill in known values.");
	std::vector<int> local_scope;
	int local_count_scope = 0;
	for (size_t i = 0; i < m_obj.rel_text.size(); i++) {
		ObjectFile::RelocationEntry &rel = m_obj.rel_text.at(i);
		DEBUG("Assembler::fill_local() - Evaluating relocation entry %s",
				m_obj.strings[m_obj.symbol_table[rel.symbol].symbol_name].c_str());

		while (tok_i < rel.token && tok_i < m_tokens.size()) {
			if (m_tokens[tok_i].type == Tokenizer::ASSEMBLER_SCOPE) {
				local_scope.push_back(local_count_scope++);
			} else if (m_tokens[tok_i].type == Tokenizer::ASSEMBLER_SCEND) {
				local_scope.pop_back();
			}

			tok_i++;
		}

		// first find if symbol is defined in local scope
		ObjectFile::SymbolTableEntry symbol_entry;
		bool found_local = false;
		std::string symbol = m_obj.strings[m_obj.symbol_table[rel.symbol].symbol_name];
		for (size_t scopeI = local_scope.size()-1; scopeI+1 != 0; scopeI--) {
			std::string local_symbol_name = symbol + "::SCOPE:" + std::to_string(local_scope[scopeI]);
			if (m_obj.string_table.find(local_symbol_name) == m_obj.string_table.end()) {
				continue;
			}

			symbol_entry = m_obj.symbol_table[m_obj.string_table[local_symbol_name]];
			found_local = true;
			break;
		}

		if (!found_local) {
			if (m_obj.symbol_table.at(rel.symbol).binding_info != ObjectFile::SymbolTableEntry::BindingInfo::WEAK
				&& m_obj.symbol_table.at(rel.symbol).section == m_obj.section_table[".text"]) {
				symbol_entry = m_obj.symbol_table.at(rel.symbol);
			} else {
				continue;
			}
		} else {
			rel.symbol = symbol_entry.symbol_name;
			continue;
		}

		/* Only fixes relative offsets, we cannot fix absolute references since that must be done when the executable is loaded into memory */
		switch (rel.type) {
			case ObjectFile::RelocationEntry::Type::R_EMU32_O_LO12:
				// text_section[rel.offset/4] = mask_0(text_section[rel.offset/4], 0, 14) + bitfield_u32(symbol_entry.symbol_value, 0, 12);
				// break;
			case ObjectFile::RelocationEntry::Type::R_EMU32_ADRP_HI20:
				// text_section[rel.offset/4] = mask_0(text_section[rel.offset/4], 0, 20) + bitfield_u32(symbol_entry.symbol_value, 12, 20);
				// break;
			case ObjectFile::RelocationEntry::Type::R_EMU32_MOV_LO19:
				// text_section[rel.offset/4] = mask_0(text_section[rel.offset/4], 0, 19) + bitfield_u32(symbol_entry.symbol_value, 0, 19);
				// break;
			case ObjectFile::RelocationEntry::Type::R_EMU32_MOV_HI13:
				// text_section[rel.offset/4] = mask_0(text_section[rel.offset/4], 0, 19) + bitfield_u32(symbol_entry.symbol_value, 19, 13);
				// break;
				continue;
			case ObjectFile::RelocationEntry::Type::R_EMU32_B_OFFSET22:
				EXPECT_TRUE_SS((symbol_entry.symbol_value & 0b11) == 0, std::stringstream()
						<< "Assembler::fill_local() - Expected relocation value for R_EMU32_B_OFFSET22 to be 4 byte aligned. Got "
						<< symbol_entry.symbol_value);
				m_obj.text_section[rel.offset/4] = mask_0(m_obj.text_section[rel.offset/4], 0, 22) +
						bitfield_u32(bitfield_s32(symbol_entry.symbol_value, 2, 22) - rel.offset/4, 0, 22);
				break;
			case ObjectFile::RelocationEntry::Type::UNDEFINED:
			default:
				ERROR("Assembler::fill_local() - Unknown relocation entry type.");
		}

		m_obj.rel_text.erase(m_obj.rel_text.begin() + i);							/* For now, simply delete from vector. In future look to optimize */
		i--;															/* Offset the for loop increment */
	}

	DEBUG("Assembler::fill_local() - Finished parsing relocation entries.");
}


/**
 * Skips tokens that match the given regex.
 *
 * @param regex matches tokens to skip.
 * @param tok_i the index of the current token.
 */
void Assembler::skip_tokens(size_t& tok_i, const std::string& regex)
{
	while (in_bounds(tok_i) && std::regex_match(m_tokens[tok_i].value, std::regex(regex))) {
		tok_i++;
	}
}

/**
 * Skips tokens that match the given types.
 *
 * @param tok_i the index of the current token.
 * @param tokenTypes the types to match.
 */
void Assembler::skip_tokens(size_t& tok_i, const std::set<Tokenizer::Type>& tokenTypes)
{
    while (in_bounds(tok_i) && tokenTypes.find(m_tokens[tok_i].type) != tokenTypes.end()) {
        tok_i++;
    }
}

/**
 * Expects the current token to exist.
 *
 * @param tok_i the index of the expected token.
 * @param errorMsg the error message to throw if the token does not exist.
 */
bool Assembler::expect_token(size_t tok_i, const std::string& errorMsg)
{
	EXPECT_TRUE_SS(in_bounds(tok_i), std::stringstream(errorMsg));
    return true;
}

bool Assembler::expect_token(size_t tok_i, const std::set<Tokenizer::Type>& expectedTypes, const std::string& errorMsg)
{
	EXPECT_TRUE_SS(in_bounds(tok_i), std::stringstream(errorMsg));
	EXPECT_TRUE_SS(expectedTypes.find(m_tokens[tok_i].type) != expectedTypes.end(), std::stringstream(errorMsg));
    return true;
}

/**
 * Returns whether the current token matches the given types.
 *
 * @param tok_i the index of the current token.
 * @param tokenTypes the types to match.
 *
 * @return true if the current token matches the given types.
 */
bool Assembler::is_token(size_t tok_i, const std::set<Tokenizer::Type>& tokenTypes, const std::string& errorMsg)
{
    expect_token(tok_i, errorMsg);
    return tokenTypes.find(m_tokens[tok_i].type) != tokenTypes.end();
}

/**
 * Returns whether the current token index is within the bounds of the tokens list.
 *
 * @param tok_i the index of the current token
 *
 * @return true if the token index is within the bounds of the tokens list.
 */
bool Assembler::in_bounds(size_t tok_i)
{
    return tok_i < m_tokens.size();
}

/**
 * Consumes the current token.
 *
 * @param tok_i the index of the current token.
 * @param errorMsg the error message to throw if the token does not exist.
 *
 * @returns the value of the consumed token.
 */
Tokenizer::Token& Assembler::consume(size_t& tok_i, const std::string& errorMsg)
{
    expect_token(tok_i, errorMsg);
    return m_tokens[tok_i++];
}

/**
 * Consumes the current token and checks it matches the given types.
 *
 * @param tok_i the index of the current token.
 * @param expectedTypes the expected types of the token.
 * @param errorMsg the error message to throw if the token does not have the expected type.
 *
 * @returns the value of the consumed token.
 */
Tokenizer::Token& Assembler::consume(size_t& tok_i, const std::set<Tokenizer::Type>& expectedTypes, const std::string& errorMsg)
{
    expect_token(tok_i, errorMsg);
	EXPECT_TRUE_SS(expectedTypes.find(m_tokens[tok_i].type) != expectedTypes.end(), std::stringstream()
			<< errorMsg << " - Unexpected end of file.");
	return m_tokens.at(tok_i++);
}
