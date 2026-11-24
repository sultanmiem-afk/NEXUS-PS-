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


    // 15-bit textured triangle samples texels from a configured texture page.
    vram.writePixel(64, 0, 0x7C00u);
    gpu.writeGp0(0xE1000200u); // 15-bit texture page mode.
    gpu.writeGp0(0x24000000u);
    gpu.writeGp0(400u | (100u << 16));
    gpu.writeGp0(0u);
    gpu.writeGp0(420u | (100u << 16));
    gpu.writeGp0(0x02000000u); // UV and page (page X=64, 15-bit).
    gpu.writeGp0(400u | (120u << 16));
    gpu.writeGp0(0u);
    assert(vram.readPixel(405, 105, pixel) && pixel == 0x7C00u);

    gpu.writeGp0(0x400000FFu);
    gpu.writeGp0(300u | (300u << 16));
    gpu.writeGp0(310u | (305u << 16));
    assert(vram.readPixel(305, 303, pixel) && pixel == 0x001Fu);

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
    gpu.writeGp1(0x00000000u);
    assert((gpu.readStatus() & (1u << 23)) != 0);
    return 0;
}
