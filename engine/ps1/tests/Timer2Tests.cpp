#include "Timer2.hpp"

#include <cassert>
#include <cstdint>

using namespace nexus::ps1;

int main() {
    Timer2 timer;
    std::uint32_t value = 0;

    assert(timer.read32(Timer2::kCounterAddress, value) && value == 0u);
    assert(timer.write32(Timer2::kTargetAddress, 3u));
    assert(timer.write32(Timer2::kModeAddress, (1u << 3) | (1u << 4) | (1u << 6)));
    assert(!timer.advance(2u));
    assert(timer.advance(1u));
    assert(timer.counter() == 0u);
    assert((timer.mode() & (1u << 11)) != 0u);
    assert(timer.advance(3u));

    // Timer 2 /8 source preserves divider phase across separate advances.
    assert(timer.write32(Timer2::kTargetAddress, 1u));
    assert(timer.write32(Timer2::kModeAddress, (1u << 8) | (1u << 4)));
    assert(!timer.advance(7u));
    assert(timer.counter() == 0u);
    assert(timer.advance(1u));
    assert(timer.counter() == 1u);

    // Counter overflow and one-shot IRQ.
    assert(timer.write32(Timer2::kModeAddress, 1u << 5));
    assert(timer.write32(Timer2::kCounterAddress, 0xFFFFu));
    assert(timer.advance(1u));
    assert((timer.mode() & (1u << 12)) != 0u);
    assert(!timer.advance(1u));

    // Mode write resets counter, divider phase, flags, and IRQ arm.
    assert(timer.write32(Timer2::kModeAddress, 1u));
    assert(!timer.advance(16u));
    assert(timer.counter() == 0u);
    assert((timer.mode() & ((1u << 11) | (1u << 12))) == 0u);

    assert(!timer.read32(0x1F80112Cu, value));
    assert(!timer.write32(0x1F80112Cu, 0u));

    // Toggle IRQ output: only the active (high-to-low) transition interrupts.
    assert(timer.write32(Timer2::kTargetAddress, 1u));
    assert(timer.write32(Timer2::kModeAddress, (1u << 3) | (1u << 4) | (1u << 6) | (1u << 7)));
    assert(timer.advance(1u));
    assert((timer.mode() & (1u << 10)) == 0u);
    assert(!timer.advance(1u));
    assert((timer.mode() & (1u << 10)) != 0u);
    assert(timer.advance(1u));
    assert((timer.mode() & (1u << 10)) == 0u);

    // Target zero is reached when the 16-bit counter wraps.
    assert(timer.write32(Timer2::kTargetAddress, 0u));
    assert(timer.write32(Timer2::kModeAddress, (1u << 4) | (1u << 3)));
    assert(timer.write32(Timer2::kCounterAddress, 0xFFFFu));
    assert(timer.advance(1u));
    assert(timer.counter() == 0u);
    assert((timer.mode() & (1u << 11)) != 0u);

    return 0;
}
