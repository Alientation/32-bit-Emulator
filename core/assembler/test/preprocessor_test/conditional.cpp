#include "assembler_test/assembler_test.h"

TEST_F (EmulatorFixture, conditional_ifdef)
{
	Process p ("-kp " + AEMU_PROJECT_ROOT_DIR +
			"core/assembler/test/preprocessor_test/src/conditional_ifdef.basm "
			"-outdir " + AEMU_PROJECT_ROOT_DIR +
			"core/assembler/test/preprocessor_test/build");
	ASSERT_TRUE (p.does_create_exe ());

	LoadExecutable loader(*machine, p.get_exe_file());
	machine->run(MAX_INSTRUCTIONS);

	ASSERT_EQ(machine->read_reg(0), 3);
}