#pragma once
#ifndef LOADEXECUTABLE_H

#include "emulator32bit/Emulator32bit.h"
#include "util/File.h"

class LoadExecutable {
	public:
		LoadExecutable(Emulator32bit& emu, File exe_file);

	private:
		Emulator32bit& m_emu;
		File m_exe_file;

		void load(word start_addr = 0);
};



#endif /* LOADEXECUTABLE_H */