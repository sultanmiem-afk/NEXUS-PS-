#include "DmaController.hpp"
#include "Gpu.hpp"
#include "Ram.hpp"
#include "Vram.hpp"

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

    // Channel 2: manual RAM-to-GPU transfer writes two packed VRAM pixels.
    Ram ram;
    Vram vram;
    Gpu gpu(vram);
    assert(dma.write32(DmaController::kDpcrAddress, 1u << 11)); // Enable channel 2.
    assert(ram.write32(0x100u, 0x22221111u));
    gpu.writeGp0(0xA0000000u); // CPU-to-VRAM image transfer.
    gpu.writeGp0(0x00000000u); // X=0, Y=0.
    gpu.writeGp0(0x00010002u); // Width=2, Height=1.
    assert(dma.write32(DmaController::kBaseAddress + 0x20u, 0x100u));
    assert(dma.write32(DmaController::kBaseAddress + 0x24u, 1u));
    assert(dma.write32(DmaController::kBaseAddress + 0x28u, 0x11000001u));
    assert(dma.executeGpuTransfer(ram, gpu));
    std::uint16_t pixel = 0;
    assert(vram.readPixel(0, 0, pixel) && pixel == 0x1111u);
    assert(vram.readPixel(1, 0, pixel) && pixel == 0x2222u);
    assert(dma.lastTransferWords() == 1u);
    assert((dma.channelControl(2) & (1u << 24)) == 0u);

    // Channel 2 reverse direction copies VRAM pixels back into main RAM.
    gpu.writeGp0(0xC0000000u); // VRAM-to-CPU image transfer.
    gpu.writeGp0(0x00000000u);
    gpu.writeGp0(0x00010002u);
    assert(dma.write32(DmaController::kBaseAddress + 0x20u, 0x200u));
    assert(dma.write32(DmaController::kBaseAddress + 0x24u, 1u));
    assert(dma.write32(DmaController::kBaseAddress + 0x28u, 0x11000000u));
    assert(dma.executeGpuTransfer(ram, gpu));
    assert(ram.read32(0x200u, value) && value == 0x22221111u);
    assert(dma.lastTransferWords() == 1u);

    dma.reset();
    assert(dma.read32(DmaController::kDpcrAddress, value) && value == 0x07654321u);
    assert(dma.read32(DmaController::kBaseAddress, value) && value == 0u);
    return 0;
}
