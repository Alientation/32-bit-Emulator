cmake_minimum_required(VERSION 3.15)
project(emulator32bit_tests LANGUAGES CXX)

if(BUILD_TESTS)
    add_executable(emulator32bit_tests
        ./emulator_tests/emulator_test.cpp
        ./emulator_tests/fbl_test.cpp
        ./instruction_tests/hlt_test.cpp
        ./instruction_tests/add_test.cpp
        ./instruction_tests/sub_test.cpp
        ./instruction_tests/rsb_test.cpp
        ./instruction_tests/adc_test.cpp
        ./instruction_tests/sbc_test.cpp
        ./instruction_tests/rsc_test.cpp
        ./instruction_tests/mul_test.cpp
        ./instruction_tests/umull_test.cpp
        ./instruction_tests/smull_test.cpp
        ./instruction_tests/and_test.cpp
        ./instruction_tests/orr_test.cpp
        ./instruction_tests/eor_test.cpp
        ./instruction_tests/bic_test.cpp
        ./instruction_tests/lsl_test.cpp
        ./instruction_tests/lsr_test.cpp
        ./instruction_tests/asr_test.cpp
        ./instruction_tests/ror_test.cpp
        ./instruction_tests/cmp_test.cpp
        ./instruction_tests/cmn_test.cpp
        ./instruction_tests/tst_test.cpp
        ./instruction_tests/teq_test.cpp
        ./instruction_tests/mov_test.cpp
        ./instruction_tests/mvn_test.cpp
        ./instruction_tests/ldr_test.cpp
        ./instruction_tests/ldrb_test.cpp
        ./instruction_tests/ldrh_test.cpp
        ./instruction_tests/str_test.cpp
        ./instruction_tests/strb_test.cpp
        ./instruction_tests/strh_test.cpp
        ./instruction_tests/swp_test.cpp
    )

    target_include_directories(emulator32bit_tests
        PRIVATE
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    )

    target_link_libraries(emulator32bit_tests
        PRIVATE
            emulator32bit
            util
            GTest::gtest_main
    )

    target_compile_options(emulator32bit_tests PRIVATE
        -Wall -Wextra -Wpedantic
        $<$<CONFIG:Debug>:-g --coverage>
        $<$<CONFIG:RelWithDebInfo>:-O3 -flto=auto>
        $<$<CONFIG:Release>:-O3 -flto=auto>
    )

    target_link_options(emulator32bit_tests PRIVATE
        $<$<CONFIG:Debug>:--coverage>
        $<$<CONFIG:RelWithDebInfo>:-flto=auto>
        $<$<CONFIG:Release>:-flto=auto>
    )

    include(GoogleTest)
    gtest_discover_tests(emulator32bit_tests PROPERTIES LABELS emulator32bit)
endif()