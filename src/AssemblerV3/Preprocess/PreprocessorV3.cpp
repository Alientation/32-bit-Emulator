#include "PreprocessorV3.h"


/**
 * Constructs a preprocessor object with the given file.
 * 
 * @param file the file to preprocess
 */
Preprocessor::Preprocessor(File file) {
	this->inputFile = &file;
	this->outputFile = new File(file.getFileName(), "i", file.getFileDirectory());

	state = State::UNPROCESSED;
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
	state = State::PROCESSING;
	
	// find each instance of '#' and see if it is a preprocessor directive
	// if it is, then call the appropriate function
	FileReader reader = FileReader(inputFile);
	FileWriter writer = FileWriter(outputFile);

}

















/**
 * Returns the state of the preprocessor
 * 
 * @return the state of the preprocessor
 */
Preprocessor::State Preprocessor::getState() {
	return state;
}