// End to end tests of the preprocessor: each program in programs/ is built, loaded and run, and the
// result is read from x0.

#include "assembler_test/assembler_test.h"

TEST_F(EmulatorFixture, include_local_dir)
{
    Build p("-kp " + std::string(AEMU_PROJECT_ROOT_DIR)
            + "assembler/tests/programs/src/include_local_dir.basm "
              "-outdir "
            + std::string(AEMU_PROJECT_ROOT_DIR) + "assembler/tests/programs/build");
    p.run();
    ASSERT_TRUE(p.does_create_exe());

    LoadExecutable(*machine, p.get_exe_file()).load();
    machine->run(MAX_INSTRUCTIONS);

    ASSERT_EQ(machine->read_reg(0), 0x135);
}

TEST_F(EmulatorFixture, include_system_dir)
{
    Build p("-kp " + std::string(AEMU_PROJECT_ROOT_DIR)
            + "assembler/tests/programs/src/include_sys_dir.basm " + "-I "
            + std::string(AEMU_PROJECT_ROOT_DIR) + "assembler/tests/programs/include/system "
            + "-outdir " + std::string(AEMU_PROJECT_ROOT_DIR) + "assembler/tests/programs/build");
    p.run();
    ASSERT_TRUE(p.does_create_exe());

    LoadExecutable(*machine, p.get_exe_file()).load();
    machine->run(MAX_INSTRUCTIONS);

    ASSERT_EQ(machine->read_reg(0), 0x531);
}

TEST_F(EmulatorFixture, define_no_args)
{
    Build p("-kp " + std::string(AEMU_PROJECT_ROOT_DIR)
            + "assembler/tests/programs/src/define_no_args.basm "
              "-outdir "
            + std::string(AEMU_PROJECT_ROOT_DIR) + "assembler/tests/programs/build");
    p.run();
    ASSERT_TRUE(p.does_create_exe());

    LoadExecutable(*machine, p.get_exe_file()).load();
    machine->run(MAX_INSTRUCTIONS);

    ASSERT_EQ(machine->read_reg(0), 13);
}

TEST_F(EmulatorFixture, define_no_args_multiline)
{
    Build p("-kp " + std::string(AEMU_PROJECT_ROOT_DIR)
            + "assembler/tests/programs/src/define_no_args_multiline.basm "
              "-outdir "
            + std::string(AEMU_PROJECT_ROOT_DIR) + "assembler/tests/programs/build");
    p.run();
    ASSERT_TRUE(p.does_create_exe());

    LoadExecutable(*machine, p.get_exe_file()).load();
    machine->run(MAX_INSTRUCTIONS);

    ASSERT_EQ(machine->read_reg(0), 13);
}

TEST_F(EmulatorFixture, define_args)
{
    Build p("-kp " + std::string(AEMU_PROJECT_ROOT_DIR)
            + "assembler/tests/programs/src/define_args.basm "
              "-outdir "
            + std::string(AEMU_PROJECT_ROOT_DIR) + "assembler/tests/programs/build");
    p.run();
    ASSERT_TRUE(p.does_create_exe());

    LoadExecutable(*machine, p.get_exe_file()).load();
    machine->run(MAX_INSTRUCTIONS);

    ASSERT_EQ(machine->read_reg(0), 13);
}

TEST_F(EmulatorFixture, define_args_multiline)
{
    Build p("-kp " + std::string(AEMU_PROJECT_ROOT_DIR)
            + "assembler/tests/programs/src/define_args_multiline.basm "
              "-outdir "
            + std::string(AEMU_PROJECT_ROOT_DIR) + "assembler/tests/programs/build");
    p.run();
    ASSERT_TRUE(p.does_create_exe());

    LoadExecutable(*machine, p.get_exe_file()).load();
    machine->run(MAX_INSTRUCTIONS);

    ASSERT_EQ(machine->read_reg(0), 13);
}

TEST_F(EmulatorFixture, define_redefine)
{
    Build p("-kp " + std::string(AEMU_PROJECT_ROOT_DIR)
            + "assembler/tests/programs/src/define_redefine.basm "
              "-outdir "
            + std::string(AEMU_PROJECT_ROOT_DIR) + "assembler/tests/programs/build");
    p.run();
    ASSERT_TRUE(p.does_create_exe());

    LoadExecutable(*machine, p.get_exe_file()).load();
    machine->run(MAX_INSTRUCTIONS);

    ASSERT_EQ(machine->read_reg(0), 13);
}

TEST_F(EmulatorFixture, define_undefine)
{
    Build p("-kp " + std::string(AEMU_PROJECT_ROOT_DIR)
            + "assembler/tests/programs/src/define_undefine.basm "
              "-outdir "
            + std::string(AEMU_PROJECT_ROOT_DIR) + "assembler/tests/programs/build");
    p.run();
    ASSERT_TRUE(p.does_create_exe());

    LoadExecutable(*machine, p.get_exe_file()).load();
    machine->run(MAX_INSTRUCTIONS);

    ASSERT_EQ(machine->read_reg(0), 13);
}

