#include "BiosRom.hpp"
#include "Vram.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <vector>

using namespace nexus::ps1;

int main() {
    BiosRom bios;
    assert(bios.size() == 512u * 1024u);
    assert(!bios.loaded());
    std::uint32_t word = 0;
    assert(!bios.read32(0x1FC00000u, word));

    std::vector<std::uint8_t> image(BiosRom::kSize, 0);
    image[0] = 0x78; image[1] = 0x56; image[2] = 0x34; image[3] = 0x12;
    assert(!bios.load(image.data(), image.size() - 1));
    assert(bios.load(image));
    assert(bios.read32(0x1FC00000u, word) && word == 0x12345678u);
    assert(bios.read32(0xBFC00000u, word) && word == 0x12345678u);
    assert(!bios.read32(0x1FC00001u, word));
    std::uint8_t byte = 0;
    // The BIOS API intentionally exposes no write methods (read-only ROM).
    bios.unload();
    assert(!bios.loaded() && !bios.read8(0x1FC00000u, byte));

    Vram vram;
    assert(vram.sizeBytes() == 1024u * 512u * 2u);
    assert(vram.width() == 1024 && vram.height() == 512);
    std::uint16_t pixel = 0;
    assert(vram.readPixel(0, 0, pixel) && pixel == 0);
    assert(vram.writePixel(1023, 511, 0x7FFFu));
    assert(vram.readPixel(1023, 511, pixel) && pixel == 0x7FFFu);
    assert(!vram.writePixel(1024, 0, 1));

    const std::array<std::uint16_t, 4> source{1, 2, 3, 4};
    assert(vram.copyRectFrom(source.data(), source.size(), 10, 20, 2, 2));
    std::array<std::uint16_t, 4> destination{};
    assert(vram.copyRectTo(destination.data(), destination.size(), 10, 20, 2, 2));
    assert(destination == source);
    assert(!vram.copyRectFrom(source.data(), 3, 0, 0, 2, 2));
    assert(!vram.copyRectTo(destination.data(), destination.size(), 1023, 511, 2, 2));

    vram.clear(0x1234u);
    assert(vram.readPixel(0, 0, pixel) && pixel == 0x1234u);
    return 0;
}
