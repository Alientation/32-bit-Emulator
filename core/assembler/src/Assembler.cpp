#include "assembler/Assembler.h"
#include "util/Logger.h"

#include <fstream>
#include <regex>

using namespace lgr;

Assembler::Assembler(Process *process, File *processed_file, std::string output_path) {
	m_process = process;
	m_inputFile = processed_file;

	if (output_path.empty()) {
		m_outputFile = new File(m_inputFile->getFileName(), OBJECT_EXTENSION, processed_file->getFileDirectory(), true);
	} else {
		m_outputFile = new File(output_path, true);
	}

	EXPECT_TRUE(m_process->isValidProcessedFile(processed_file), Logger::LogType::ERROR, std::stringstream() << "Assembler::Assembler() - Invalid processed file: " << processed_file->getExtension());

	m_state = State::NOT_ASSEMBLED;
	m_tokens = Tokenizer::tokenize(processed_file);
}

Assembler::~Assembler() {
	delete m_outputFile;
}

Assembler::State Assembler::get_state() {
	return this->m_state;
}

void Assembler::assemble() {
	log(Logger::LogType::DEBUG, std::stringstream() << "Assembler::assemble() - Assembling file: " << m_inputFile->getFileName());

	EXPECT_TRUE(m_state == State::NOT_ASSEMBLED, Logger::LogType::ERROR, std::stringstream() << "Assembler::assemble() - Assembler is not in the NOT ASSEMBLED state");
	m_state = State::ASSEMBLING;

	// clearing object file
	std::ofstream ofs;
	ofs.open(m_outputFile->getFilePath(), std::ofstream::out | std::ofstream::trunc);
	ofs.close();

	section_table.push_back((SectionHeader) {
		.section_name = 0,
		.type = SectionHeader::Type::TEXT,
		.section_size = 0,
		.entry_size = 0,
	});
	string_table[".text"] = string_table.size();

	section_table.push_back((SectionHeader) {
		.section_name = 1,
		.type = SectionHeader::Type::DATA,
		.section_size = 0,
		.entry_size = 0,
	});
	string_table[".data"] = string_table.size();

	section_table.push_back((SectionHeader) {
		.section_name = 2,
		.type = SectionHeader::Type::BSS,
		.section_size = 0,
		.entry_size = 0,
	});
	string_table[".bss"] = string_table.size();

	// parse tokens
	for (int i = 0; i < m_tokens.size(); ) {
		Tokenizer::Token& token = m_tokens[i];
        log(Logger::LogType::DEBUG, std::stringstream() << "Assembler::assemble() - Assembling token " << i << ": " << token.to_string());
		// log(Logger::LogType::DEBUG, std::stringstream() << "Assembler::assemble() - Indent Level: " << currentIndentLevel << " " << token.to_string());

        // skip non code or directives
        if (isToken(i, Tokenizer::WHITESPACES) || isToken(i, Tokenizer::COMMENTS)) {
            i++;
            continue;
        }

		// perform logic on current token
		if (token.type == Tokenizer::LABEL) {
			if (current_section == Section::NONE) {
				log(Logger::LogType::ERROR, std::stringstream() << "Assembler::assemble() - Label must be located in a section.");
				m_state = State::ASSEMBLER_ERROR;
				break;
			}

			std::string symbol = token.value + (scope_token_indices.empty() ? "" : "::SCOPE:" + std::to_string(scope_token_indices.back()));
			if (current_section == Section::TEXT) {
				add_symbol(symbol, text_section.size() * 4, SymbolTableEntry::BindingInfo::LOCAL, 0);
			} else if (current_section == Section::DATA) {
				add_symbol(symbol, text_section.size() * 4, SymbolTableEntry::BindingInfo::LOCAL, 1);
			} else if (current_section == Section::BSS) {
				add_symbol(symbol, text_section.size() * 4, SymbolTableEntry::BindingInfo::LOCAL, 2);
			}

		} else if (instructions.find(token.type) != instructions.end()) {
			if (current_section != Section::TEXT) {
				log(Logger::LogType::ERROR, std::stringstream() << "Assembler::assemble() - Code must be located in .text section.");
				m_state = State::ASSEMBLER_ERROR;
				break;
			}
			(this->*instructions[token.type])(i);
		} else if (directives.find(token.type) != directives.end()) {
			(this->*directives[token.type])(i);
		} else {
			log(Logger::LogType::ERROR, std::stringstream() << "Assembler::assemble() - Cannot parse token " << i << " " << token.to_string());
			m_state = State::ASSEMBLER_ERROR;
			break;
		}
	}


	// create writer for object file
	m_writer = new FileWriter(m_outputFile);

	/* BELF Header */

	/* Data Section and Header */

	/* Text Section and Header */

	/* BSS Section and Header */

	/* rel.data Section and Header */

	/* rel.text Section and Header */

	/* rel.bss Section and Header */

	/* Symbol Table */

	/* String Table */

	m_writer->close();
    delete m_writer;

	if (m_state == State::ASSEMBLING) {
		m_state = State::ASSEMBLED;
		log(Logger::LogType::DEBUG, std::stringstream() << "Assembler::assemble() - Assembled file: " << m_inputFile->getFileName());
	}
}


