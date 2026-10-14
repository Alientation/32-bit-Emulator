# Assembler pipeline: basm -> emu32
aemu_add_gtest(assembler_integration
    LABEL assembler_integration
    LIBS aemu::assembler
    SOURCES ./assembler_integration/assembler_integration_test.cpp
)

# Make sure the executables the tests shell out to are built first.
add_dependencies(assembler_integration emu32 basm)

# Paths to the executables, available to the tests as string literals.
target_compile_definitions(assembler_integration PRIVATE
    EMULATOR_PATH=\"$<TARGET_FILE:emu32>\"
    ASSEMBLER_PATH=\"$<TARGET_FILE:basm>\"
    PROGRAMS_DIR=\"${PROJECT_SOURCE_DIR}/app/programs\"
)

# C compiler pipeline: bcc -> basm -> emu32
aemu_add_gtest(ccompiler_integration
    LABEL ccompiler_integration
    LIBS aemu::util
    SOURCES ./ccompiler_integration/ccompiler_integration_test.cpp
)

# The test only runs the executables, util is for the integer types.
add_dependencies(ccompiler_integration emu32 basm bcc)

target_compile_definitions(ccompiler_integration PRIVATE
    EMULATOR_PATH=\"$<TARGET_FILE:emu32>\"
    ASSEMBLER_PATH=\"$<TARGET_FILE:basm>\"
    CCOMPILER_PATH=\"$<TARGET_FILE:bcc>\"
    CRT0_PATH=\"${PROJECT_SOURCE_DIR}/ccompiler/lib/crt0.basm\"
)
