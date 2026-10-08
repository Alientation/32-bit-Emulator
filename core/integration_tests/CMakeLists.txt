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
)

# TODO: Comment back in when ccompiler is implemented.
# ccompiler pipeline: ccompiler_exec -> basm -> emu32
# aemu_add_gtest(ccompiler_integration
#     LABEL ccompiler_integration
#     LIBS aemu::assembler ccompiler
#     SOURCES ./ccompiler_integration/ccompiler_integration_test.cpp
# )

# add_dependencies(ccompiler_integration emu32 basm ccompiler_exec)

# target_compile_definitions(ccompiler_integration PRIVATE
#     EMULATOR_PATH=\"$<TARGET_FILE:emu32>\"
#     ASSEMBLER_PATH=\"$<TARGET_FILE:basm>\"
#     CCOMPILER_PATH=\"$<TARGET_FILE:ccompiler_exec>\"
# )