/**
 * Skips tokens that match the given regex.
 *
 * @param regex matches tokens to skip.
 * @param tokenI the index of the current token.
 */
void Assembler::skipTokens(int& tokenI, const std::string& regex) {
	while (inBounds(tokenI) && std::regex_match(m_tokens[tokenI].value, std::regex(regex))) {
		tokenI++;
	}
}

/**
 * Skips tokens that match the given types.
 *
 * @param tokenI the index of the current token.
 * @param tokenTypes the types to match.
 */
void Assembler::skipTokens(int& tokenI, const std::set<Tokenizer::Type>& tokenTypes) {
    while (inBounds(tokenI) && tokenTypes.find(m_tokens[tokenI].type) != tokenTypes.end()) {
        tokenI++;
    }
}

/**
 * Expects the current token to exist.
 *
 * @param tokenI the index of the expected token.
 * @param errorMsg the error message to throw if the token does not exist.
 */
bool Assembler::expectToken(int tokenI, const std::string& errorMsg) {
	EXPECT_TRUE(inBounds(tokenI), Logger::LogType::ERROR, std::stringstream(errorMsg));
    return true;
}

bool Assembler::expectToken(int tokenI, const std::set<Tokenizer::Type>& expectedTypes, const std::string& errorMsg) {
	EXPECT_TRUE(inBounds(tokenI), Logger::LogType::ERROR, std::stringstream(errorMsg));
	EXPECT_TRUE(expectedTypes.find(m_tokens[tokenI].type) != expectedTypes.end(), Logger::LogType::ERROR, std::stringstream(errorMsg));
    return true;
}

/**
 * Returns whether the current token matches the given types.
 *
 * @param tokenI the index of the current token.
 * @param tokenTypes the types to match.
 *
 * @return true if the current token matches the given types.
 */
bool Assembler::isToken(int tokenI, const std::set<Tokenizer::Type>& tokenTypes, const std::string& errorMsg) {
    expectToken(tokenI, errorMsg);
    return tokenTypes.find(m_tokens[tokenI].type) != tokenTypes.end();
}

/**
 * Returns whether the current token index is within the bounds of the tokens list.
 *
 * @param tokenI the index of the current token
 *
 * @return true if the token index is within the bounds of the tokens list.
 */
bool Assembler::inBounds(int tokenI) {
    return tokenI < m_tokens.size();
}

/**
 * Consumes the current token.
 *
 * @param tokenI the index of the current token.
 * @param errorMsg the error message to throw if the token does not exist.
 *
 * @returns the value of the consumed token.
 */
Tokenizer::Token& Assembler::consume(int& tokenI, const std::string& errorMsg) {
    expectToken(tokenI, errorMsg);
    return m_tokens[tokenI++];
}

/**
 * Consumes the current token and checks it matches the given types.
 *
 * @param tokenI the index of the current token.
 * @param expectedTypes the expected types of the token.
 * @param errorMsg the error message to throw if the token does not have the expected type.
 *
 * @returns the value of the consumed token.
 */
Tokenizer::Token& Assembler::consume(int& tokenI, const std::set<Tokenizer::Type>& expectedTypes, const std::string& errorMsg) {
    expectToken(tokenI, errorMsg);
	EXPECT_TRUE(expectedTypes.find(m_tokens[tokenI].type) != expectedTypes.end(), Logger::LogType::ERROR, std::stringstream() << errorMsg << " - Unexpected end of file.");
    return m_tokens[tokenI++];
}
