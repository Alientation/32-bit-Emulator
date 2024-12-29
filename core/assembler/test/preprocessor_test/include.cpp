#include "assembler_test/assembler_test.h"

TEST(include, include_local_dir)
{
	Process p("-c -kp " + AEMU_PROJECT_ROOT_DIR +
			"core\\assembler\\test\\preprocessor_test\\include_local_dir.basm "
			"-outdir " + AEMU_PROJECT_ROOT_DIR +
			"core\\assembler\\test\\preprocessor_test\\build");
}