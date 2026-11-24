#include "Timer1.hpp"

namespace nexus::ps1 {

void Timer1::reset() noexcept {
    counter_ = 0;
    target_ = 0;
    mode_ = 0;
    reachedTarget_ = false;
    reachedOverflow_ = false;
    irqArmed_ = true;
    irqRequest_ = true;
}

std::uint32_t Timer1::mode() const noexcept {
    return (mode_ & 0x3FFu) |
           (irqRequest_ ? (1u << 10) : 0u) |
           (reachedTarget_ ? (1u << 11) : 0u) |
           (reachedOverflow_ ? (1u << 12) : 0u);
}

bool Timer1::advance(std::uint32_t cpuCycles) noexcept {
    if ((mode_ & kSyncEnable) != 0u) return false;
    const auto clockSource = (mode_ & kClockSourceMask) >> 8;
    // Timer 1 system-clock input is modeled for source 0. HBlank is supplied
    // through advanceHBlankPulses(); other sources are not modeled yet.
    if (clockSource != 0u) return false;

    bool irq = false;
    for (std::uint32_t i = 0; i < cpuCycles; ++i) {
        const auto next = static_cast<std::uint32_t>(counter_) + 1u;
        const bool overflow = next > 0xFFFFu;
        const bool targetHit = next == target_ || (overflow && target_ == 0u);
        if (targetHit) reachedTarget_ = true;
        if (overflow) reachedOverflow_ = true;

        const bool event = (targetHit && (mode_ & kIrqAtTarget) != 0u) ||
                           (overflow && (mode_ & kIrqAtOverflow) != 0u);
        if (event && ((mode_ & kIrqRepeat) != 0u || irqArmed_)) {
            if ((mode_ & kIrqToggle) != 0u) {
                irqRequest_ = !irqRequest_;
                irq = !irqRequest_; // Interrupt on the active (high-to-low) transition.
            } else {
                irqRequest_ = false;
                irq = true;
            }
            if ((mode_ & kIrqRepeat) == 0u) irqArmed_ = false;
        }

        if (targetHit && (mode_ & kResetAtTarget) != 0u)
            counter_ = 0;
        else
            counter_ = static_cast<std::uint16_t>(next);
    }
    return irq;
}

bool Timer1::advanceHBlankPulses(std::uint32_t pulses) noexcept {
    if (((mode_ & kClockSourceMask) >> 8) != 1u || (mode_ & kSyncEnable) != 0u)
        return false;
    const std::uint32_t savedMode = mode_;
    mode_ &= ~kClockSourceMask;
    const bool irq = advance(pulses);
    mode_ = savedMode;
    return irq;
}

bool Timer1::read32(std::uint32_t address, std::uint32_t& value) const noexcept {
    switch (address) {
    case kCounterAddress: value = counter_; return true;
    case kModeAddress: value = mode(); return true;
    case kTargetAddress: value = target_; return true;
    default: return false;
    }
}

bool Timer1::write32(std::uint32_t address, std::uint32_t value) noexcept {
    switch (address) {
    case kCounterAddress:
        counter_ = static_cast<std::uint16_t>(value);
        return true;
    case kModeAddress:
        mode_ = value & 0x3FFu;
        counter_ = 0;
        reachedTarget_ = false;
        reachedOverflow_ = false;
        irqArmed_ = true;
        irqRequest_ = true;
        return true;
    case kTargetAddress:
        target_ = static_cast<std::uint16_t>(value);
        return true;
    default:
        return false;
    }
}

} // namespace nexus::ps1
