#pragma once

#include "BiosRom.hpp"
#include "CpuCore.hpp"
#include "DmaController.hpp"
#include "InterruptController.hpp"
#include "Ram.hpp"
#include "Scratchpad.hpp"

namespace nexus::ps1 {

// CPU-visible bus. VRAM remains owned by the GPU path; it is not a flat
// CPU-addressable RAM region on the original PlayStation.
class MemoryBus final : public CpuBus {
public:
    MemoryBus() = default;

    Ram& ram() noexcept { return ram_; }
    const Ram& ram() const noexcept { return ram_; }
    Scratchpad& scratchpad() noexcept { return scratchpad_; }
    const Scratchpad& scratchpad() const noexcept { return scratchpad_; }
    DmaController& dma() noexcept { return dma_; }
    const DmaController& dma() const noexcept { return dma_; }
    InterruptController& interrupts() noexcept { return interrupts_; }
    const InterruptController& interrupts() const noexcept { return interrupts_; }
    BiosRom& bios() noexcept { return bios_; }
    const BiosRom& bios() const noexcept { return bios_; }

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
    BiosRom bios_{};
    DmaController dma_{};
    InterruptController interrupts_{};
};

} // namespace nexus::ps1
