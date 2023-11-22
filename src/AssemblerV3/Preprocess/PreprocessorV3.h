#include <../src/AssemblerV3/File.h>

#include <string>

#ifndef PREPROCESSORV3_H
#define PREPROCESSORV3_H

class Preprocessor;
class Preprocessor {
	public:
		enum State {
			UNPROCESSED, PROCESSING, PROCESSED_SUCCESS, PROCESSED_ERROR
		};

		Preprocessor(File file);
		~Preprocessor();

		void preprocess();
		State getState();
	private:
		File* file;
		State state;
};


#endif