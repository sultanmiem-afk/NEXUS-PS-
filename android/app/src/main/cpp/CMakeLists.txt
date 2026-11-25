cmake_minimum_required(VERSION 3.22.1)
project(nexus_ps1_android LANGUAGES CXX)

get_filename_component(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../.." ABSOLUTE)
set(PS1_DIR "${REPO_ROOT}/engine/ps1")

add_library(nexus_ps1_core STATIC
    ${PS1_DIR}/CpuState.cpp
    ${PS1_DIR}/CpuCore.cpp
    ${PS1_DIR}/Cop0.cpp
    ${PS1_DIR}/Ram.cpp
    ${PS1_DIR}/Scratchpad.cpp
    ${PS1_DIR}/BiosRom.cpp
    ${PS1_DIR}/Cdrom.cpp
    ${PS1_DIR}/Vram.cpp
    ${PS1_DIR}/Gpu.cpp
    ${PS1_DIR}/MemoryBus.cpp
    ${PS1_DIR}/DmaController.cpp
    ${PS1_DIR}/InterruptController.cpp
    ${PS1_DIR}/Timer0.cpp
    ${PS1_DIR}/Timer1.cpp
    ${PS1_DIR}/Timer2.cpp
    ${PS1_DIR}/Spu.cpp
    ${PS1_DIR}/Ps1System.cpp
)

target_include_directories(nexus_ps1_core PUBLIC ${PS1_DIR})
target_compile_features(nexus_ps1_core PUBLIC cxx_std_17)
set_target_properties(nexus_ps1_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(nexus_ps1_android SHARED native-lib.cpp)
target_compile_features(nexus_ps1_android PRIVATE cxx_std_17)
target_link_libraries(nexus_ps1_android PRIVATE nexus_ps1_core)
