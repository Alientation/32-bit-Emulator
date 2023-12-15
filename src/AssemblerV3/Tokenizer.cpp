#include "Tokenizer.h"
#include <../src/util/Logger.h>

#include <regex>

void Tokenizer::tokenize(File* srcFile, std::vector<Tokenizer::Token>& tokens) {
    log(DEBUG, std::stringstream() << "Tokenizer::tokenize() - Tokenizing file: " << srcFile->getFileName());
	FileReader reader(srcFile);

    // append a new line to the end to allow regex matching to match an ending whitespace
	std::string source_code = reader.readAll() + "\n";
	reader.close();

	tokens.clear();
	while (source_code.size() > 0) {
		// try to match regex
		bool matched = false;
		for (std::pair<std::string, Type> regexPair : TOKEN_SPEC) {
			std::string regex = regexPair.first;
			Type type = regexPair.second;
			std::regex token_regex(regex);
			std::smatch match;
			if (std::regex_search(source_code, match, token_regex)) {
				// matched regex
				std::string token_value = match.str();
				tokens.push_back(Token(type, token_value));
				source_code = match.suffix();
				matched = true;

                // log(LOG, std::stringstream() << "Tokenizer::tokenize() - Token " << tokens.size()-1 << ": " << tokens.back().toString());
				break;
			}
		}

		// check if regex matched
		EXPECT_TRUE(matched, ERROR, std::stringstream() << "Tokenizer::tokenize() - Could not match regex to source code: " << source_code);
	}

	// print out tokens
	log(DEBUG, std::stringstream() << "Tokenizer::tokenize() - Tokenized file: " << srcFile->getFileName());
	for (int i = 0; i < tokens.size(); i++) {
		log(DEBUG, std::stringstream() << "Tokenizer::tokenize() - Token[" << i << "]=" << tokens[i].toString());
	}
}