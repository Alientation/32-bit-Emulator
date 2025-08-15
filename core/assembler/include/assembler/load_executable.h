#pragma once

#include "emulator32bit/emulator32bit.h"
#include "util/file.h"

class LoadExecutable
{
    public:
        LoadExecutable(Emulator32bit& emu, File exe_file);

    private:
        Emulator32bit& m_emu;
        File m_exe_file;

        void load();
};
