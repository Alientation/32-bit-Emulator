#include "Assembler.h"

int main() {
	std::vector<std::string> files = {"..\\src\\Assembly\\temp.basm"};
	Assembler assembler(files);
}


Assembler::Assembler(std::vector<std::string> files) {
	this->files = files;

	// tokenize each file
	for (std::string file : files) {
		tokenize(file);
	}

	// parse each file the first time to build symbol table
	for (std::string file : files) {
		parse(file);
	}


}


void Assembler::parse(std::string filename) {
	if (objectFilesMap.find(filename) == objectFilesMap.end()) {
		error(INTERNAL_ERROR, std::stringstream() << "File Not Tokenized: " << filename);
	}

	ObjectFile objectfile = *objectFilesMap[filename];

	// iterate through each token in the file
	for (int i = 0; i < objectfile.tokens.size(); i++) {
		Token& token = objectfile.tokens[i];

		if (token.string.size() == 0) {
			error(INTERNAL_ERROR, std::stringstream() << "Empty Token");
		}

		// check if token is a label
		if (token.string.back() == ':') {

		} else if (directiveMap.find(token.string) != directiveMap.end()) {
			DirectiveType directiveType = directiveMap[token.string];
			

		} else if (instructionMap.find(token.string) != instructionMap.end()) {
			


		} else {
			
		}
	}
}


void Assembler::linker() {
	
}


void Assembler::assemble() {
	
}



void Assembler::tokenize(std::string filename) {
	log(LOG, std::stringstream() << BOLD << BOLD_WHITE << "Reading File: " << filename << RESET);

	// ensure we have not already read this file
	if (objectFilesMap.find(filename) != objectFilesMap.end()) {
		error(MULTIPLE_DEFINITION_ERROR, std::stringstream() << "File Already Read: " << filename);
	}

	// read all characters from file to a string
    if (split(filename, '.').back() != "basm") {
        error(FILE_ERROR, std::stringstream() << "Unrecognized File Extension: " << split(filename, '.').back());
    }

    std::ifstream file(filename, std::ifstream::in);
    std::string sourceCode;

    // check if file exists
    if (!file) {
        error(FILE_ERROR, std::stringstream() << "File Does Not Exist: " << filename);
    }

    char mychar;
    while (file) {
        mychar = file.get();
        sourceCode += mychar;
    }
    file.close();

    log(LOG, std::stringstream() << BOLD << BOLD_WHITE << "Source Code\n" << RESET << sourceCode);
    log(LOG, std::stringstream() << BOLD << BOLD_GREEN << "Read File: " << filename << RESET);


	// now tokenize the file
	log(LOG_TOKENIZER, std::stringstream() << BOLD << BOLD_WHITE << "Tokenizing Source Code" << RESET);
	std::vector<Token> tokens;

	// current token being constructed
    std::string currentToken = "";

    // is the current token a comment and what type
    bool isSingleLineComment = false;
    bool isMultiLineComment = false;

    // the index of the first character from the original source code of the current token being constructed
    int currentTokenStart = -1;

    // the line the current token is on in the original source code
    int lineNumber = 1;

    // default true for first column on each line. Every subsequent column should have a preceeding tab character
    bool readyForNextToken = true;

    // iterate through each character in the source code
    for (int charLocation = 0; charLocation < sourceCode.size(); charLocation++) {
        char character = sourceCode[charLocation];
        
        // keep track of the current line number
        if (character == '\n') {
            lineNumber++;
        }

        // skip whitespace until we find a token to tokenize, this will trim any leading whitespace
        if (readyForNextToken && std::isspace(character)) {
            continue;
        }

        // add the current token to tokens
        currentToken += character;

        // check to end current built token
        if (!isMultiLineComment && (character == '\n' || character == '\t' || charLocation == sourceCode.size() - 1)) {
            // end current token, trim any trailing whitespace
            currentToken = trim(currentToken);

            // check if not a comment
            if (!isSingleLineComment) {
                tokens.push_back(Token(currentToken, currentTokenStart, character == '\n' ? lineNumber - 1 : lineNumber));
                log(LOG_TOKENIZER, std::stringstream() << CYAN << "Token\t" << RESET << "[" << currentToken << "]");
            } else {
                log(LOG_TOKENIZER, std::stringstream() << GREEN << "Comment\t" << RESET << "[" << currentToken << "]");
            }

            // reset current token
            currentToken.clear();
            currentTokenStart = -1;

            // prepare for next token
            readyForNextToken = true;
            
            // end single line comment if a new line was reached
            if (character == '\n') {
                isSingleLineComment = false;
            }
            continue;
        }

        // found first non-whitespace character of a token
        if (readyForNextToken) {
            // mark current character index
            currentTokenStart = charLocation;
            readyForNextToken = false;

            // mark token as a comment if it starts with a ';'
            if (character == ';') {
                isSingleLineComment = true;
            }
        }

        // check if token is start of multi line comment denoted by ;*
        if (isSingleLineComment && currentToken.size() == 2 && character == '*') {
            isMultiLineComment = true;
            isSingleLineComment = false;
        }

        // check if multi line comment is ending denoted by *;
        if (isMultiLineComment && currentToken.size() >= 4 && character == ';' 
            && currentToken[currentToken.size() - 2] == '*') {
            isMultiLineComment = false;
            
            std::vector<std::string> list = split(currentToken,'\n');
            log(LOG_TOKENIZER, std::stringstream() << GREEN << "Comments\t" << RESET << "[" << tostring(list) << "]");

            // end current token
            currentToken.clear();
            currentTokenStart = -1;

            readyForNextToken = true;
        }
    }

    // check if multi line comment was never closed
    if (isMultiLineComment) {
        error(MISSING_TOKEN_ERROR, std::stringstream() << "Multiline comment is not closed by \'*;\' ");
    }

    // check if current token has not been processed
    if (currentToken.size() != 0) {
        error(INTERNAL_ERROR,
                std::stringstream() << "Current token has not been processed");
    }

    log(LOG_TOKENIZER, std::stringstream() << BOLD << BOLD_GREEN << "Tokenized\t" << RESET << tostring(tokens));


	// add list of tokens to map to filename
	objectFilesMap[filename] = new ObjectFile(tokens);
}