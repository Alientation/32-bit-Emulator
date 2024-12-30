#pragma once
#ifndef ASSEMBLER_TEST_H
#define ASSEMBLER_TEST_H

#include <gtest/gtest.h>
#include <assembler/assembler.h>
#include <assembler/build.h>
#include <assembler/linker.h>
#include <assembler/load_executable.h>
#include <assembler/object_file.h>
#include <assembler/preprocessor.h>
#include <assembler/static_library.h>
#include <assembler/tokenizer.h>

#define MAX_INSTRUCTIONS 10000

class EmulatorFixture : public ::testing::Test
{
protected:
	Emulator32bit *machine = nullptr;

	void SetUp () override
	{
		machine = new Emulator32bit (
		new RAM (16, 0),
		new ROM (File (AEMU_PROJECT_ROOT_DIR + "core/assembler/test/rom.bin", true), 16, 16),
		new Disk (File (AEMU_PROJECT_ROOT_DIR + "core/assembler/test/disk.bin"), 32, 32));
		long long pid = machine->system_bus.mmu.begin_process ();
	}

	void TearDown () override
	{
		delete machine;
	}
};

#endif /* ASSEMBLER_TEST_H */