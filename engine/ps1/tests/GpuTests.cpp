#include "Gpu.hpp"

#include <array>
#include <cassert>
#include <cstdint>

using namespace nexus::ps1;

int main() {
    Vram vram;
    Gpu gpu(vram);

    // GP0(02h): fill rectangle. The hardware command aligns X/width to 16.
    gpu.writeGp0(0x020000FFu); // red
    gpu.writeGp0(10u | (20u << 16));
    gpu.writeGp0(8u | (2u << 16));
    std::uint16_t pixel = 0;
    assert(vram.readPixel(0, 20, pixel) && pixel == 0x001Fu);
    assert(vram.readPixel(15, 21, pixel) && pixel == 0x001Fu);
    assert(vram.readPixel(16, 20, pixel) && pixel == 0);

    // Set a full-screen drawing area before polygon tests.\n    gpu.writeGp0(0xE3000000u);\n    gpu.writeGp0(0xE4000000u | 1023u | (511u << 10));\n\n    // GP0(20h): flat-shaded triangle.\n    gpu.writeGp0(0x200000FFu);\n    gpu.writeGp0(100u | (100u << 16));\n    gpu.writeGp0(120u | (100u << 16));\n    gpu.writeGp0(100u | (120u << 16));\n    assert(vram.readPixel(105, 105, pixel) && pixel == 0x001Fu);\n    assert(vram.readPixel(119, 119, pixel) && pixel == 0);\n\n    // GP0(28h): flat-shaded quad, split into two triangles.\n    gpu.writeGp0(0x2800FF00u);\n    gpu.writeGp0(200u | (200u << 16));\n    gpu.writeGp0(220u | (200u << 16));\n    gpu.writeGp0(200u | (220u << 16));\n    gpu.writeGp0(220u | (220u << 16));\n    assert(vram.readPixel(205, 205, pixel) && pixel == 0x03E0u);\n\n    // GP0(A0h): CPU-to-VRAM transfer of four pixels.
    gpu.writeGp0(0xA0000000u);
    gpu.writeGp0(30u | (40u << 16));
    gpu.writeGp0(2u | (2u << 16));
    gpu.writeGp0(0x22221111u);
    gpu.writeGp0(0x44443333u);
    assert(vram.readPixel(30, 40, pixel) && pixel == 0x1111u);
    assert(vram.readPixel(31, 40, pixel) && pixel == 0x2222u);
    assert(vram.readPixel(30, 41, pixel) && pixel == 0x3333u);
    assert(vram.readPixel(31, 41, pixel) && pixel == 0x4444u);

    // GP0(C0h): VRAM-to-CPU readback packs two 16-bit pixels per word.
    gpu.writeGp0(0xC0000000u);
    gpu.writeGp0(30u | (40u << 16));
    gpu.writeGp0(2u | (2u << 16));
    assert(gpu.readGp0() == 0x22221111u);
    assert(gpu.readGp0() == 0x44443333u);

    // GP1 display control and reset behavior.
    gpu.writeGp1(0x03000000u);
    assert((gpu.readStatus() & (1u << 23)) == 0);
    gpu.writeGp1(0x03000001u);
    assert((gpu.readStatus() & (1u << 23)) != 0);
    gpu.writeGp1(0x00000000u);
    assert((gpu.readStatus() & (1u << 23)) != 0);
    return 0;
}
