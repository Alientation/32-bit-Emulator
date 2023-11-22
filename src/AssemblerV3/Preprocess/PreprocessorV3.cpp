#include "PreprocessorV3.h"


/**
 * Constructs a preprocessor object with the given file.
 * 
 * @param file the file to preprocess
 */
Preprocessor::Preprocessor(File file) {
	this->file = &file;

	state = State::UNPROCESSED;
}

/**
 * Destructs a preprocessor object
 */
Preprocessor::~Preprocessor() {
	file->~File();
}

/**
 * Preprocesses the file
 */
void Preprocessor::preprocess() {
	state = State::PROCESSING;
}

/**
 * Returns the state of the preprocessor
 * 
 * @return the state of the preprocessor
 */
Preprocessor::State Preprocessor::getState() {
	return state;
}