TEST_F(EmulatorFixture, macro_no_args)
{
    Build p("-kp " + std::string(AEMU_PROJECT_ROOT_DIR)
            + "assembler/tests/programs/src/macro_no_args.basm "
              "-outdir "
            + std::string(AEMU_PROJECT_ROOT_DIR) + "assembler/tests/programs/build");
    p.run();
    ASSERT_TRUE(p.does_create_exe());

    LoadExecutable(*machine, p.get_exe_file()).load();
    machine->run(MAX_INSTRUCTIONS);

    ASSERT_EQ(machine->read_reg(0), 10);
}

TEST_F(EmulatorFixture, macro_args)
{
    Build p("-kp " + std::string(AEMU_PROJECT_ROOT_DIR)
            + "assembler/tests/programs/src/macro_args.basm "
              "-outdir "
            + std::string(AEMU_PROJECT_ROOT_DIR) + "assembler/tests/programs/build");
    p.run();
    ASSERT_TRUE(p.does_create_exe());

    LoadExecutable(*machine, p.get_exe_file()).load();
    machine->run(MAX_INSTRUCTIONS);

    ASSERT_EQ(machine->read_reg(0), 10);
}

TEST_F(EmulatorFixture, macro_ret)
{
    Build p("-kp " + std::string(AEMU_PROJECT_ROOT_DIR)
            + "assembler/tests/programs/src/macro_ret.basm "
              "-outdir "
            + std::string(AEMU_PROJECT_ROOT_DIR) + "assembler/tests/programs/build");
    p.run();
    ASSERT_TRUE(p.does_create_exe());

    LoadExecutable(*machine, p.get_exe_file()).load();
    machine->run(MAX_INSTRUCTIONS);

    ASSERT_EQ(machine->read_reg(0), 10);
}

TEST_F(EmulatorFixture, conditional_ifdef)
{
    Build p("-kp " + std::string(AEMU_PROJECT_ROOT_DIR)
            + "assembler/tests/programs/src/conditional_ifdef.basm "
              "-outdir "
            + std::string(AEMU_PROJECT_ROOT_DIR) + "assembler/tests/programs/build");
    p.run();
    ASSERT_TRUE(p.does_create_exe());

    LoadExecutable(*machine, p.get_exe_file()).load();
    machine->run(MAX_INSTRUCTIONS);

    ASSERT_EQ(machine->read_reg(0), 3);
}

TEST_F(EmulatorFixture, conditional_ifndef)
{
    Build p("-kp " + std::string(AEMU_PROJECT_ROOT_DIR)
            + "assembler/tests/programs/src/conditional_ifndef.basm "
              "-outdir "
            + std::string(AEMU_PROJECT_ROOT_DIR) + "assembler/tests/programs/build");
    p.run();
    ASSERT_TRUE(p.does_create_exe());

    LoadExecutable(*machine, p.get_exe_file()).load();
    machine->run(MAX_INSTRUCTIONS);

    ASSERT_EQ(machine->read_reg(0), 5);
}

TEST_F(EmulatorFixture, conditional_ifequ)
{
    Build p("-kp " + std::string(AEMU_PROJECT_ROOT_DIR)
            + "assembler/tests/programs/src/conditional_ifequ.basm "
              "-outdir "
            + std::string(AEMU_PROJECT_ROOT_DIR) + "assembler/tests/programs/build");
    p.run();
    ASSERT_TRUE(p.does_create_exe());

    LoadExecutable(*machine, p.get_exe_file()).load();
    machine->run(MAX_INSTRUCTIONS);

    ASSERT_EQ(machine->read_reg(0), 3);
}

TEST_F(EmulatorFixture, conditional_ifnequ)
{
    Build p("-kp " + std::string(AEMU_PROJECT_ROOT_DIR)
            + "assembler/tests/programs/src/conditional_ifnequ.basm "
              "-outdir "
            + std::string(AEMU_PROJECT_ROOT_DIR) + "assembler/tests/programs/build");
    p.run();
    ASSERT_TRUE(p.does_create_exe());

    LoadExecutable(*machine, p.get_exe_file()).load();
    machine->run(MAX_INSTRUCTIONS);

    ASSERT_EQ(machine->read_reg(0), 5);
}

TEST_F(EmulatorFixture, conditional_ifless_or_more)
{
    Build p("-kp " + std::string(AEMU_PROJECT_ROOT_DIR)
            + "assembler/tests/programs/src/conditional_ifless_or_more.basm "
              "-outdir "
            + std::string(AEMU_PROJECT_ROOT_DIR) + "assembler/tests/programs/build");
    p.run();
    ASSERT_TRUE(p.does_create_exe());

    LoadExecutable(*machine, p.get_exe_file()).load();
    machine->run(MAX_INSTRUCTIONS);

    ASSERT_EQ(machine->read_reg(0), 13);
}

TEST_F(EmulatorFixture, conditional_chain)
{
    Build p("-kp " + std::string(AEMU_PROJECT_ROOT_DIR)
            + "assembler/tests/programs/src/conditional_chain.basm "
              "-outdir "
            + std::string(AEMU_PROJECT_ROOT_DIR) + "assembler/tests/programs/build");
    p.run();
    ASSERT_TRUE(p.does_create_exe());

    LoadExecutable(*machine, p.get_exe_file()).load();
    machine->run(MAX_INSTRUCTIONS);

    ASSERT_EQ(machine->read_reg(0), 1);
    ASSERT_EQ(machine->read_reg(1), 2);
    ASSERT_EQ(machine->read_reg(2), 4);
    ASSERT_EQ(machine->read_reg(3), 16);
}
