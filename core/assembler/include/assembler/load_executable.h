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
    /// @param[in] emu The emulator that gets the program.
    /// @param[in] exe_file The executable to load. It is read by load (), not here.
    LoadExecutable(Emulator32bit &emu, const File &exe_file);

    /// An executable that is in memory (the fuzz target of the loader). The error messages name
    /// no file.
    ///
    /// @param[in] emu The emulator that gets the program.
    /// @param[in] exe_bytes The bytes of the executable.
    LoadExecutable(Emulator32bit &emu, std::vector<byte> exe_bytes);

    /// Reads the executable and copies it into the emulator. The emulator must already have a
    /// current process (VirtualMemory::begin_process) if the sections are placed at virtual
    /// addresses. Fatal if the file is not a valid executable.
    void load();

  private:
    Emulator32bit &m_emu;
    File m_exe_file;

    /// The executable when it is in memory, then m_exe_file is not used.
    std::vector<byte> m_exe_bytes;
    bool m_in_memory = false;
};
