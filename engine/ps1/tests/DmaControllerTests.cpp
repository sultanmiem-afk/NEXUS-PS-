#include "DmaController.hpp"

#include <cassert>
#include <cstdint>

using namespace nexus::ps1;

int main() {
    DmaController dma;
    std::uint32_t value = 0;

    assert(dma.read32(DmaController::kDpcrAddress, value));
    assert(value == 0x07654321u);
    assert(dma.read32(DmaController::kDicrAddress, value));
    assert(value == 0u);

    // Every channel exposes MADR, BCR and CHCR at 0x10-byte intervals.
    for (std::uint32_t channel = 0; channel < 7; ++channel) {
        const std::uint32_t base = DmaController::kBaseAddress + channel * 0x10u;
        assert(dma.write32(base, 0x00ABCDEFu + channel));
        assert(dma.read32(base, value) && value == (0x00ABCDEFu + channel));
        assert(dma.write32(base + 4u, 0x00010020u + channel));
        assert(dma.read32(base + 4u, value) && value == (0x00010020u + channel));
        assert(dma.write32(base + 8u, 0x11000001u));
        assert(dma.read32(base + 8u, value) && value == (0x11000001u & 0x71770703u));
    }

    assert(dma.write32(DmaController::kDpcrAddress, 0x0FFFFFFFu));
    assert(dma.read32(DmaController::kDpcrAddress, value) && value == 0x0FFFFFFFu);

    // DICR master IRQ is derived from force or enabled channel flags.
    assert(dma.write32(DmaController::kDicrAddress, 1u << 15));
    assert(dma.read32(DmaController::kDicrAddress, value));
    assert((value & 0x80000000u) != 0);
    assert(dma.write32(DmaController::kDicrAddress, 0u));
    assert(dma.read32(DmaController::kDicrAddress, value));
    assert((value & 0x80000000u) == 0);

    assert(!dma.read32(DmaController::kBaseAddress + 0x78u, value));
    assert(!dma.read32(DmaController::kBaseAddress + 1u, value));
    assert(!dma.write32(DmaController::kBaseAddress + 0xCu, 0u));

    dma.reset();
    assert(dma.read32(DmaController::kDpcrAddress, value) && value == 0x07654321u);
    assert(dma.read32(DmaController::kBaseAddress, value) && value == 0u);
    return 0;
}
