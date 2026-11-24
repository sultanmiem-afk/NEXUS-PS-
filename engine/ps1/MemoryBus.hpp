#pragma once

#include "CpuCore.hpp"
#include "Ram.hpp"
#include "Scratchpad.hpp"

namespace nexus::ps1 {

// Connects implemented CPU-visible memory devices. Unimplemented BIOS/MMIO
// regions deliberately return false rather than pretending to contain RAM.
class MemoryBus final : public CpuBus {
public:
    MemoryBus() = default;

    Ram& ram() noexcept { return ram_; }
    const Ram& ram() const noexcept { return ram_; }
    Scratchpad& scratchpad() noexcept { return scratchpad_; }
    const Scratchpad& scratchpad() const noexcept { return scratchpad_; }

    void clear() noexcept;

    bool read8(std::uint32_t address, std::uint8_t& value) override;
    bool read16(std::uint32_t address, std::uint16_t& value) override;
    bool read32(std::uint32_t address, std::uint32_t& value) override;
    bool write8(std::uint32_t address, std::uint8_t value) override;
    bool write16(std::uint32_t address, std::uint16_t value) override;
    bool write32(std::uint32_t address, std::uint32_t value) override;

private:
    Ram ram_{};
    Scratchpad scratchpad_{};
};

} // namespace nexus::ps1
