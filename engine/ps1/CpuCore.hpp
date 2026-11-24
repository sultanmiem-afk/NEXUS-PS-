#pragma once

#include "CpuState.hpp"
#include "Cop0.hpp"

#include <cstdint>

namespace nexus::ps1 {

// The bus is supplied by the machine/memory subsystem. Implementations must
// perform PS1 address mapping (RAM mirrors, BIOS, scratchpad, and MMIO).
class CpuBus {
public:
    virtual ~CpuBus() = default;
    virtual bool read8(std::uint32_t address, std::uint8_t& value) = 0;
    virtual bool read16(std::uint32_t address, std::uint16_t& value) = 0;
    virtual bool read32(std::uint32_t address, std::uint32_t& value) = 0;
    virtual bool write8(std::uint32_t address, std::uint8_t value) = 0;
    virtual bool write16(std::uint32_t address, std::uint16_t value) = 0;
    virtual bool write32(std::uint32_t address, std::uint32_t value) = 0;
};

enum class CpuStepStatus : std::uint8_t {
    Executed,
    BusFault,
    AddressError,
    ReservedInstruction,
    ExceptionTaken
};

struct CpuStepResult {
    CpuStepStatus status = CpuStepStatus::Executed;
    std::uint32_t instruction_pc = 0;
    std::uint32_t instruction = 0;
    std::uint32_t cycles = 1;
};

// Portable interpreter foundation for the PS1's MIPS R3000A.
// Timing is intentionally instruction-level at this stage, not cycle-exact.
class CpuCore {
public:
    explicit CpuCore(CpuBus& bus) noexcept : bus_(bus) {}

    CpuState& state() noexcept { return state_; }
    const CpuState& state() const noexcept { return state_; }
    Cop0& cop0() noexcept { return cop0_; }
    const Cop0& cop0() const noexcept { return cop0_; }

    void reset(std::uint32_t reset_vector = 0xBFC00000u) noexcept;
    CpuStepResult step();

private:
    CpuBus& bus_;
    CpuState state_{};
    Cop0 cop0_{};
    bool in_delay_slot_ = false;

    void write_reg(std::uint32_t index, std::uint32_t value) noexcept;
    void branch(std::uint32_t target) noexcept;
};

} // namespace nexus::ps1
