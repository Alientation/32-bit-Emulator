#include "assembler_test/assembler_test.h"

TEST_F (EmulatorFixture, include_local_dir)
{
	Process p ("-kp " + AEMU_PROJECT_ROOT_DIR +
			"core/assembler/test/preprocessor_test/src/include_local_dir.basm "
			"-outdir " + AEMU_PROJECT_ROOT_DIR +
			"core/assembler/test/preprocessor_test/build");
	ASSERT_TRUE (p.does_create_exe ());

	LoadExecutable loader(*machine, p.get_exe_file());
	machine->run(MAX_INSTRUCTIONS);

	ASSERT_EQ(machine->read_reg(0), 0x135);
}

TEST_F (EmulatorFixture, include_system_dir)
{
	Process p ("-kp " + AEMU_PROJECT_ROOT_DIR +
			"core/assembler/test/preprocessor_test/src/include_sys_dir.basm " +
			"-I " + AEMU_PROJECT_ROOT_DIR +
			"core/assembler/test/preprocessor_test/include/system " +
			"-outdir " + AEMU_PROJECT_ROOT_DIR +
			"core/assembler/test/preprocessor_test/build");
	ASSERT_TRUE (p.does_create_exe ());

	LoadExecutable loader(*machine, p.get_exe_file());
	machine->run(MAX_INSTRUCTIONS);

	ASSERT_EQ(machine->read_reg(0), 0x531);
}