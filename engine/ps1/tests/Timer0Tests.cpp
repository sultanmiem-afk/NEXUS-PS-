#include "Timer0.hpp"

#include <cassert>
#include <cstdint>

using namespace nexus::ps1;

int main() {
    Timer0 timer;
    std::uint32_t value = 0;

    assert(timer.read32(Timer0::kCounterAddress, value) && value == 0u);
    assert(timer.write32(Timer0::kTargetAddress, 3u));
    // Target IRQ + reset-at-target + repeat, CPU clock.
    assert(timer.write32(Timer0::kModeAddress, (1u << 3) | (1u << 4) | (1u << 6)));
    assert(!timer.advance(2u));
    assert(timer.advance(1u));
    assert(timer.counter() == 0u);
    assert((timer.mode() & (1u << 11)) != 0u);
    assert(timer.advance(3u));

    // Writing MODE resets counter and sticky event flags.
    assert(timer.write32(Timer0::kModeAddress, 0u));
    assert(timer.counter() == 0u);
    assert((timer.mode() & ((1u << 11) | (1u << 12))) == 0u);
    assert(!timer.advance(5u));
    assert(timer.counter() == 5u);

    // Overflow event and one-shot IRQ.
    assert(timer.write32(Timer0::kCounterAddress, 0xFFFFu));
    assert(timer.write32(Timer0::kModeAddress, 1u << 5));
    assert(timer.write32(Timer0::kCounterAddress, 0xFFFFu));
    assert(timer.advance(1u));
    assert((timer.mode() & (1u << 12)) != 0u);
    assert(!timer.advance(1u));

    // Sync and non-CPU clock sources are not advanced by CPU cycles yet.
    assert(timer.write32(Timer0::kModeAddress, 1u));
    assert(!timer.advance(10u));
    assert(timer.counter() == 0u);
    assert(timer.write32(Timer0::kModeAddress, 1u << 8));
    assert(!timer.advance(10u));
    assert(timer.counter() == 0u);

    // Dot-clock source advances only from GPU-provided pulses.
    assert(timer.write32(Timer0::kTargetAddress, 2u));
    assert(timer.write32(Timer0::kModeAddress, (1u << 8) | (1u << 4)));
    assert(!timer.advance(100u));
    assert(timer.counter() == 0u);
    assert(!timer.advanceDotClockPulses(1u));
    assert(timer.advanceDotClockPulses(1u));
    assert(timer.counter() == 2u);

    assert(!timer.read32(0x1F80110Cu, value));

    // Toggle IRQ output: only the active (high-to-low) transition interrupts.
    assert(timer.write32(Timer0::kTargetAddress, 1u));
    assert(timer.write32(Timer0::kModeAddress, (1u << 3) | (1u << 4) | (1u << 6) | (1u << 7)));
    assert(timer.advance(1u));
    assert((timer.mode() & (1u << 10)) == 0u);
    assert(!timer.advance(1u));
    assert((timer.mode() & (1u << 10)) != 0u);
    assert(timer.advance(1u));
    assert((timer.mode() & (1u << 10)) == 0u);

    // Target zero is reached when the 16-bit counter wraps.
    assert(timer.write32(Timer0::kTargetAddress, 0u));
    assert(timer.write32(Timer0::kModeAddress, (1u << 4) | (1u << 3)));
    assert(timer.write32(Timer0::kCounterAddress, 0xFFFFu));
    assert(timer.advance(1u));
    assert(timer.counter() == 0u);
    assert((timer.mode() & (1u << 11)) != 0u);

    return 0;
}
