#pragma once
#include <cstdint>
namespace nexus::ps1 {
class Timer1 final {
public:
    static constexpr std::uint32_t kCounterAddress=0x1F801110u, kModeAddress=0x1F801114u, kTargetAddress=0x1F801118u;
    void reset() noexcept;
    std::uint32_t counter() const noexcept{return counter_;}
    std::uint32_t mode() const noexcept;
    std::uint32_t target() const noexcept{return target_;}
    bool advance(std::uint32_t cpuCycles) noexcept;
    bool advanceHBlankPulses(std::uint32_t pulses) noexcept;
    void setVBlankActive(bool active) noexcept;
    bool read32(std::uint32_t address,std::uint32_t& value) noexcept;
    bool write32(std::uint32_t address,std::uint32_t value) noexcept;
private:
    static constexpr std::uint32_t kSyncEnable=1u,kResetAtTarget=1u<<3,kIrqAtTarget=1u<<4,
      kIrqAtOverflow=1u<<5,kIrqRepeat=1u<<6,kIrqToggle=1u<<7,kClockSourceMask=3u<<8;
    bool advanceTicks(std::uint32_t ticks) noexcept;
    bool clockAllowed() const noexcept;
    std::uint16_t counter_=0,target_=0; std::uint32_t mode_=0;
    bool reachedTarget_=false,reachedOverflow_=false,irqArmed_=true,irqRequest_=true;
    std::uint8_t irqPulseTicks_=0;
    bool vBlankActive_=false,syncStarted_=false;
};
} // namespace nexus::ps1
