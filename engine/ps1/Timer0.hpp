#pragma once
#include <cstdint>
namespace nexus::ps1 {
class Timer0 final {
public:
    static constexpr std::uint32_t kCounterAddress=0x1F801100u, kModeAddress=0x1F801104u, kTargetAddress=0x1F801108u;
    void reset() noexcept;
    std::uint32_t counter() const noexcept { return counter_; }
    std::uint32_t mode() const noexcept;
    std::uint32_t target() const noexcept { return target_; }
    bool advance(std::uint32_t cpuCycles) noexcept;
    bool advanceDotClockPulses(std::uint32_t pulses) noexcept;
    void setHBlankActive(bool active) noexcept;
    bool read32(std::uint32_t address,std::uint32_t& value) noexcept;
    bool write32(std::uint32_t address,std::uint32_t value) noexcept;
private:
    static constexpr std::uint32_t kSyncEnable=1u, kResetAtTarget=1u<<3, kIrqAtTarget=1u<<4,
        kIrqAtOverflow=1u<<5, kIrqRepeat=1u<<6, kIrqToggle=1u<<7, kClockSourceMask=3u<<8;
    bool advanceTicks(std::uint32_t ticks) noexcept;
    bool clockAllowed() const noexcept;
    std::uint16_t counter_=0,target_=0;
    std::uint32_t mode_=0;
    bool reachedTarget_=false,reachedOverflow_=false,irqArmed_=true,irqRequest_=true;
    bool hBlankActive_=false,syncStarted_=false;
};
} // namespace nexus::ps1
