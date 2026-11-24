#include "Timer1.hpp"

#include <cassert>
#include <cstdint>

using namespace nexus::ps1;

int main() {
    Timer1 timer;
    std::uint32_t value = 0;

    assert(timer.read32(Timer1::kCounterAddress, value) && value == 0u);
    assert(timer.write32(Timer1::kTargetAddress, 2u));
    assert(timer.write32(Timer1::kModeAddress, (1u << 3) | (1u << 4) | (1u << 6)));
    assert(!timer.advance(1u));
    assert(timer.advance(1u));
    assert((timer.mode() & (1u << 11)) != 0u);
    assert(timer.advance(2u));

    // Timer 1 source 1 is driven by externally supplied HBlank pulses.
    assert(timer.write32(Timer1::kModeAddress, (1u << 8) | (1u << 4)));
    assert(!timer.advance(10u));
    assert(timer.counter() == 0u);
    assert(!timer.advanceHBlankPulses(1u));
    assert(timer.advanceHBlankPulses(1u));
    assert(timer.counter() == 2u);

    // Sync mode currently pauses the simplified model.
    assert(timer.write32(Timer1::kModeAddress, 1u));
    assert(!timer.advance(10u));
    assert(!timer.advanceHBlankPulses(10u));
    assert(timer.counter() == 0u);

    assert(!timer.read32(0x1F80111Cu, value));

    // Toggle IRQ output: only the active (high-to-low) transition interrupts.
    assert(timer.write32(Timer1::kTargetAddress, 1u));
    assert(timer.write32(Timer1::kModeAddress, (1u << 4) | (1u << 6) | (1u << 7)));
    assert(timer.advance(1u));
    assert((timer.mode() & (1u << 10)) == 0u);
    assert(!timer.advance(1u));
    assert((timer.mode() & (1u << 10)) != 0u);
    assert(timer.advance(1u));
    assert((timer.mode() & (1u << 10)) == 0u);

    // Target zero is reached when the 16-bit counter wraps.
    assert(timer.write32(Timer1::kTargetAddress, 0u));
    assert(timer.write32(Timer1::kModeAddress, (1u << 4) | (1u << 3)));
    assert(timer.write32(Timer1::kCounterAddress, 0xFFFFu));
    assert(timer.advance(1u));
    assert(timer.counter() == 0u);
    assert((timer.mode() & (1u << 11)) != 0u);

    return 0;
}
