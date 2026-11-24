#pragma once

#include <cstdint>

namespace nexus::ps1 {

// Initial Timer 2 model: 16-bit counter with system-clock and system-clock/8 inputs.
class Timer2 final {
public:
    static constexpr std::uint32_t kCounterAddress = 0x1F801120u;
    static constexpr std::uint32_t kModeAddress = 0x1F801124u;
    static constexpr std::uint32_t kTargetAddress = 0x1F801128u;

    void reset() noexcept;
    std::uint32_t counter() const noexcept { return counter_; }
    std::uint32_t mode() const noexcept;
    std::uint32_t target() const noexcept { return target_; }

    // Returns true when this advance generates a Timer 2 IRQ event.
    bool advance(std::uint32_t cpuCycles) noexcept;
    bool read32(std::uint32_t address, std::uint32_t& value) noexcept;
    bool write32(std::uint32_t address, std::uint32_t value) noexcept;

private:
    static constexpr std::uint32_t kSyncEnable = 1u << 0;
    static constexpr std::uint32_t kResetAtTarget = 1u << 3;
    static constexpr std::uint32_t kIrqAtTarget = 1u << 4;
    static constexpr std::uint32_t kIrqAtOverflow = 1u << 5;
    static constexpr std::uint32_t kIrqRepeat = 1u << 6;
    static constexpr std::uint32_t kIrqToggle = 1u << 7;
    static constexpr std::uint32_t kClockSourceMask = 3u << 8;

    bool advanceTicks(std::uint32_t ticks) noexcept;

    std::uint16_t counter_ = 0;
    std::uint16_t target_ = 0;
    std::uint32_t mode_ = 0;
    std::uint8_t dividerPhase_ = 0;
    std::uint8_t irqPulseTicks_ = 0;
    bool reachedTarget_ = false;
    bool reachedOverflow_ = false;
    bool irqArmed_ = true;
    bool irqRequest_ = true;
};

} // namespace nexus::ps1
