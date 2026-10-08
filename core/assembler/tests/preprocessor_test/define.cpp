#include "assembler_test/assembler_test.h"

TEST_F(EmulatorFixture, define_no_args)
{
    Build p("-kp " + std::string(AEMU_PROJECT_ROOT_DIR)
            + "assembler/tests/preprocessor_test/src/define_no_args.basm "
              "-outdir "
            + std::string(AEMU_PROJECT_ROOT_DIR) + "assembler/tests/preprocessor_test/build");
    p.run();
    ASSERT_TRUE(p.does_create_exe());

    LoadExecutable(*machine, p.get_exe_file()).load();
    machine->run(MAX_INSTRUCTIONS);

    ASSERT_EQ(machine->read_reg(0), 13);
}

TEST_F(EmulatorFixture, define_no_args_multiline)
{
    Build p("-kp " + std::string(AEMU_PROJECT_ROOT_DIR)
            + "assembler/tests/preprocessor_test/src/define_no_args_multiline.basm "
              "-outdir "
            + std::string(AEMU_PROJECT_ROOT_DIR) + "assembler/tests/preprocessor_test/build");
    p.run();
    ASSERT_TRUE(p.does_create_exe());

    LoadExecutable(*machine, p.get_exe_file()).load();
    machine->run(MAX_INSTRUCTIONS);

    ASSERT_EQ(machine->read_reg(0), 13);
}

TEST_F(EmulatorFixture, define_args)
{
    Build p("-kp " + std::string(AEMU_PROJECT_ROOT_DIR)
            + "assembler/tests/preprocessor_test/src/define_args.basm "
              "-outdir "
            + std::string(AEMU_PROJECT_ROOT_DIR) + "assembler/tests/preprocessor_test/build");
    p.run();
    ASSERT_TRUE(p.does_create_exe());

    LoadExecutable(*machine, p.get_exe_file()).load();
    machine->run(MAX_INSTRUCTIONS);

    ASSERT_EQ(machine->read_reg(0), 13);
}

TEST_F(EmulatorFixture, define_args_multiline)
{
    Build p("-kp " + std::string(AEMU_PROJECT_ROOT_DIR)
            + "assembler/tests/preprocessor_test/src/define_args_multiline.basm "
              "-outdir "
            + std::string(AEMU_PROJECT_ROOT_DIR) + "assembler/tests/preprocessor_test/build");
    p.run();
    ASSERT_TRUE(p.does_create_exe());

    LoadExecutable(*machine, p.get_exe_file()).load();
    machine->run(MAX_INSTRUCTIONS);

    ASSERT_EQ(machine->read_reg(0), 13);
}

TEST_F(EmulatorFixture, define_redefine)
{
    Build p("-kp " + std::string(AEMU_PROJECT_ROOT_DIR)
            + "assembler/tests/preprocessor_test/src/define_redefine.basm "
              "-outdir "
            + std::string(AEMU_PROJECT_ROOT_DIR) + "assembler/tests/preprocessor_test/build");
    p.run();
    ASSERT_TRUE(p.does_create_exe());

    LoadExecutable(*machine, p.get_exe_file()).load();
    machine->run(MAX_INSTRUCTIONS);

    ASSERT_EQ(machine->read_reg(0), 13);
}

TEST_F(EmulatorFixture, define_undefine)
{
    Build p("-kp " + std::string(AEMU_PROJECT_ROOT_DIR)
            + "assembler/tests/preprocessor_test/src/define_undefine.basm "
              "-outdir "
            + std::string(AEMU_PROJECT_ROOT_DIR) + "assembler/tests/preprocessor_test/build");
    p.run();
    ASSERT_TRUE(p.does_create_exe());

    LoadExecutable(*machine, p.get_exe_file()).load();
    machine->run(MAX_INSTRUCTIONS);

    ASSERT_EQ(machine->read_reg(0), 13);
}
