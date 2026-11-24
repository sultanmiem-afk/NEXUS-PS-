#pragma once

#include "CpuCore.hpp"
#include "Gpu.hpp"
#include "MemoryBus.hpp"
#include "Vram.hpp"

#include <cstdint>
#include <vector>

namespace nexus::ps1 {

struct SystemStepResult {
    CpuStepResult cpu{};
    bool dmaTransferred = false;
    std::uint32_t dmaWords = 0;
    bool timer0Irq = false;
    bool timer1Irq = false;
};

// First machine-level integration: CPU-visible memory, GPU ports, VRAM and
// synchronous channel-2 GPU DMA. Timing and the remaining peripherals are TODO.
class Ps1System final {
public:
    Ps1System() noexcept;

    void reset(std::uint32_t resetVector = 0xBFC00000u) noexcept;
    bool loadBios(const std::vector<std::uint8_t>& image) noexcept;

    SystemStepResult step();

    CpuCore& cpu() noexcept { return cpu_; }
    const CpuCore& cpu() const noexcept { return cpu_; }
    MemoryBus& memory() noexcept { return memory_; }
    const MemoryBus& memory() const noexcept { return memory_; }
    Gpu& gpu() noexcept { return gpu_; }
    const Gpu& gpu() const noexcept { return gpu_; }
    Vram& vram() noexcept { return vram_; }
    const Vram& vram() const noexcept { return vram_; }

private:
    class SystemBus final : public CpuBus {
    public:
        SystemBus(MemoryBus& memory, Gpu& gpu) noexcept : memory_(memory), gpu_(gpu) {}
        bool read8(std::uint32_t address, std::uint8_t& value) override;
        bool read16(std::uint32_t address, std::uint16_t& value) override;
        bool read32(std::uint32_t address, std::uint32_t& value) override;
        bool write8(std::uint32_t address, std::uint8_t value) override;
        bool write16(std::uint32_t address, std::uint16_t value) override;
        bool write32(std::uint32_t address, std::uint32_t value) override;
    private:
        MemoryBus& memory_;
        Gpu& gpu_;
    };

    MemoryBus memory_{};
    Vram vram_{};
    Gpu gpu_;
    SystemBus bus_;
    CpuCore cpu_;
};

} // namespace nexus::ps1
