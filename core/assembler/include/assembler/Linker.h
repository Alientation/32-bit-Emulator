#pragma once
#ifndef LINKER_H
#define LINKER_H

#include "assembler/ObjectFile.h"

class Linker {
	public:
		Linker(std::vector<ObjectFile> obj_files, File exe_file);

	private:
		std::vector<ObjectFile> m_obj_files;

		File m_exe_file;

		void link();
};

#endif /* LINKER_H */