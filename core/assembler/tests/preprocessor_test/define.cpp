#include "assembler_test/assembler_test.h"

TEST_F (EmulatorFixture, define_no_args)
{
    Process p ("-kp " + AEMU_PROJECT_ROOT_DIR
               + "core/assembler/tests/preprocessor_test/src/define_no_args.basm "
                 "-outdir "
               + AEMU_PROJECT_ROOT_DIR + "core/assembler/tests/preprocessor_test/build");
    ASSERT_TRUE (p.does_create_exe ());

    LoadExecutable loader (*machine, p.get_exe_file ());
    machine->run (MAX_INSTRUCTIONS);

    ASSERT_EQ (machine->read_reg (0), 13);
}

TEST_F (EmulatorFixture, define_no_args_multiline)
{
    Process p ("-kp " + AEMU_PROJECT_ROOT_DIR
               + "core/assembler/tests/preprocessor_test/src/define_no_args_multiline.basm "
                 "-outdir "
               + AEMU_PROJECT_ROOT_DIR + "core/assembler/tests/preprocessor_test/build");
    ASSERT_TRUE (p.does_create_exe ());

    LoadExecutable loader (*machine, p.get_exe_file ());
    machine->run (MAX_INSTRUCTIONS);

    ASSERT_EQ (machine->read_reg (0), 13);
}

TEST_F (EmulatorFixture, define_args)
{
    Process p ("-kp " + AEMU_PROJECT_ROOT_DIR
               + "core/assembler/tests/preprocessor_test/src/define_args.basm "
                 "-outdir "
               + AEMU_PROJECT_ROOT_DIR + "core/assembler/tests/preprocessor_test/build");
    ASSERT_TRUE (p.does_create_exe ());

    LoadExecutable loader (*machine, p.get_exe_file ());
    machine->run (MAX_INSTRUCTIONS);

    ASSERT_EQ (machine->read_reg (0), 13);
}

TEST_F (EmulatorFixture, define_args_multiline)
{
    Process p ("-kp " + AEMU_PROJECT_ROOT_DIR
               + "core/assembler/tests/preprocessor_test/src/define_args_multiline.basm "
                 "-outdir "
               + AEMU_PROJECT_ROOT_DIR + "core/assembler/tests/preprocessor_test/build");
    ASSERT_TRUE (p.does_create_exe ());

    LoadExecutable loader (*machine, p.get_exe_file ());
    machine->run (MAX_INSTRUCTIONS);

    ASSERT_EQ (machine->read_reg (0), 13);
}

TEST_F (EmulatorFixture, define_redefine)
{
    Process p ("-kp " + AEMU_PROJECT_ROOT_DIR
               + "core/assembler/tests/preprocessor_test/src/define_redefine.basm "
                 "-outdir "
               + AEMU_PROJECT_ROOT_DIR + "core/assembler/tests/preprocessor_test/build");
    ASSERT_TRUE (p.does_create_exe ());

    LoadExecutable loader (*machine, p.get_exe_file ());
    machine->run (MAX_INSTRUCTIONS);

    ASSERT_EQ (machine->read_reg (0), 13);
}

TEST_F (EmulatorFixture, define_undefine)
{
    Process p ("-kp " + AEMU_PROJECT_ROOT_DIR
               + "core/assembler/tests/preprocessor_test/src/define_undefine.basm "
                 "-outdir "
               + AEMU_PROJECT_ROOT_DIR + "core/assembler/tests/preprocessor_test/build");
    ASSERT_TRUE (p.does_create_exe ());

    LoadExecutable loader (*machine, p.get_exe_file ());
    machine->run (MAX_INSTRUCTIONS);

    ASSERT_EQ (machine->read_reg (0), 13);
}
