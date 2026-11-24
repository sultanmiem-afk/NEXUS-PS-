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
    vram.writePixel(0, 0, 0x7C00u);
    gpu.writeGp0(0xE1000200u); // 15-bit texture page mode.
    gpu.writeGp0(0x24000000u);
    gpu.writeGp0(400u | (100u << 16));
    gpu.writeGp0(0u);
    gpu.writeGp0(420u | (100u << 16));
    gpu.writeGp0(0x01000000u); // UV and 15-bit direct-color page.
    gpu.writeGp0(400u | (120u << 16));
    gpu.writeGp0(0u);
    assert(vram.readPixel(405, 105, pixel) && pixel == 0x7C00u);


    // 4-bit indexed texture: nibble index resolves through a 16-color CLUT.
    vram.writePixel(128, 10, 0x0003u); // pixel index 3 in low nibble.
    vram.writePixel(48, 20, 0x03E0u);  // CLUT entry 3.
    gpu.writeGp0(0x24000000u);
    gpu.writeGp0(500u | (100u << 16));
    gpu.writeGp0(0u | (1u << 16)); // CLUT X=0, Y=1.
    gpu.writeGp0(520u | (100u << 16));
    gpu.writeGp0(0x00000000u); // UV + tpage: 4-bit page at X=128.
    gpu.writeGp0(500u | (120u << 16));
    gpu.writeGp0(0u);
    assert(vram.readPixel(505, 105, pixel) && pixel == 0x03E0u);

    // 8-bit indexed texture: byte index resolves through its CLUT.
    vram.writePixel(192, 30, 0x002Au); // low byte index 42.
    vram.writePixel(42, 40, 0x7C00u);  // CLUT entry 42.
    gpu.writeGp0(0x24000000u);
    gpu.writeGp0(550u | (100u << 16));
    gpu.writeGp0(0u | (2u << 16)); // CLUT Y=2.
    gpu.writeGp0(570u | (100u << 16));
    gpu.writeGp0(0x00000080u); // tpage depth bits = 1 (8-bit), page X=192.
    gpu.writeGp0(550u | (120u << 16));
    gpu.writeGp0(0u);
    assert(vram.readPixel(555, 105, pixel) && pixel == 0x7C00u);

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
