#pragma once

#include <array>
#include <cstdint>

namespace nexus::ps1 {

// Architectural state of the PlayStation's MIPS R3000A CPU.
// Register zero is hard-wired to zero and is enforced by the core.
struct CpuState {
    std::array<std::uint32_t, 32> gpr{};
    std::uint32_t hi = 0;
    std::uint32_t lo = 0;
    std::uint32_t pc = 0;
    std::uint32_t next_pc = 4;

    void reset(std::uint32_t reset_vector = 0xBFC00000u) noexcept;
};

struct Instruction {
    explicit constexpr Instruction(std::uint32_t value = 0) noexcept : raw(value) {}

    std::uint32_t raw;

    constexpr std::uint32_t opcode() const noexcept { return (raw >> 26) & 0x3Fu; }
    constexpr std::uint32_t rs() const noexcept { return (raw >> 21) & 0x1Fu; }
    constexpr std::uint32_t rt() const noexcept { return (raw >> 16) & 0x1Fu; }
    constexpr std::uint32_t rd() const noexcept { return (raw >> 11) & 0x1Fu; }
    constexpr std::uint32_t shift_amount() const noexcept { return (raw >> 6) & 0x1Fu; }
    constexpr std::uint32_t function() const noexcept { return raw & 0x3Fu; }
    constexpr std::uint16_t immediate() const noexcept { return static_cast<std::uint16_t>(raw); }
    constexpr std::uint32_t jump_index() const noexcept { return raw & 0x03FFFFFFu; }

    constexpr std::int32_t signed_immediate() const noexcept {
        return static_cast<std::int16_t>(immediate());
    }
};

} // namespace nexus::ps1
