#include "assembler_test/assembler_test.h"

TEST_F(EmulatorFixture, conditional_ifdef)
{
    Build p("-kp " + std::string(AEMU_PROJECT_ROOT_DIR)
            + "assembler/tests/preprocessor_test/src/conditional_ifdef.basm "
              "-outdir "
            + std::string(AEMU_PROJECT_ROOT_DIR) + "assembler/tests/preprocessor_test/build");
    p.run();
    ASSERT_TRUE(p.does_create_exe());

    LoadExecutable(*machine, p.get_exe_file()).load();
    machine->run(MAX_INSTRUCTIONS);

    ASSERT_EQ(machine->read_reg(0), 3);
}

TEST_F(EmulatorFixture, conditional_ifndef)
{
    Build p("-kp " + std::string(AEMU_PROJECT_ROOT_DIR)
            + "assembler/tests/preprocessor_test/src/conditional_ifndef.basm "
              "-outdir "
            + std::string(AEMU_PROJECT_ROOT_DIR) + "assembler/tests/preprocessor_test/build");
    p.run();
    ASSERT_TRUE(p.does_create_exe());

    LoadExecutable(*machine, p.get_exe_file()).load();
    machine->run(MAX_INSTRUCTIONS);

    ASSERT_EQ(machine->read_reg(0), 5);
}

TEST_F(EmulatorFixture, conditional_ifequ)
{
    Build p("-kp " + std::string(AEMU_PROJECT_ROOT_DIR)
            + "assembler/tests/preprocessor_test/src/conditional_ifequ.basm "
              "-outdir "
            + std::string(AEMU_PROJECT_ROOT_DIR) + "assembler/tests/preprocessor_test/build");
    p.run();
    ASSERT_TRUE(p.does_create_exe());

    LoadExecutable(*machine, p.get_exe_file()).load();
    machine->run(MAX_INSTRUCTIONS);

    ASSERT_EQ(machine->read_reg(0), 3);
}

TEST_F(EmulatorFixture, conditional_ifnequ)
{
    Build p("-kp " + std::string(AEMU_PROJECT_ROOT_DIR)
            + "assembler/tests/preprocessor_test/src/conditional_ifnequ.basm "
              "-outdir "
            + std::string(AEMU_PROJECT_ROOT_DIR) + "assembler/tests/preprocessor_test/build");
    p.run();
    ASSERT_TRUE(p.does_create_exe());

    LoadExecutable(*machine, p.get_exe_file()).load();
    machine->run(MAX_INSTRUCTIONS);

    ASSERT_EQ(machine->read_reg(0), 5);
}

TEST_F(EmulatorFixture, conditional_ifless_or_more)
{
    Build p("-kp " + std::string(AEMU_PROJECT_ROOT_DIR)
            + "assembler/tests/preprocessor_test/src/conditional_ifless_or_more.basm "
              "-outdir "
            + std::string(AEMU_PROJECT_ROOT_DIR) + "assembler/tests/preprocessor_test/build");
    p.run();
    ASSERT_TRUE(p.does_create_exe());

    LoadExecutable(*machine, p.get_exe_file()).load();
    machine->run(MAX_INSTRUCTIONS);

    ASSERT_EQ(machine->read_reg(0), 13);
}

TEST_F(EmulatorFixture, conditional_chain)
{
    Build p("-kp " + std::string(AEMU_PROJECT_ROOT_DIR)
            + "assembler/tests/preprocessor_test/src/conditional_chain.basm "
              "-outdir "
            + std::string(AEMU_PROJECT_ROOT_DIR) + "assembler/tests/preprocessor_test/build");
    p.run();
    ASSERT_TRUE(p.does_create_exe());

    LoadExecutable(*machine, p.get_exe_file()).load();
    machine->run(MAX_INSTRUCTIONS);

    ASSERT_EQ(machine->read_reg(0), 1);
    ASSERT_EQ(machine->read_reg(1), 2);
    ASSERT_EQ(machine->read_reg(2), 4);
    ASSERT_EQ(machine->read_reg(3), 16);
}
