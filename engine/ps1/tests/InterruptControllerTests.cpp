#include "InterruptController.hpp"

#include <cassert>
#include <cstdint>

using namespace nexus::ps1;

int main() {
    InterruptController irq;
    std::uint32_t value = 0;

    assert(irq.status() == 0u);
    assert(irq.mask() == 0u);
    assert(!irq.active());

    // Request VBlank and DMA. Pending bits latch even while masked.
    irq.request((1u << 0) | (1u << 3));
    assert(irq.read32(InterruptController::kStatusAddress, value));
    assert(value == ((1u << 0) | (1u << 3)));
    assert(!irq.active());

    // Only enabled pending sources assert the CPU interrupt line.
    assert(irq.write32(InterruptController::kMaskAddress, 1u << 3));
    assert(irq.active());
    assert(irq.read32(InterruptController::kMaskAddress, value));
    assert(value == (1u << 3));

    // I_STAT is write-zero-to-clear. A one preserves the selected bit.
    assert(irq.write32(InterruptController::kStatusAddress, ~(1u << 0)));
    assert(irq.status() == (1u << 3));
    assert(irq.active());
    assert(irq.write32(InterruptController::kStatusAddress, ~(1u << 3)));
    assert(irq.status() == 0u);
    assert(!irq.active());

    // Ignore undefined source bits and unknown addresses.
    irq.request(0xFFFFFFFFu);
    assert(irq.status() == InterruptController::kValidSources);
    assert(!irq.read32(0x1F801078u, value));
    assert(!irq.write32(0x1F801078u, 0u));

    irq.reset();
    assert(irq.status() == 0u);
    assert(irq.mask() == 0u);
    return 0;
}
