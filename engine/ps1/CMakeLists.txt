cmake_minimum_required(VERSION 3.22)
project(nexus_ps1_cpu LANGUAGES CXX)

add_library(nexus_ps1_cpu
    CpuState.cpp
    CpuCore.cpp
    Cop0.cpp
    Ram.cpp
    Scratchpad.cpp
    BiosRom.cpp
    Vram.cpp
    Gpu.cpp
    MemoryBus.cpp
    DmaController.cpp
    InterruptController.cpp
    Timer0.cpp
    Timer1.cpp
    Timer2.cpp
    Ps1System.cpp
)
target_include_directories(nexus_ps1_cpu PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(nexus_ps1_cpu PUBLIC cxx_std_17)

option(NEXUS_PS1_BUILD_TESTS "Build CPU unit tests" ON)
if(NEXUS_PS1_BUILD_TESTS)
    enable_testing()
    add_executable(nexus_ps1_cpu_tests tests/CpuCoreTests.cpp)
    target_link_libraries(nexus_ps1_cpu_tests PRIVATE nexus_ps1_cpu)
    add_test(NAME nexus_ps1_cpu_tests COMMAND nexus_ps1_cpu_tests)

    add_executable(nexus_ps1_ram_tests tests/RamTests.cpp)
    target_link_libraries(nexus_ps1_ram_tests PRIVATE nexus_ps1_cpu)
    add_test(NAME nexus_ps1_ram_tests COMMAND nexus_ps1_ram_tests)

    add_executable(nexus_ps1_memory_tests tests/MemoryBusTests.cpp)
    target_link_libraries(nexus_ps1_memory_tests PRIVATE nexus_ps1_cpu)
    add_test(NAME nexus_ps1_memory_tests COMMAND nexus_ps1_memory_tests)

    add_executable(nexus_ps1_bios_vram_tests tests/BiosVramTests.cpp)
    target_link_libraries(nexus_ps1_bios_vram_tests PRIVATE nexus_ps1_cpu)
    add_test(NAME nexus_ps1_bios_vram_tests COMMAND nexus_ps1_bios_vram_tests)

    add_executable(nexus_ps1_dma_tests tests/DmaControllerTests.cpp)
    target_link_libraries(nexus_ps1_dma_tests PRIVATE nexus_ps1_cpu)
    add_test(NAME nexus_ps1_dma_tests COMMAND nexus_ps1_dma_tests)

    add_executable(nexus_ps1_gpu_tests tests/GpuTests.cpp)
    target_link_libraries(nexus_ps1_gpu_tests PRIVATE nexus_ps1_cpu)
    add_test(NAME nexus_ps1_gpu_tests COMMAND nexus_ps1_gpu_tests)

    add_executable(nexus_ps1_interrupt_tests tests/InterruptControllerTests.cpp)
    target_link_libraries(nexus_ps1_interrupt_tests PRIVATE nexus_ps1_cpu)
    add_test(NAME nexus_ps1_interrupt_tests COMMAND nexus_ps1_interrupt_tests)

    add_executable(nexus_ps1_timer0_tests tests/Timer0Tests.cpp)
    target_link_libraries(nexus_ps1_timer0_tests PRIVATE nexus_ps1_cpu)
    add_test(NAME nexus_ps1_timer0_tests COMMAND nexus_ps1_timer0_tests)

    add_executable(nexus_ps1_timer1_tests tests/Timer1Tests.cpp)
    target_link_libraries(nexus_ps1_timer1_tests PRIVATE nexus_ps1_cpu)
    add_test(NAME nexus_ps1_timer1_tests COMMAND nexus_ps1_timer1_tests)

    add_executable(nexus_ps1_timer2_tests tests/Timer2Tests.cpp)
    target_link_libraries(nexus_ps1_timer2_tests PRIVATE nexus_ps1_cpu)
    add_test(NAME nexus_ps1_timer2_tests COMMAND nexus_ps1_timer2_tests)

    add_executable(nexus_ps1_system_tests tests/Ps1SystemTests.cpp)
    target_link_libraries(nexus_ps1_system_tests PRIVATE nexus_ps1_cpu)
    add_test(NAME nexus_ps1_system_tests COMMAND nexus_ps1_system_tests)
endif()
