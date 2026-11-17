cmake_minimum_required(VERSION 3.16)
project(dso_tron C)

# Само ядро собирается freestanding-компилятором через Makefile (нужны ld.lld и
# кросс-инструменты). Здесь — только host-тесты: они не требуют ни железа, ни QEMU.

option(DSO_HOST_TESTS "Собрать host-тесты планировщика (без железа)" ON)

if(DSO_HOST_TESTS)
    enable_testing()

    add_executable(test_sched tests/host/test_sched.c)
    target_include_directories(test_sched PRIVATE include)
    set_target_properties(test_sched PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)
    if(NOT MSVC)
        target_compile_options(test_sched PRIVATE -Wall -Wextra -Werror -O2)
    endif()
    add_test(NAME scheduler_determinism COMMAND test_sched)
endif()
