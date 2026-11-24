cmake_minimum_required(VERSION 3.22)
project(nexus_ps1_cpu LANGUAGES CXX)

add_library(nexus_ps1_cpu
    CpuState.cpp
    CpuCore.cpp
    Cop0.cpp
)
target_include_directories(nexus_ps1_cpu PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(nexus_ps1_cpu PUBLIC cxx_std_17)

option(NEXUS_PS1_BUILD_TESTS "Build CPU unit tests" ON)
if(NEXUS_PS1_BUILD_TESTS)
    enable_testing()
    add_executable(nexus_ps1_cpu_tests tests/CpuCoreTests.cpp)
    target_link_libraries(nexus_ps1_cpu_tests PRIVATE nexus_ps1_cpu)
    add_test(NAME nexus_ps1_cpu_tests COMMAND nexus_ps1_cpu_tests)
endif()
