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
