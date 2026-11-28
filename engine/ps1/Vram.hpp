#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace nexus::ps1 {

// PS1 VRAM: 1024 x 512 pixels, 16 bits per pixel (1 MiB).
// It is accessed by the GPU command/transfer path, not as ordinary CPU RAM.
class Vram {
public:
    static constexpr std::uint32_t kWidth = 1024;
    static constexpr std::uint32_t kHeight = 512;
    static constexpr std::size_t kPixelCount =
        static_cast<std::size_t>(kWidth) * kHeight;
    static constexpr std::size_t kSizeBytes = kPixelCount * sizeof(std::uint16_t);

    Vram() noexcept = default;

    constexpr std::uint32_t width() const noexcept { return kWidth; }
    constexpr std::uint32_t height() const noexcept { return kHeight; }
    constexpr std::size_t sizeBytes() const noexcept { return kSizeBytes; }

    // Contiguous row-major pixel access for hot GPU rasterization/scanout loops.
    std::uint16_t* data() noexcept { return pixels_.data(); }
    const std::uint16_t* data() const noexcept { return pixels_.data(); }

    void clear(std::uint16_t color = 0) noexcept;
    bool readPixel(std::uint32_t x, std::uint32_t y,
                   std::uint16_t& color) const noexcept;
    bool writePixel(std::uint32_t x, std::uint32_t y,
                    std::uint16_t color) noexcept;
    bool copyRectFrom(const std::uint16_t* source, std::size_t sourcePixels,
                      std::uint32_t x, std::uint32_t y,
                      std::uint32_t width, std::uint32_t height) noexcept;
    bool copyRectTo(std::uint16_t* destination, std::size_t destinationPixels,
                    std::uint32_t x, std::uint32_t y,
                    std::uint32_t width, std::uint32_t height) const noexcept;

private:
    std::array<std::uint16_t, kPixelCount> pixels_{};
    static bool validRect(std::uint32_t x, std::uint32_t y,
                          std::uint32_t width, std::uint32_t height) noexcept;
};

} // namespace nexus::ps1
