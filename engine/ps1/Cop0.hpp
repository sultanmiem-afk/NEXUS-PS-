#pragma once

#include "CpuState.hpp"

#include <array>
#include <cstdint>

namespace nexus::ps1 {

enum class CpuException : std::uint8_t {
    Interrupt = 0,
    AddressLoad = 4,
    AddressStore = 5,
    Syscall = 8,
    Breakpoint = 9,
    ReservedInstruction = 10,
    CoprocessorUnusable = 11,
    ArithmeticOverflow = 12
};

// MIPS R3000A system-control coprocessor (CP0) register file and exception
// entry/return mechanics. Register numbers follow the architectural encoding.
class Cop0 {
public:
    static constexpr std::uint32_t kStatus = 12;
    static constexpr std::uint32_t kCause = 13;
    static constexpr std::uint32_t kEpc = 14;
    static constexpr std::uint32_t kBadVaddr = 8;
    static constexpr std::uint32_t kStatusBev = 1u << 22;
    static constexpr std::uint32_t kCauseBranchDelay = 1u << 31;

    void reset() noexcept;
    std::uint32_t read(std::uint32_t index) const noexcept;
    void write(std::uint32_t index, std::uint32_t value) noexcept;
    void enter_exception(CpuState& cpu, CpuException code,
                         std::uint32_t fault_pc, bool in_delay_slot,
                         std::uint32_t bad_vaddr = 0) noexcept;
    void return_from_exception() noexcept;
    void setHardwareInterruptPending(bool pending) noexcept;

private:
    std::array<std::uint32_t, 16> regs_{};
};

} // namespace nexus::ps1
