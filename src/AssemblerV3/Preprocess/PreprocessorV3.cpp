#include "PreprocessorV3.h"

#include "Logger.h"


/**
 * Constructs a preprocessor object with the given file.
 * 
 * @param file the file to preprocess
 */
Preprocessor::Preprocessor(File file) {
	this->inputFile = &file;
	this->outputFile = new File(file.getFileName(), "i", file.getFileDirectory());

	if (inputFile->getExtension() != "basm") {
		log(ERROR, std::stringstream() << "Preprocessor::Preprocessor() - File extension must be .basm");
		return;
	}

	state = State::UNPROCESSED;

	reader = new FileReader(inputFile);
	writer = new FileWriter(outputFile);
}

/**
 * Destructs a preprocessor object
 */
Preprocessor::~Preprocessor() {
	inputFile->~File();
}

/**
 * Preprocesses the file
 */
void Preprocessor::preprocess() {
	if (state != State::UNPROCESSED) {
		log(ERROR, std::stringstream() << "Preprocessor::preprocess() - Preprocessor is not in the UNPROCESSED state");
		return;
	}
	state = State::PROCESSING;

	currentPreprocessedToken = "";
	while (reader->hasNextByte()) {
		char byte = reader->readByte();
		if (std::isspace(byte)) {
			preprocessToken();
			writer->writeByte(byte);
		} else if (!reader->hasNextByte()) {
			preprocessToken();
		} else {
			currentPreprocessedToken += byte;
		}
	}

	state = State::PROCESSED_SUCCESS;
	writer->close();
	reader->close();
}

void Preprocessor::preprocessToken() {
	if (preprocessorDirectives.find(currentPreprocessedToken) != preprocessorDirectives.end()) {
		(this->*preprocessorDirectives[currentPreprocessedToken])();
	} else if (symbols.find(currentPreprocessedToken) != symbols.end()) {
		writer->writeString(symbols[currentPreprocessedToken]);
	} else {
		writer->writeString(currentPreprocessedToken);
	}

	currentPreprocessedToken.clear();
}



void Preprocessor::_include() {

}

void Preprocessor::_macro() {

}

void Preprocessor::_macret() {

}

void Preprocessor::_macend() {

}

void Preprocessor::_invoke() {

}

void Preprocessor::_define() {

}

void Preprocessor::_ifdef() {

}

void Preprocessor::_ifndef() {

}

void Preprocessor::_else() {

}

void Preprocessor::_elsedef() {

}

void Preprocessor::_elsendef() {

}

void Preprocessor::_endif() {

}

void Preprocessor::_undef() {

}







/**
 * Returns the state of the preprocessor
 * 
 * @return the state of the preprocessor
 */
Preprocessor::State Preprocessor::getState() {
	return state;
}