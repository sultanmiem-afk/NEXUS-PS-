#include "Gpu.hpp"

#include <cassert>
#include <cstdint>

using namespace nexus::ps1;

int main() {
    Vram vram;
    Gpu gpu(vram);
    std::uint16_t pixel = 0;

    gpu.writeGp0(0x020000FFu);
    gpu.writeGp0(10u | (20u << 16));
    gpu.writeGp0(8u | (2u << 16));
    assert(vram.readPixel(0, 20, pixel) && pixel == 0x001Fu);
    assert(vram.readPixel(15, 21, pixel) && pixel == 0x001Fu);
    assert(vram.readPixel(16, 20, pixel) && pixel == 0);

    gpu.writeGp0(0xE3000000u);
    gpu.writeGp0(0xE4000000u | 1023u | (511u << 10));

    gpu.writeGp0(0x200000FFu);
    gpu.writeGp0(100u | (100u << 16));
    gpu.writeGp0(120u | (100u << 16));
    gpu.writeGp0(100u | (120u << 16));
    assert(vram.readPixel(105, 105, pixel) && pixel == 0x001Fu);
    assert(vram.readPixel(119, 119, pixel) && pixel == 0);

    gpu.writeGp0(0x2800FF00u);
    gpu.writeGp0(200u | (200u << 16));
    gpu.writeGp0(220u | (200u << 16));
    gpu.writeGp0(200u | (220u << 16));
    gpu.writeGp0(220u | (220u << 16));
    assert(vram.readPixel(205, 205, pixel) && pixel == 0x03E0u);

    // Flat polygons retain 24-bit color until rasterization so GP0(E1)
    // dithering is applied before RGB555 quantization.
    gpu.writeGp0(0xE1000000u);
    gpu.writeGp0(0x20808080u);
    gpu.writeGp0(250u | (250u << 16));
    gpu.writeGp0(270u | (250u << 16));
    gpu.writeGp0(250u | (270u << 16));
    assert(vram.readPixel(255u, 255u, pixel) && pixel == 0x4210u);

    gpu.writeGp0(0xE1000200u);
    gpu.writeGp0(0x20808080u);
    gpu.writeGp0(250u | (250u << 16));
    gpu.writeGp0(270u | (250u << 16));
    gpu.writeGp0(250u | (270u << 16));
    assert(vram.readPixel(255u, 255u, pixel) && pixel == 0x3DEFu);

    // Gouraud-shaded triangle: red, green, and blue vertex colors interpolate.
    gpu.writeGp0(0x300000FFu);
    gpu.writeGp0(100u | (100u << 16));
    gpu.writeGp0(0x0000FF00u);
    gpu.writeGp0(120u | (100u << 16));
    gpu.writeGp0(0x00FF0000u);
    gpu.writeGp0(100u | (120u << 16));
    assert(vram.readPixel(105, 105, pixel));
    assert(pixel != 0x001Fu && pixel != 0x03E0u && pixel != 0x7C00u);
    assert((pixel & 0x7FFFu) != 0);

    // GP0(E1) bit 9 enables the PS1 4x4 dithering matrix for Gouraud polygons.
    gpu.writeGp0(0xE1000000u); // dithering disabled
    gpu.writeGp0(0x30808080u); // vertex 0 color is in the command word
    gpu.writeGp0(100u | (100u << 16));
    gpu.writeGp0(0x00808080u);
    gpu.writeGp0(120u | (100u << 16));
    gpu.writeGp0(0x00808080u);
    gpu.writeGp0(100u | (120u << 16));
    assert(vram.readPixel(105u, 105u, pixel) && pixel == 0x4210u);

    gpu.writeGp0(0xE1000200u); // enable dithering (E1 bit 9)
    gpu.writeGp0(0x30808080u);
    gpu.writeGp0(100u | (100u << 16));
    gpu.writeGp0(0x00808080u);
    gpu.writeGp0(120u | (100u << 16));
    gpu.writeGp0(0x00808080u);
    gpu.writeGp0(100u | (120u << 16));
    assert(vram.readPixel(105u, 105u, pixel) && pixel == 0x3DEFu);

    // Gouraud interpolation keeps exact vertex colors and uses weighted
    // channel interpolation at interior samples.
    gpu.writeGp0(0xE1000000u); // Disable dithering for exact RGB555 checks.
    gpu.writeGp0(0x300000FFu);
    gpu.writeGp0(400u | (400u << 16));
    gpu.writeGp0(0x0000FF00u);
    gpu.writeGp0(420u | (400u << 16));
    gpu.writeGp0(0x00FF0000u);
    gpu.writeGp0(400u | (420u << 16));
    assert(vram.readPixel(400u, 400u, pixel) && pixel == 0x001Fu);
    assert(vram.readPixel(420u, 400u, pixel) && pixel == 0x03E0u);
    assert(vram.readPixel(400u, 420u, pixel) && pixel == 0x7C00u);
    assert(vram.readPixel(405u, 405u, pixel) && pixel == 0x1CEFu);

    // Reversed winding must preserve the same interpolated color.
    gpu.writeGp0(0x300000FFu);
    gpu.writeGp0(400u | (400u << 16));
    gpu.writeGp0(0x00FF0000u);
    gpu.writeGp0(400u | (420u << 16));
    gpu.writeGp0(0x0000FF00u);
    gpu.writeGp0(420u | (400u << 16));
    assert(vram.readPixel(405u, 405u, pixel) && pixel == 0x1CEFu);

    // 15-bit textured triangle samples texels from a configured texture page.
    vram.writePixel(0, 0, 0x7C00u);
    gpu.writeGp0(0xE1000200u); // 15-bit texture page mode.
    gpu.writeGp0(0x26000000u); // raw texture: isolate page sampling.
    gpu.writeGp0(400u | (100u << 16));
    gpu.writeGp0(0u);
    gpu.writeGp0(420u | (100u << 16));
    gpu.writeGp0(0x01000000u); // UV and 15-bit direct-color page.
    gpu.writeGp0(400u | (120u << 16));
    gpu.writeGp0(0u);
    assert(vram.readPixel(405, 105, pixel) && pixel == 0x7C00u);


    // 4-bit indexed texture: nibble index resolves through a 16-color CLUT.
    vram.writePixel(0, 0, 0x0003u); // pixel index 3 in low nibble.
    vram.writePixel(3, 1, 0x03E0u);  // CLUT entry 3.
    gpu.writeGp0(0x24808080u); // neutral modulation for indexed texture.
    gpu.writeGp0(500u | (100u << 16));
    gpu.writeGp0(0u | (64u << 16)); // CLUT X=0, Y=1.
    gpu.writeGp0(520u | (100u << 16));
    gpu.writeGp0(0x00000000u); // UV + tpage: 4-bit page at X=0.
    gpu.writeGp0(500u | (120u << 16));
    gpu.writeGp0(0u);
    assert(vram.readPixel(505, 105, pixel) && pixel == 0x03E0u);

    // 8-bit indexed texture: byte index resolves through its CLUT.
    vram.writePixel(0, 0, 0x002Au); // low byte index 42.
    vram.writePixel(42, 2, 0x7C00u);  // CLUT entry 42.
    gpu.writeGp0(0x24808080u); // neutral modulation for indexed texture.
    gpu.writeGp0(550u | (100u << 16));
    gpu.writeGp0(0u | (128u << 16)); // CLUT X=0, Y=2.
    gpu.writeGp0(570u | (100u << 16));
    gpu.writeGp0(0x00800000u); // tpage depth bits = 1 (8-bit), page X=0.
    gpu.writeGp0(550u | (120u << 16));
    gpu.writeGp0(0u);
    assert(vram.readPixel(555, 105, pixel) && pixel == 0x7C00u);


    // Modulated textured triangle: neutral 128 RGB preserves texture color.
    vram.writePixel(0, 0, 0x03E0u);
    gpu.writeGp0(0x24808080u);
    gpu.writeGp0(600u | (100u << 16));
    gpu.writeGp0(0u);
    gpu.writeGp0(620u | (100u << 16));
    gpu.writeGp0(0x01000000u);
    gpu.writeGp0(600u | (120u << 16));
    gpu.writeGp0(0u);
    assert(vram.readPixel(605, 105, pixel) && pixel == 0x03E0u);

    // Raw-texture opcode bypasses vertex-color modulation.
    vram.writePixel(0, 0, 0x7C00u);
    gpu.writeGp0(0x26FFFFFFu); // raw texture, opaque.
    gpu.writeGp0(630u | (100u << 16));
    gpu.writeGp0(0u);
    gpu.writeGp0(650u | (100u << 16));
    gpu.writeGp0(0x01000000u);
    gpu.writeGp0(630u | (120u << 16));
    gpu.writeGp0(0u);
    assert(vram.readPixel(635, 105, pixel) && pixel == 0x7C00u);

    // GP0 untextured rectangle family: variable, 1x1, 8x8 and 16x16.
    gpu.writeGp0(0x600000FFu);
    gpu.writeGp0(700u | (100u << 16));
    gpu.writeGp0(5u | (3u << 16));
    assert(vram.readPixel(704, 102, pixel) && pixel == 0x001Fu);
    assert(vram.readPixel(705, 102, pixel) && pixel == 0u);

    gpu.writeGp0(0x6800FF00u);
    gpu.writeGp0(710u | (100u << 16));
    assert(vram.readPixel(710, 100, pixel) && pixel == 0x03E0u);
    assert(vram.readPixel(711, 100, pixel) && pixel == 0u);

    gpu.writeGp0(0x700000FFu);
    gpu.writeGp0(720u | (100u << 16));
    assert(vram.readPixel(727, 107, pixel) && pixel == 0x001Fu);
    assert(vram.readPixel(728, 107, pixel) && pixel == 0u);

    gpu.writeGp0(0x7800FF00u);
    gpu.writeGp0(740u | (100u << 16));
    assert(vram.readPixel(755, 115, pixel) && pixel == 0x03E0u);
    assert(vram.readPixel(756, 115, pixel) && pixel == 0u);

    // Untextured semi-transparency mode 0 averages source and destination.
    vram.writePixel(800u, 100u, 0x001Fu);
    gpu.writeGp0(0xE1000000u);
    gpu.writeGp0(0x6100FF00u);
    gpu.writeGp0(800u | (100u << 16));
    gpu.writeGp0(1u | (1u << 16));
    assert(vram.readPixel(800u, 100u, pixel) && pixel == 0x01EFu);

    // Textured 1x1 sprite samples a direct-color texel from the active page.
    vram.writePixel(2u, 3u, 0x7C00u);
    gpu.writeGp0(0xE1000100u); // 15-bit page, X=0.
    gpu.writeGp0(0x6E000000u); // raw, opaque, fixed 1x1 textured rectangle.
    gpu.writeGp0(820u | (100u << 16));
    gpu.writeGp0(2u | (3u << 8));
    assert(vram.readPixel(820u, 100u, pixel) && pixel == 0x7C00u);

    // Variable-size textured rectangle increments U/V across the sprite.
    vram.writePixel(4u, 5u, 0x03E0u);
    vram.writePixel(5u, 5u, 0x001Fu);
    gpu.writeGp0(0x66000000u); // raw textured rectangle, variable dimensions.
    gpu.writeGp0(830u | (100u << 16));
    gpu.writeGp0(4u | (5u << 8));
    gpu.writeGp0(2u | (1u << 16));
    assert(vram.readPixel(830u, 100u, pixel) && pixel == 0x03E0u);
    assert(vram.readPixel(831u, 100u, pixel) && pixel == 0x001Fu);

    gpu.writeGp0(0x400000FFu);
    gpu.writeGp0(300u | (300u << 16));
    gpu.writeGp0(310u | (305u << 16));
    assert(vram.readPixel(305, 303, pixel) && pixel == 0x001Fu);

    // Flat semi-transparent line 0x42; poison XY data must stay inside the packet.
    gpu.writeGp0(0xE1000000u); // Blend mode 0: average.
    gpu.writeGp0(0xE5000000u | 330u | (300u << 11)); // Draw offset.
    vram.writePixel(330u, 300u, 0x001Fu);
    gpu.writeGp0(0x4200FF00u);
    gpu.writeGp0(0xA0000000u); // XY=(0,0), not GP0(A0).
    gpu.writeGp0(4u);
    assert(vram.readPixel(332u, 300u, pixel) && pixel == 0x3DE0u);
    gpu.writeGp0(0xE5000000u); // Restore zero draw offset.

    // Gouraud line interpolation; A0000000 is XY packet data, not a transfer command.
    gpu.writeGp0(0xE5000000u | 400u | (300u << 11));
    gpu.writeGp0(0x50000000u);
    gpu.writeGp0(0xA0000000u); // XY0=(0,0) after draw offset.
    gpu.writeGp0(0x000000FFu); // Red endpoint color.
    gpu.writeGp0(4u); // XY1=(4,0) after draw offset.
    assert(vram.readPixel(404u, 300u, pixel) && pixel == 0x001Fu);
    assert(vram.readPixel(402u, 300u, pixel) && pixel != 0u && pixel != 0x001Fu);
    gpu.writeGp0(0xE5000000u);
    gpu.writeGp0(0x020000FFu);
    gpu.writeGp0(500u | (300u << 16));
    gpu.writeGp0(1u | (1u << 16));
    assert(vram.readPixel(500u, 300u, pixel) && pixel == 0x001Fu);

    // Gouraud semi-transparent line 0x52.
    gpu.writeGp0(0xE5000000u | 510u | (300u << 11));
    vram.writePixel(510u, 300u, 0x001Fu);
    gpu.writeGp0(0x5200FF00u);
    gpu.writeGp0(0xA0000000u);
    gpu.writeGp0(0x0000FF00u);
    gpu.writeGp0(4u);
    assert(vram.readPixel(512u, 300u, pixel) && pixel == 0x3DE0u);
    gpu.writeGp0(0xE5000000u);

    gpu.writeGp0(0xA0000000u);
    gpu.writeGp0(30u | (40u << 16));
    gpu.writeGp0(2u | (2u << 16));
    gpu.writeGp0(0x22221111u);
    gpu.writeGp0(0x44443333u);
    assert(vram.readPixel(30, 40, pixel) && pixel == 0x1111u);
    assert(vram.readPixel(31, 40, pixel) && pixel == 0x2222u);
    assert(vram.readPixel(30, 41, pixel) && pixel == 0x3333u);
    assert(vram.readPixel(31, 41, pixel) && pixel == 0x4444u);

    gpu.writeGp0(0xC0000000u);
    gpu.writeGp0(30u | (40u << 16));
    gpu.writeGp0(2u | (2u << 16));
    assert(gpu.readGp0() == 0x22221111u);
    assert(gpu.readGp0() == 0x44443333u);

    gpu.writeGp1(0x03000000u);
    assert((gpu.readStatus() & (1u << 23)) == 0);
    gpu.writeGp1(0x03000001u);
    assert((gpu.readStatus() & (1u << 23)) != 0);

    // HBlank follows the programmed horizontal display interval rather than
    // being emitted only at the scanline wrap.
    gpu.reset();
    const auto beforeVisible = gpu.advanceTiming(387u); // 608 GPU clocks
    assert(beforeVisible.hBlankPulses == 0u);
    assert(!beforeVisible.hBlankActive);
    const auto afterVisible = gpu.advanceTiming(1629u); // reaches 3168 clocks
    assert(afterVisible.hBlankPulses == 1u);
    assert(afterVisible.hBlankActive);

    // One complete modeled scanline still emits one HBlank pulse and 341
    // dot-clock ticks in the default 256-pixel mode.
    gpu.reset();
    const auto partialTiming = gpu.advanceTiming(2171u);
    assert(partialTiming.hBlankPulses == 1u);
    const auto lineTiming = gpu.advanceTiming(1u);
    assert(lineTiming.hBlankPulses == 0u);
    assert(partialTiming.dotClockTicks + lineTiming.dotClockTicks == 341u);

    gpu.writeGp1(0x00000000u);
    assert((gpu.readStatus() & (1u << 23)) != 0);

    // GP0 FIFO service uses a fixed 10-CPU-cycle word budget and preserves
    // fractional cycle credit across small CPU steps.
    gpu.reset();
    const auto fifoTimingA = gpu.advanceTiming(9u);
    assert(fifoTimingA.gp0ServiceWords == 0u);
    const auto fifoTimingB = gpu.advanceTiming(1u);
    assert(fifoTimingB.gp0ServiceWords == 1u);
    const auto fifoTimingC = gpu.advanceTiming(9u);
    assert(fifoTimingC.gp0ServiceWords == 0u);
    const auto fifoTimingD = gpu.advanceTiming(1u);
    assert(fifoTimingD.gp0ServiceWords == 1u);
    const auto fifoTimingBatch = gpu.advanceTiming(25u);
    assert(fifoTimingBatch.gp0ServiceWords == 2u);

    // GPUSTAT readiness and DMA request bits follow the selected direction.
    gpu.reset();
    std::uint32_t status = gpu.readStatus();
    assert((status & (1u << 26)) != 0); // ready for a command
    assert((status & (1u << 27)) == 0); // no VRAM read data
    assert((status & (1u << 28)) != 0); // ready for a DMA block
    assert((status & (1u << 25)) == 0); // DMA disabled

    gpu.writeGp1(0x04000001u); // FIFO mode
    status = gpu.readStatus();
    assert((status & (1u << 25)) != 0);
    assert(((status >> 29) & 3u) == 1u);

    gpu.writeGp1(0x04000002u); // CPU -> GP0
    status = gpu.readStatus();
    assert((status & (1u << 25)) != 0);
    assert((status & (1u << 28)) != 0);
    assert(((status >> 29) & 3u) == 2u);

    // An incomplete polygon packet can still accept GP0 words while FIFO has space,
    // but it is not ready for a new DMA block.
    gpu.writeGp0(0x200000FFu);
    status = gpu.readStatus();
    assert((status & (1u << 26)) != 0);
    assert((status & (1u << 28)) == 0);
    assert((status & (1u << 25)) == 0);
    gpu.writeGp0(10u | (10u << 16));
    gpu.writeGp0(20u | (10u << 16));
    gpu.writeGp0(10u | (20u << 16));
    status = gpu.readStatus();
    assert((status & (1u << 26)) != 0);
    assert((status & (1u << 28)) != 0);
    assert((status & (1u << 25)) != 0);

    // FIFO-full backpressure clears GP0 write readiness and recovers on service.
    gpu.reset();
    for (std::size_t i = 0; i < GpuFifo::kCapacity; ++i)
        assert(gpu.enqueueGp0(0x00000000u));
    status = gpu.readStatus();
    assert((status & (1u << 26)) == 0);
    assert(gpu.serviceGp0Fifo(1u) == 1u);
    status = gpu.readStatus();
    assert((status & (1u << 26)) != 0);
    gpu.reset();

    // CPU-to-VRAM transfer blocks command/DMA readiness until its data arrives.
    gpu.writeGp0(0xA0000000u);
    gpu.writeGp0(40u | (40u << 16));
    gpu.writeGp0(2u | (1u << 16));
    status = gpu.readStatus();
    assert((status & (1u << 26)) == 0);
    assert((status & (1u << 28)) == 0);
    assert((status & (1u << 25)) == 0);
    gpu.writeGp0(0x22221111u);
    status = gpu.readStatus();
    assert((status & (1u << 26)) != 0);
    assert((status & (1u << 28)) != 0);

    // VRAM-to-CPU transfer asserts the read-ready and direction-3 request bits.
    gpu.writeGp1(0x04000003u);
    gpu.writeGp0(0xC0000000u);
    gpu.writeGp0(40u | (40u << 16));
    gpu.writeGp0(2u | (1u << 16));
    status = gpu.readStatus();
    assert((status & (1u << 27)) != 0);
    assert((status & (1u << 25)) != 0);
    assert((status & (1u << 26)) == 0);
    assert((status & (1u << 28)) == 0);
    assert(((status >> 29) & 3u) == 3u);
    assert(gpu.readGp0() == 0x22221111u);
    status = gpu.readStatus();
    assert((status & (1u << 27)) == 0);
    assert((status & (1u << 25)) == 0);
    assert((status & (1u << 26)) != 0);
    assert((status & (1u << 28)) != 0);

    // Staged GP0 FIFO integration: commands remain pending until serviced.
    gpu.reset();
    assert(gpu.enqueueGp0(0x020000FFu));
    assert(gpu.enqueueGp0(900u | (400u << 16)));
    assert(gpu.enqueueGp0(1u | (1u << 16)));
    assert(gpu.gp0FifoSize() == 3u);
    assert(vram.readPixel(896u, 400u, pixel) && pixel == 0u);
    assert(gpu.serviceGp0Fifo(2u) == 2u);
    assert(gpu.gp0FifoSize() == 1u);
    assert(vram.readPixel(896u, 400u, pixel) && pixel == 0u);
    assert(gpu.serviceGp0Fifo(1u) == 1u);
    assert(gpu.gp0FifoSize() == 0u);
    assert(vram.readPixel(896u, 400u, pixel) && pixel == 0x001Fu);
    assert(gpu.serviceGp0Fifo(4u) == 0u);

    // GPU FIFO pressure test: queue complete command packets, hit capacity,
    // drain in uneven batches, wrap the ring, and verify execution order.
    gpu.reset();
    for (std::uint32_t round = 0; round < 100u; ++round) {
        const std::uint32_t y = 200u + round;
        const std::uint32_t colors[7] = {
            0x000000FFu, 0x0000FF00u, 0x00FF0000u,
            0x0000FFFFu, 0x00FFFF00u, 0x00FF00FFu, 0x00FFFFFFu
        };
        for (std::uint32_t i = 0; i < 5u; ++i) {
            assert(gpu.enqueueGp0(0x02000000u | colors[i]));
            assert(gpu.enqueueGp0((i * 16u) | (y << 16)));
            assert(gpu.enqueueGp0(16u | (1u << 16)));
        }
        assert(gpu.enqueueGp0(0u)); // NOP fills the sixteenth FIFO slot.
        assert(gpu.gp0FifoSize() == GpuFifo::kCapacity);
        assert(!gpu.enqueueGp0(0x020000FFu));
        assert((gpu.readStatus() & (1u << 26)) == 0u);

        assert(gpu.serviceGp0Fifo(7u) == 7u);
        for (std::uint32_t i = 5u; i < 7u; ++i) {
            assert(gpu.enqueueGp0(0x02000000u | colors[i]));
            assert(gpu.enqueueGp0((i * 16u) | (y << 16)));
            assert(gpu.enqueueGp0(16u | (1u << 16)));
        }
        assert(gpu.gp0FifoSize() == 15u);
        assert((gpu.readStatus() & (1u << 26)) != 0u);
        assert(gpu.serviceGp0Fifo(5u) == 5u);
        assert(gpu.serviceGp0Fifo(32u) == 10u);
        assert(gpu.gp0FifoSize() == 0u);

        for (std::uint32_t i = 0; i < 7u; ++i) {
            assert(vram.readPixel(i * 16u, y, pixel));
            assert(pixel == (i == 0u ? 0x001Fu :
                             i == 1u ? 0x03E0u :
                             i == 2u ? 0x7C00u :
                             i == 3u ? 0x03FFu :
                             i == 4u ? 0x7FE0u :
                             i == 5u ? 0x7C1Fu : 0x7FFFu));
        }
    }

    // Bounded GP0 FIFO primitive: capacity, overflow, ordering and wraparound.
    GpuFifo fifo;
    assert(fifo.empty());
    assert(!fifo.full());
    assert(fifo.capacity() == 16u);
    std::uint32_t fifoWord = 0xFFFFFFFFu;
    assert(!fifo.pop(fifoWord));
    for (std::uint32_t i = 0; i < fifo.capacity(); ++i) {
        assert(fifo.push(0xA5000000u | i));
    }
    assert(fifo.full());
    assert(fifo.size() == fifo.capacity());
    assert(!fifo.push(0xDEADBEEFu)); // Full FIFO rejects new data.
    for (std::uint32_t i = 0; i < 8u; ++i) {
        assert(fifo.pop(fifoWord));
        assert(fifoWord == (0xA5000000u | i));
    }
    for (std::uint32_t i = 16u; i < 24u; ++i) {
        assert(fifo.push(0xA5000000u | i)); // Reuses wrapped slots.
    }
    for (std::uint32_t i = 8u; i < 24u; ++i) {
        assert(fifo.pop(fifoWord));
        assert(fifoWord == (0xA5000000u | i));
    }
    assert(fifo.empty());
    assert(!fifo.pop(fifoWord));
    assert(fifo.push(0x12345678u));
    fifo.clear();
    assert(fifo.empty());
    assert(fifo.size() == 0u);

    // Deterministic FIFO stress test: repeated mixed push/pop operations
    // verify ordering while the ring buffer wraps many times.
    GpuFifo stressFifo;
    std::uint32_t expectedNext = 0u;
    std::uint32_t consumed = 0u;
    for (std::uint32_t round = 0; round < 2000u; ++round) {
        for (std::uint32_t i = 0; i < 12u; ++i) {
            assert(stressFifo.push(0x5A000000u | expectedNext));
            ++expectedNext;
        }
        for (std::uint32_t i = 0; i < 12u; ++i) {
            assert(stressFifo.pop(fifoWord));
            assert(fifoWord == (0x5A000000u | consumed));
            ++consumed;
        }
    }
    while (!stressFifo.empty()) {
        assert(stressFifo.pop(fifoWord));
        assert(fifoWord == (0x5A000000u | consumed));
        ++consumed;
    }
    assert(consumed == expectedNext);

    // Unsupported GP0 packets must consume their exact payload. Poison words
    // deliberately resemble valid commands (notably A0000000 / image transfer).
    const std::uint8_t unsupportedOpcodes[] = {
        0x21u, 0x22u, 0x23u, 0x2Au, 0x32u, 0x3Au,
        0x34u, 0x35u, 0x36u, 0x37u, 0x3Cu, 0x3Du, 0x3Eu, 0x3Fu,
        0x80u
    };
    const std::size_t unsupportedLengths[] = {
        4u, 4u, 4u, 5u, 6u, 8u,
        9u, 9u, 9u, 9u, 12u, 12u, 12u, 12u,
        4u
    };
    for (std::size_t command = 0; command < 15u; ++command) {
        gpu.reset();
        gpu.writeGp0(static_cast<std::uint32_t>(unsupportedOpcodes[command]) << 24u);
        for (std::size_t word = 1u; word < unsupportedLengths[command]; ++word)
            gpu.writeGp0(0xA0000000u);
        const std::uint32_t x = 700u + static_cast<std::uint32_t>(command) * 8u;
        gpu.writeGp0(0x020000FFu);
        gpu.writeGp0(x | (450u << 16u));
        gpu.writeGp0(1u | (1u << 16u));
        assert(vram.readPixel(x, 450u, pixel) && pixel == 0x001Fu);
    }

    // Flat and Gouraud polylines are variable-length. The 5xxx5xxx marker
    // includes the PsyQ 0x55555555 terminator, not only 0x50005000.
    gpu.reset();
    gpu.writeGp0(0x480000FFu); // Flat polyline.
    gpu.writeGp0(0xA0000000u); // Poisoned vertex word must remain polyline data.
    gpu.writeGp0(20u | (20u << 16u));
    gpu.writeGp0(0x55555555u);
    gpu.writeGp0(0x020000FFu);
    gpu.writeGp0(950u | (450u << 16u));
    gpu.writeGp0(1u | (1u << 16u));
    assert(vram.readPixel(950u, 450u, pixel) && pixel == 0x001Fu);

    gpu.reset();
    gpu.writeGp0(0x580000FFu); // Gouraud polyline: bit 4 selects color/XY pairs.
    gpu.writeGp0(10u | (10u << 16u));
    gpu.writeGp0(0xA0000000u); // Poisoned vertex color.
    gpu.writeGp0(20u | (20u << 16u));
    gpu.writeGp0(0x00FF0000u);
    gpu.writeGp0(0x55555555u); // Terminator at an XY position.
    gpu.writeGp0(0x020000FFu);
    gpu.writeGp0(960u | (450u << 16u));
    gpu.writeGp0(1u | (1u << 16u));
    assert(vram.readPixel(960u, 450u, pixel) && pixel == 0x001Fu);

    gpu.reset();
    gpu.writeGp0(0x5A0000FFu); // Gouraud polyline mirror opcode.
    gpu.writeGp0(10u | (10u << 16u));
    gpu.writeGp0(0xA0000000u);
    gpu.writeGp0(20u | (20u << 16u));
    gpu.writeGp0(0x00FF0000u);
    gpu.writeGp0(0x55555555u);
    gpu.writeGp0(0x020000FFu);
    gpu.writeGp0(970u | (450u << 16u));
    gpu.writeGp0(1u | (1u << 16u));
    assert(vram.readPixel(970u, 450u, pixel) && pixel == 0x001Fu);

    return 0;
}
