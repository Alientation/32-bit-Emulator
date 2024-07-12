#pragma once
#ifndef LINKER_H
#define LINKER_H

#include "assembler/ObjectFile.h"

class Linker {
	public:
		Linker(std::vector<ObjectFile> obj_files);

	private:
		std::vector<ObjectFile> m_obj_files;

		ObjectFile m_obj_File;
};


#endif /* LINKER_H */