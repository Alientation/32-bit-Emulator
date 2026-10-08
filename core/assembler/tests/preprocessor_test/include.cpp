#include "assembler_test/assembler_test.h"

TEST_F(EmulatorFixture, include_local_dir)
{
    Build p("-kp " + std::string(AEMU_PROJECT_ROOT_DIR)
            + "assembler/tests/preprocessor_test/src/include_local_dir.basm "
              "-outdir "
            + std::string(AEMU_PROJECT_ROOT_DIR) + "assembler/tests/preprocessor_test/build");
    p.run();
    ASSERT_TRUE(p.does_create_exe());

    LoadExecutable(*machine, p.get_exe_file()).load();
    machine->run(MAX_INSTRUCTIONS);

    ASSERT_EQ(machine->read_reg(0), 0x135);
}

TEST_F(EmulatorFixture, include_system_dir)
{
    Build p("-kp " + std::string(AEMU_PROJECT_ROOT_DIR)
            + "assembler/tests/preprocessor_test/src/include_sys_dir.basm " + "-I "
            + std::string(AEMU_PROJECT_ROOT_DIR)
            + "assembler/tests/preprocessor_test/include/system " + "-outdir "
            + std::string(AEMU_PROJECT_ROOT_DIR) + "assembler/tests/preprocessor_test/build");
    p.run();
    ASSERT_TRUE(p.does_create_exe());

    LoadExecutable(*machine, p.get_exe_file()).load();
    machine->run(MAX_INSTRUCTIONS);

    ASSERT_EQ(machine->read_reg(0), 0x531);
}