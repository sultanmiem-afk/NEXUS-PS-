#pragma once

#include "BiosRom.hpp"
#include "Cdrom.hpp"
#include "CpuCore.hpp"
#include "DmaController.hpp"
#include "InterruptController.hpp"
#include "Ram.hpp"
#include "Scratchpad.hpp"
#include "Timer0.hpp"
#include "Timer1.hpp"
#include "Timer2.hpp"

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
    Timer0& timer0() noexcept { return timer0_; }
    const Timer0& timer0() const noexcept { return timer0_; }
    Timer1& timer1() noexcept { return timer1_; }
    const Timer1& timer1() const noexcept { return timer1_; }
    Timer2& timer2() noexcept { return timer2_; }
    const Timer2& timer2() const noexcept { return timer2_; }
    InterruptController& interrupts() noexcept { return interrupts_; }
    const InterruptController& interrupts() const noexcept { return interrupts_; }
    Cdrom& cdrom() noexcept { return cdrom_; }
    const Cdrom& cdrom() const noexcept { return cdrom_; }
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
    Cdrom cdrom_{};
    DmaController dma_{};
    InterruptController interrupts_{};
    Timer0 timer0_{};
    Timer1 timer1_{};
    Timer2 timer2_{};
};

} // namespace nexus::ps1
