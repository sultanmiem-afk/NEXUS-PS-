#include "Vram.hpp"

#include <algorithm>

namespace nexus::ps1 {

void Vram::clear(std::uint16_t color) noexcept {
    pixels_.fill(color);
}

bool Vram::validRect(std::uint32_t x, std::uint32_t y,
                     std::uint32_t width, std::uint32_t height) noexcept {
    return width != 0 && height != 0 && x < kWidth && y < kHeight &&
           width <= kWidth - x && height <= kHeight - y;
}

bool Vram::readPixel(std::uint32_t x, std::uint32_t y,
                     std::uint16_t& color) const noexcept {
    if (x >= kWidth || y >= kHeight) return false;
    color = pixels_[static_cast<std::size_t>(y) * kWidth + x];
    return true;
}

bool Vram::writePixel(std::uint32_t x, std::uint32_t y,
                      std::uint16_t color) noexcept {
    if (x >= kWidth || y >= kHeight) return false;
    pixels_[static_cast<std::size_t>(y) * kWidth + x] = color;
    return true;
}

bool Vram::copyRectFrom(const std::uint16_t* source, std::size_t sourcePixels,
                        std::uint32_t x, std::uint32_t y,
                        std::uint32_t width, std::uint32_t height) noexcept {
    if (source == nullptr || !validRect(x, y, width, height) ||
        sourcePixels < static_cast<std::size_t>(width) * height) return false;
    for (std::uint32_t row = 0; row < height; ++row) {
        const auto dst = pixels_.begin() +
            static_cast<std::size_t>(y + row) * kWidth + x;
        std::copy_n(source + static_cast<std::size_t>(row) * width, width, dst);
    }
    return true;
}

bool Vram::copyRectTo(std::uint16_t* destination, std::size_t destinationPixels,
                      std::uint32_t x, std::uint32_t y,
                      std::uint32_t width, std::uint32_t height) const noexcept {
    if (destination == nullptr || !validRect(x, y, width, height) ||
        destinationPixels < static_cast<std::size_t>(width) * height) return false;
    for (std::uint32_t row = 0; row < height; ++row) {
        const auto src = pixels_.begin() +
            static_cast<std::size_t>(y + row) * kWidth + x;
        std::copy_n(src, width, destination + static_cast<std::size_t>(row) * width);
    }
    return true;
}

} // namespace nexus::ps1
