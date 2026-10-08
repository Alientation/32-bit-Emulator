aemu_add_gtest(emulator32bit_tests
    LABEL emulator32bit
    LIBS aemu::emulator32bit
    SOURCES
        ./alu_tests/alu_test.cpp
        ./emulator_tests/emulator_test.cpp
        ./emulator_tests/fbl_test.cpp
        ./instruction_tests/hlt_test.cpp
        ./instruction_tests/dataproc_test.cpp
        ./instruction_tests/shift_test.cpp
        ./instruction_tests/long_multiply_test.cpp
        ./instruction_tests/move_test.cpp
        ./instruction_tests/memory_test.cpp
        ./instruction_tests/tlbi_test.cpp
        ./instruction_tests/swp_test.cpp
)
