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
    assert(dma.write32(DmaController::kDicrAddress, (1u << 23) | (1u << 18)));
    assert(dma.executeGpuTransfer(ram, gpu));
    assert((dma.dicr() & (1u << 26)) != 0u); // Channel 2 completion flag.
    assert((dma.dicr() & 0x80000000u) != 0u); // Master IRQ asserted.
    assert(dma.irqPending());
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

    // Linked-list mode streams a GP0 packet from a RAM DMA list.
    assert(ram.write32(0x300u, 0x03FFFFFFu)); // Three words, end-of-list.
    assert(ram.write32(0x304u, 0x020000FFu)); // GP0 fill rectangle, red.
    assert(ram.write32(0x308u, 0x00000000u)); // X=0, Y=0.
    assert(ram.write32(0x30Cu, 0x00010001u)); // 1x1 (hardware aligns fill width).
    assert(dma.write32(DmaController::kBaseAddress + 0x20u, 0x300u));
    assert(dma.write32(DmaController::kBaseAddress + 0x28u, 0x01000401u));
    assert(dma.executeGpuTransfer(ram, gpu));
    assert(vram.readPixel(0, 0, pixel) && pixel == 31u);
    assert(dma.lastTransferWords() == 3u);
    assert(dma.base(2) == 0x00FFFFFFu);


    // Simple multi-word data round-trip: RAM -> GPU VRAM -> RAM.
    const std::uint32_t testWords[] = {
        0x12345678u, 0x9ABCDEF0u, 0x0F1E2D3Cu, 0xA5B6C7D8u
    };
    for (std::uint32_t i = 0; i < 4u; ++i) {
        assert(ram.write32(0x400u + i * 4u, testWords[i]));
    }

    // Prepare an 8-pixel VRAM upload (four 32-bit words).
    gpu.writeGp0(0xA0000000u);
    gpu.writeGp0(0x00000000u);
    gpu.writeGp0(0x00010008u);
    assert(dma.write32(DmaController::kBaseAddress + 0x20u, 0x400u));
    assert(dma.write32(DmaController::kBaseAddress + 0x24u, 4u));
    assert(dma.write32(DmaController::kBaseAddress + 0x28u, 0x11000001u));
    assert(dma.executeGpuTransfer(ram, gpu));
    assert(dma.lastTransferWords() == 4u);

    // Read the same eight pixels back into a different RAM location.
    gpu.writeGp0(0xC0000000u);
    gpu.writeGp0(0x00000000u);
    gpu.writeGp0(0x00010008u);
    assert(dma.write32(DmaController::kBaseAddress + 0x20u, 0x500u));
    assert(dma.write32(DmaController::kBaseAddress + 0x24u, 4u));
    assert(dma.write32(DmaController::kBaseAddress + 0x28u, 0x11000000u));
    assert(dma.executeGpuTransfer(ram, gpu));
    assert(dma.lastTransferWords() == 4u);
    for (std::uint32_t i = 0; i < 4u; ++i) {
        assert(ram.read32(0x500u + i * 4u, value));
        assert(value == testWords[i]);
    }

    // Larger 256-word DMA round-trip to exercise sustained multi-block data movement.
    for (std::uint32_t i = 0; i < 256u; ++i) {
        const std::uint32_t pattern = 0x9E3779B9u * (i + 1u) ^ (0xA5A50000u + i);
        assert(ram.write32(0x1000u + i * 4u, pattern));
    }

    gpu.writeGp0(0xA0000000u);
    gpu.writeGp0(0x00010000u); // X=0, Y=1.
    gpu.writeGp0(0x00010200u); // 512 pixels x 1 row = 256 words.
    assert(dma.write32(DmaController::kBaseAddress + 0x20u, 0x1000u));
    assert(dma.write32(DmaController::kBaseAddress + 0x24u, 256u));
    assert(dma.write32(DmaController::kBaseAddress + 0x28u, 0x11000001u));
    assert(dma.executeGpuTransfer(ram, gpu));
    assert(dma.lastTransferWords() == 256u);

    gpu.writeGp0(0xC0000000u);
    gpu.writeGp0(0x00010000u);
    gpu.writeGp0(0x00010200u);
    assert(dma.write32(DmaController::kBaseAddress + 0x20u, 0x2000u));
    assert(dma.write32(DmaController::kBaseAddress + 0x24u, 256u));
    assert(dma.write32(DmaController::kBaseAddress + 0x28u, 0x11000000u));
    assert(dma.executeGpuTransfer(ram, gpu));
    assert(dma.lastTransferWords() == 256u);
    for (std::uint32_t i = 0; i < 256u; ++i) {
        std::uint32_t original = 0;
        assert(ram.read32(0x1000u + i * 4u, original));
        assert(ram.read32(0x2000u + i * 4u, value));
        assert(value == original);
    }

    // 1024-word DMA round-trip stress test across two full VRAM rows.
    for (std::uint32_t i = 0; i < 1024u; ++i) {
        const std::uint32_t pattern = (0x7F4A7C15u * (i + 17u)) ^ (0xC3D20000u | i);
        assert(ram.write32(0x3000u + i * 4u, pattern));
    }

    gpu.writeGp0(0xA0000000u);
    gpu.writeGp0(0x000A0000u); // X=0, Y=10.
    gpu.writeGp0(0x00020400u); // 1024 pixels x 2 rows = 1024 words.
    assert(dma.write32(DmaController::kBaseAddress + 0x20u, 0x3000u));
    assert(dma.write32(DmaController::kBaseAddress + 0x24u, 1024u));
    assert(dma.write32(DmaController::kBaseAddress + 0x28u, 0x11000001u));
    assert(dma.executeGpuTransfer(ram, gpu));
    assert(dma.lastTransferWords() == 1024u);

    gpu.writeGp0(0xC0000000u);
    gpu.writeGp0(0x000A0000u);
    gpu.writeGp0(0x00020400u);
    assert(dma.write32(DmaController::kBaseAddress + 0x20u, 0x5000u));
    assert(dma.write32(DmaController::kBaseAddress + 0x24u, 1024u));
    assert(dma.write32(DmaController::kBaseAddress + 0x28u, 0x11000000u));
    assert(dma.executeGpuTransfer(ram, gpu));
    assert(dma.lastTransferWords() == 1024u);
    for (std::uint32_t i = 0; i < 1024u; ++i) {
        std::uint32_t original = 0;
        assert(ram.read32(0x3000u + i * 4u, original));
        assert(ram.read32(0x5000u + i * 4u, value));
        assert(value == original);
    }

    // 4096-word DMA round-trip stress test across eight full VRAM rows.
    for (std::uint32_t i = 0; i < 4096u; ++i) {
        const std::uint32_t pattern = (0x6D2B79F5u * (i + 31u)) ^ (0x5A3C0000u | (i * 13u));
        assert(ram.write32(0x8000u + i * 4u, pattern));
    }

    gpu.writeGp0(0xA0000000u);
    gpu.writeGp0(0x00140000u); // X=0, Y=20.
    gpu.writeGp0(0x00080400u); // 1024 pixels x 8 rows = 4096 words.
    assert(dma.write32(DmaController::kBaseAddress + 0x20u, 0x8000u));
    assert(dma.write32(DmaController::kBaseAddress + 0x24u, 4096u));
    assert(dma.write32(DmaController::kBaseAddress + 0x28u, 0x11000001u));
    assert(dma.executeGpuTransfer(ram, gpu));
    assert(dma.lastTransferWords() == 4096u);

    gpu.writeGp0(0xC0000000u);
    gpu.writeGp0(0x00140000u);
    gpu.writeGp0(0x00080400u);
    assert(dma.write32(DmaController::kBaseAddress + 0x20u, 0xC000u));
    assert(dma.write32(DmaController::kBaseAddress + 0x24u, 4096u));
    assert(dma.write32(DmaController::kBaseAddress + 0x28u, 0x11000000u));
    assert(dma.executeGpuTransfer(ram, gpu));
    assert(dma.lastTransferWords() == 4096u);
    for (std::uint32_t i = 0; i < 4096u; ++i) {
        std::uint32_t original = 0;
        assert(ram.read32(0x8000u + i * 4u, original));
        assert(ram.read32(0xC000u + i * 4u, value));
        assert(value == original);
    }

    // 8192-word DMA round-trip stress test across sixteen full VRAM rows.
    for (std::uint32_t i = 0; i < 8192u; ++i) {
        const std::uint32_t pattern = (0x9E3779B9u * (i + 73u)) ^ (0xA53C0000u | (i * 29u));
        assert(ram.write32(0x18000u + i * 4u, pattern));
    }

    gpu.writeGp0(0xA0000000u);
    gpu.writeGp0(0x00240000u); // X=0, Y=36.
    gpu.writeGp0(0x00100400u); // 1024 pixels x 16 rows = 8192 words.
    assert(dma.write32(DmaController::kBaseAddress + 0x20u, 0x18000u));
    assert(dma.write32(DmaController::kBaseAddress + 0x24u, 8192u));
    assert(dma.write32(DmaController::kBaseAddress + 0x28u, 0x11000001u));
    assert(dma.executeGpuTransfer(ram, gpu));
    assert(dma.lastTransferWords() == 8192u);

    gpu.writeGp0(0xC0000000u);
    gpu.writeGp0(0x00240000u);
    gpu.writeGp0(0x00100400u);
    assert(dma.write32(DmaController::kBaseAddress + 0x20u, 0x20000u));
    assert(dma.write32(DmaController::kBaseAddress + 0x24u, 8192u));
    assert(dma.write32(DmaController::kBaseAddress + 0x28u, 0x11000000u));
    assert(dma.executeGpuTransfer(ram, gpu));
    assert(dma.lastTransferWords() == 8192u);
    for (std::uint32_t i = 0; i < 8192u; ++i) {
        std::uint32_t original = 0;
        assert(ram.read32(0x18000u + i * 4u, original));
        assert(ram.read32(0x20000u + i * 4u, value));
        assert(value == original);
    }

    // 16384-word DMA round-trip stress test across thirty-two full VRAM rows.
    for (std::uint32_t i = 0; i < 16384u; ++i) {
        const std::uint32_t pattern = (0x85EBCA6Bu * (i + 101u)) ^ (0xD17A0000u | (i * 37u));
        assert(ram.write32(0x40000u + i * 4u, pattern));
    }

    gpu.writeGp0(0xA0000000u);
    gpu.writeGp0(0x00380000u); // X=0, Y=56.
    gpu.writeGp0(0x00200400u); // 1024 pixels x 32 rows = 16384 words.
    assert(dma.write32(DmaController::kBaseAddress + 0x20u, 0x40000u));
    assert(dma.write32(DmaController::kBaseAddress + 0x24u, 16384u));
    assert(dma.write32(DmaController::kBaseAddress + 0x28u, 0x11000001u));
    assert(dma.executeGpuTransfer(ram, gpu));
    assert(dma.lastTransferWords() == 16384u);

    gpu.writeGp0(0xC0000000u);
    gpu.writeGp0(0x00380000u);
    gpu.writeGp0(0x00200400u);
    assert(dma.write32(DmaController::kBaseAddress + 0x20u, 0x50000u));
    assert(dma.write32(DmaController::kBaseAddress + 0x24u, 16384u));
    assert(dma.write32(DmaController::kBaseAddress + 0x28u, 0x11000000u));
    assert(dma.executeGpuTransfer(ram, gpu));
    assert(dma.lastTransferWords() == 16384u);
    for (std::uint32_t i = 0; i < 16384u; ++i) {
        std::uint32_t original = 0;
        assert(ram.read32(0x40000u + i * 4u, original));
        assert(ram.read32(0x50000u + i * 4u, value));
        assert(value == original);
    }

    // 32768-word DMA round-trip stress test across sixty-four full VRAM rows.
    for (std::uint32_t i = 0; i < 32768u; ++i) {
        const std::uint32_t pattern = (0xC2B2AE35u * (i + 149u)) ^ (0x6B8D0000u | (i * 43u));
        assert(ram.write32(0x80000u + i * 4u, pattern));
    }

    gpu.writeGp0(0xA0000000u);
    gpu.writeGp0(0x00640000u); // X=0, Y=100.
    gpu.writeGp0(0x00400400u); // 1024 pixels x 64 rows = 32768 words.
    assert(dma.write32(DmaController::kBaseAddress + 0x20u, 0x80000u));
    assert(dma.write32(DmaController::kBaseAddress + 0x24u, 32768u));
    assert(dma.write32(DmaController::kBaseAddress + 0x28u, 0x11000001u));
    assert(dma.executeGpuTransfer(ram, gpu));
    assert(dma.lastTransferWords() == 32768u);

    gpu.writeGp0(0xC0000000u);
    gpu.writeGp0(0x00640000u);
    gpu.writeGp0(0x00400400u);
    assert(dma.write32(DmaController::kBaseAddress + 0x20u, 0xA0000u));
    assert(dma.write32(DmaController::kBaseAddress + 0x24u, 32768u));
    assert(dma.write32(DmaController::kBaseAddress + 0x28u, 0x11000000u));
    assert(dma.executeGpuTransfer(ram, gpu));
    assert(dma.lastTransferWords() == 32768u);
    for (std::uint32_t i = 0; i < 32768u; ++i) {
        std::uint32_t original = 0;
        assert(ram.read32(0x80000u + i * 4u, original));
        assert(ram.read32(0xA0000u + i * 4u, value));
        assert(value == original);
    }

    // 65536-word DMA round-trip stress test across 128 full VRAM rows.
    for (std::uint32_t i = 0; i < 65536u; ++i) {
        const std::uint32_t pattern =
            (0x9E3779B9u * (i + 211u)) ^ (0xD31A0000u | (i * 67u));
        assert(ram.write32(0xC0000u + i * 4u, pattern));
    }

    gpu.writeGp0(0xA0000000u);
    gpu.writeGp0(0x00C80000u); // X=0, Y=200.
    gpu.writeGp0(0x00800400u); // 1024 pixels x 128 rows = 65536 words.
    assert(dma.write32(DmaController::kBaseAddress + 0x20u, 0xC0000u));
    assert(dma.write32(DmaController::kBaseAddress + 0x24u, 65536u));
    assert(dma.write32(DmaController::kBaseAddress + 0x28u, 0x11000001u));
    assert(dma.executeGpuTransfer(ram, gpu));
    assert(dma.lastTransferWords() == 65536u);

    gpu.writeGp0(0xC0000000u);
    gpu.writeGp0(0x00C80000u);
    gpu.writeGp0(0x00800400u);
    assert(dma.write32(DmaController::kBaseAddress + 0x20u, 0x100000u));
    assert(dma.write32(DmaController::kBaseAddress + 0x24u, 65536u));
    assert(dma.write32(DmaController::kBaseAddress + 0x28u, 0x11000000u));
    assert(dma.executeGpuTransfer(ram, gpu));
    assert(dma.lastTransferWords() == 65536u);
    for (std::uint32_t i = 0; i < 65536u; ++i) {
        std::uint32_t original = 0;
        assert(ram.read32(0xC0000u + i * 4u, original));
        assert(ram.read32(0x100000u + i * 4u, value));
        assert(value == original);
    }

    // 131072-word DMA round-trip stress test across the remaining 256 VRAM rows.
    for (std::uint32_t i = 0; i < 131072u; ++i) {
        const std::uint32_t pattern =
            (0x85EBCA6Bu * (i + 307u)) ^ (0xA71D0000u | (i * 97u));
        assert(ram.write32(0x00000u + i * 4u, pattern));
    }

    gpu.writeGp0(0xA0000000u);
    gpu.writeGp0(0x01000000u); // X=0, Y=256.
    gpu.writeGp0(0x01000400u); // 1024 pixels x 256 rows = 131072 words.
    assert(dma.write32(DmaController::kBaseAddress + 0x20u, 0x00000u));
    // Request mode: 4 blocks x 32768 words; manual mode has a 16-bit count.
    assert(dma.write32(DmaController::kBaseAddress + 0x24u, 0x00048000u));
    assert(dma.write32(DmaController::kBaseAddress + 0x28u, 0x11000201u));
    assert(dma.executeGpuTransfer(ram, gpu));
    assert(dma.lastTransferWords() == 131072u);

    gpu.writeGp0(0xC0000000u);
    gpu.writeGp0(0x01000000u);
    gpu.writeGp0(0x01000400u);
    assert(dma.write32(DmaController::kBaseAddress + 0x20u, 0x80000u));
    assert(dma.write32(DmaController::kBaseAddress + 0x24u, 0x00048000u));
    assert(dma.write32(DmaController::kBaseAddress + 0x28u, 0x11000200u));
    assert(dma.executeGpuTransfer(ram, gpu));
    assert(dma.lastTransferWords() == 131072u);
    for (std::uint32_t i = 0; i < 131072u; ++i) {
        std::uint32_t original = 0;
        assert(ram.read32(0x00000u + i * 4u, original));
        assert(ram.read32(0x80000u + i * 4u, value));
        assert(value == original);
    }

    dma.reset();
    assert(dma.read32(DmaController::kDpcrAddress, value) && value == 0x07654321u);
    assert(dma.read32(DmaController::kBaseAddress, value) && value == 0u);
    return 0;
}
