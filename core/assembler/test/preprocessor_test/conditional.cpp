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

TEST_F (EmulatorFixture, conditional_ifndef)
{
    Process p ("-kp " + AEMU_PROJECT_ROOT_DIR +
            "core/assembler/test/preprocessor_test/src/conditional_ifndef.basm "
            "-outdir " + AEMU_PROJECT_ROOT_DIR +
            "core/assembler/test/preprocessor_test/build");
    ASSERT_TRUE (p.does_create_exe ());

    LoadExecutable loader(*machine, p.get_exe_file());
    machine->run(MAX_INSTRUCTIONS);

    ASSERT_EQ(machine->read_reg(0), 5);
}

TEST_F (EmulatorFixture, conditional_ifequ)
{
    Process p ("-kp " + AEMU_PROJECT_ROOT_DIR +
            "core/assembler/test/preprocessor_test/src/conditional_ifequ.basm "
            "-outdir " + AEMU_PROJECT_ROOT_DIR +
            "core/assembler/test/preprocessor_test/build");
    ASSERT_TRUE (p.does_create_exe ());

    LoadExecutable loader(*machine, p.get_exe_file());
    machine->run(MAX_INSTRUCTIONS);

    ASSERT_EQ(machine->read_reg(0), 3);
}

TEST_F (EmulatorFixture, conditional_ifnequ)
{
    Process p ("-kp " + AEMU_PROJECT_ROOT_DIR +
            "core/assembler/test/preprocessor_test/src/conditional_ifnequ.basm "
            "-outdir " + AEMU_PROJECT_ROOT_DIR +
            "core/assembler/test/preprocessor_test/build");
    ASSERT_TRUE (p.does_create_exe ());

    LoadExecutable loader(*machine, p.get_exe_file());
    machine->run(MAX_INSTRUCTIONS);

    ASSERT_EQ(machine->read_reg(0), 5);
}

TEST_F (EmulatorFixture, conditional_ifless_or_more)
{
    Process p ("-kp " + AEMU_PROJECT_ROOT_DIR +
            "core/assembler/test/preprocessor_test/src/conditional_ifless_or_more.basm "
            "-outdir " + AEMU_PROJECT_ROOT_DIR +
            "core/assembler/test/preprocessor_test/build");
    ASSERT_TRUE (p.does_create_exe ());

    LoadExecutable loader(*machine, p.get_exe_file());
    machine->run(MAX_INSTRUCTIONS);

    ASSERT_EQ(machine->read_reg(0), 13);
}

TEST_F (EmulatorFixture, conditional_chain)
{
    Process p ("-kp " + AEMU_PROJECT_ROOT_DIR +
            "core/assembler/test/preprocessor_test/src/conditional_chain.basm "
            "-outdir " + AEMU_PROJECT_ROOT_DIR +
            "core/assembler/test/preprocessor_test/build");
    ASSERT_TRUE (p.does_create_exe ());

    LoadExecutable loader(*machine, p.get_exe_file());
    machine->run(MAX_INSTRUCTIONS);

    ASSERT_EQ(machine->read_reg(0), 1);
    ASSERT_EQ(machine->read_reg(1), 2);
    ASSERT_EQ(machine->read_reg(2), 4);
    ASSERT_EQ(machine->read_reg(3), 16);
}
