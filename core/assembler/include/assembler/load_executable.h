#pragma once

#include "emulator32bit/emulator32bit.h"
#include "util/file.h"

/// Loads a linked executable (.bexe) into the memory of an emulator and points its PC at `_start`.
///
/// The linker has already resolved every address, so loading only copies the sections to the
/// addresses in their section headers (mapping the virtual pages they need first).
class LoadExecutable
{
  public:
    LoadExecutable(Emulator32bit &emu, File exe_file);

    /// Reads the executable and copies it into the emulator. The emulator must already have a
    /// current process (VirtualMemory::begin_process) if the sections are placed at virtual
    /// addresses. Fatal if the file is not a valid executable.
    void load();

  private:
    Emulator32bit &m_emu;
    File m_exe_file;
};
