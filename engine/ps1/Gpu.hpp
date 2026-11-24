#pragma once

#include "Vram.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace nexus::ps1 {

// Early GP0/GP1 command processor. This is a functional subset, not a
// cycle-accurate GPU or a complete polygon rasterizer.
class Gpu {
public:
    explicit Gpu(Vram& vram) noexcept;

    void reset() noexcept;
    void writeGp0(std::uint32_t value);
    std::uint32_t readGp0();
    void writeGp1(std::uint32_t value) noexcept;
    std::uint32_t readStatus() const noexcept;

    bool irqPending() const noexcept { return irqPending_; }
    std::size_t unsupportedCommandCount() const noexcept {
        return unsupportedCommandCount_;
    }

private:
    enum class TransferMode { None, CpuToVram, VramToCpu };

    Vram& vram_;
    std::vector<std::uint32_t> packet_;
    std::size_t expectedWords_ = 0;
    TransferMode transferMode_ = TransferMode::None;
    std::uint32_t transferX_ = 0;
    std::uint32_t transferY_ = 0;
    std::uint32_t transferWidth_ = 0;
    std::uint32_t transferHeight_ = 0;
    std::size_t transferIndex_ = 0;
    std::uint32_t readLatch_ = 0;
    bool readLatchValid_ = false;
    bool irqPending_ = false;
    bool displayDisabled_ = true;
    std::uint32_t dmaDirection_ = 0;
    std::uint32_t displayStart_ = 0;
    std::uint32_t horizontalRange_ = 0xC60260u;
    std::uint32_t verticalRange_ = 0x3FC10u;
    std::uint32_t displayMode_ = 0;
    std::uint32_t drawMode_ = 0;
    std::uint32_t textureWindow_ = 0;
    std::uint32_t drawAreaTopLeft_ = 0;
    std::uint32_t drawAreaBottomRight_ = 0;
    std::uint32_t drawOffset_ = 0;
    std::uint32_t maskSetting_ = 0;
    std::size_t unsupportedCommandCount_ = 0;

    static std::uint32_t color24To15(std::uint32_t rgb) noexcept;
    static std::uint32_t decodeTransferDimension(std::uint16_t value,
                                                  std::uint32_t maximum) noexcept;
    void acceptPacketWord(std::uint32_t value);
    void executePacket();
    void beginImageTransfer(TransferMode mode);
    void writeTransferPixel(std::uint16_t pixel) noexcept;
    bool readTransferPixel(std::uint16_t& pixel) noexcept;
    void finishTransfer() noexcept;
    void executeFillRectangle() noexcept;
    void executeFlatPolygon() noexcept;
    void drawTriangle(int x0, int y0, int x1, int y1, int x2, int y2,
                      std::uint16_t color) noexcept;
    void executeEnvironmentCommand(std::uint32_t command) noexcept;
};

} // namespace nexus::ps1
