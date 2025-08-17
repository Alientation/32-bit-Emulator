#include "assembler_test/assembler_test.h"

TEST_F (EmulatorFixture, macro_no_args)
{
    Process p ("-kp " + AEMU_PROJECT_ROOT_DIR
               + "core/assembler/tests/preprocessor_test/src/macro_no_args.basm "
                 "-outdir "
               + AEMU_PROJECT_ROOT_DIR + "core/assembler/tests/preprocessor_test/build");
    ASSERT_TRUE (p.does_create_exe ());

    LoadExecutable loader (*machine, p.get_exe_file ());
    machine->run (MAX_INSTRUCTIONS);

    ASSERT_EQ (machine->read_reg (0), 10);
}

TEST_F (EmulatorFixture, macro_args)
{
    Process p ("-kp " + AEMU_PROJECT_ROOT_DIR
               + "core/assembler/tests/preprocessor_test/src/macro_args.basm "
                 "-outdir "
               + AEMU_PROJECT_ROOT_DIR + "core/assembler/tests/preprocessor_test/build");
    ASSERT_TRUE (p.does_create_exe ());

    LoadExecutable loader (*machine, p.get_exe_file ());
    machine->run (MAX_INSTRUCTIONS);

    ASSERT_EQ (machine->read_reg (0), 10);
}

TEST_F (EmulatorFixture, macro_ret)
{
    Process p ("-kp " + AEMU_PROJECT_ROOT_DIR
               + "core/assembler/tests/preprocessor_test/src/macro_ret.basm "
                 "-outdir "
               + AEMU_PROJECT_ROOT_DIR + "core/assembler/tests/preprocessor_test/build");
    ASSERT_TRUE (p.does_create_exe ());

    LoadExecutable loader (*machine, p.get_exe_file ());
    machine->run (MAX_INSTRUCTIONS);

    ASSERT_EQ (machine->read_reg (0), 10);
}