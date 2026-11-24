#include "Gpu.hpp"

#include <algorithm>

namespace nexus::ps1 {

Gpu::Gpu(Vram& vram) noexcept : vram_(vram) {
    reset();
}

void Gpu::reset() noexcept {
    packet_.clear();
    expectedWords_ = 0;
    transferMode_ = TransferMode::None;
    transferX_ = transferY_ = transferWidth_ = transferHeight_ = 0;
    transferIndex_ = 0;
    readLatch_ = 0;
    readLatchValid_ = false;
    irqPending_ = false;
    displayDisabled_ = true;
    dmaDirection_ = 0;
    displayStart_ = 0;
    horizontalRange_ = 0xC60260u;
    verticalRange_ = 0x3FC10u;
    displayMode_ = 0;
    drawMode_ = textureWindow_ = drawAreaTopLeft_ = 0;
    drawAreaBottomRight_ = drawOffset_ = maskSetting_ = 0;
    unsupportedCommandCount_ = 0;
}

std::uint32_t Gpu::color24To15(std::uint32_t rgb) noexcept {
    const std::uint32_t r = rgb & 0xFFu;
    const std::uint32_t g = (rgb >> 8) & 0xFFu;
    const std::uint32_t b = (rgb >> 16) & 0xFFu;
    return ((r >> 3) & 31u) | (((g >> 3) & 31u) << 5) |
           (((b >> 3) & 31u) << 10);
}

std::uint32_t Gpu::decodeTransferDimension(std::uint16_t value,
                                           std::uint32_t maximum) noexcept {
    const std::uint32_t decoded = value == 0 ? maximum : value;
    return std::min(decoded, maximum);
}

void Gpu::writeGp0(std::uint32_t value) {
    if (transferMode_ == TransferMode::CpuToVram) {
        const std::uint16_t first = static_cast<std::uint16_t>(value);
        writeTransferPixel(first);
        if (transferMode_ == TransferMode::CpuToVram) {
            const std::uint16_t second = static_cast<std::uint16_t>(value >> 16);
            writeTransferPixel(second);
        }
        return;
    }
    acceptPacketWord(value);
}

std::uint32_t Gpu::readGp0() {
    if (transferMode_ == TransferMode::VramToCpu) {
        if (!readLatchValid_) {
            std::uint16_t first = 0;
            if (!readTransferPixel(first)) return 0;
            std::uint16_t second = 0;
            if (readTransferPixel(second)) {
                readLatch_ = static_cast<std::uint32_t>(first) |
                             (static_cast<std::uint32_t>(second) << 16);
            } else {
                readLatch_ = first;
            }
            readLatchValid_ = true;
        }
        const std::uint32_t result = readLatch_;
        readLatchValid_ = false;
        return result;
    }
    return 0;
}

void Gpu::acceptPacketWord(std::uint32_t value) {
    if (packet_.empty()) {
        const std::uint8_t opcode = static_cast<std::uint8_t>(value >> 24);
        if (opcode == 0x00 || opcode == 0x01) return;
        if (opcode >= 0xE1 && opcode <= 0xE6) {
            executeEnvironmentCommand(value);
            return;
        }
        if (opcode == 0x02) expectedWords_ = 3;
        else if (opcode == 0x20) expectedWords_ = 4;
        else if (opcode == 0x28) expectedWords_ = 5;
        else if (opcode == 0xA0 || opcode == 0xC0) expectedWords_ = 3;
        else {
            ++unsupportedCommandCount_;
            return;
        }
    }

    packet_.push_back(value);
    if (packet_.size() == expectedWords_) {
        executePacket();
        packet_.clear();
        expectedWords_ = 0;
    }
}

void Gpu::executePacket() {
    const std::uint8_t opcode = static_cast<std::uint8_t>(packet_[0] >> 24);
    if (opcode == 0x02) {
        executeFillRectangle();
    } else if (opcode == 0x20 || opcode == 0x28) {
        executeFlatPolygon();
    } else if (opcode == 0xA0) {
        beginImageTransfer(TransferMode::CpuToVram);
    } else if (opcode == 0xC0) {
        beginImageTransfer(TransferMode::VramToCpu);
    }
}

void Gpu::executeFillRectangle() noexcept {
    const std::uint32_t color = color24To15(packet_[0]);
    const std::uint32_t rawX = packet_[1] & 0xFFFFu;
    const std::uint32_t rawY = (packet_[1] >> 16) & 0xFFFFu;
    const std::uint32_t width = packet_[2] & 0xFFFFu;
    const std::uint32_t height = (packet_[2] >> 16) & 0xFFFFu;
    if (width == 0 || height == 0) return;

    // GP0(02h) fill rectangles align the starting X down to 16 pixels and
    // round width up to a 16-pixel boundary. Coordinates wrap in VRAM.
    const std::uint32_t x = rawX & ~15u;
    const std::uint32_t alignedWidth = (width + 15u) & ~15u;
    for (std::uint32_t row = 0; row < height; ++row) {
        for (std::uint32_t col = 0; col < alignedWidth; ++col) {
            vram_.writePixel((x + col) % Vram::kWidth,
                             (rawY + row) % Vram::kHeight,
                             static_cast<std::uint16_t>(color));
        }
    }
}


namespace {
int signExtend11(std::uint32_t value) noexcept {
    value &= 0x7FFu;
    return (value & 0x400u) ? static_cast<int>(value) - 0x800
                            : static_cast<int>(value);
}

long long edgeFunction(int ax, int ay, int bx, int by,
                       int px, int py) noexcept {
    return static_cast<long long>(px - ax) * (by - ay) -
           static_cast<long long>(py - ay) * (bx - ax);
}
} // namespace

void Gpu::executeFlatPolygon() noexcept {
    if (packet_.size() < 4) return;
    const std::uint16_t color = static_cast<std::uint16_t>(color24To15(packet_[0]));
    const int offsetX = signExtend11(drawOffset_ & 0x7FFu);
    const int offsetY = signExtend11((drawOffset_ >> 11) & 0x7FFu);
    int x[4]{};
    int y[4]{};
    const int vertices = packet_[0] >> 24 == 0x28 ? 4 : 3;
    for (int i = 0; i < vertices; ++i) {
        const std::uint32_t packed = packet_[static_cast<std::size_t>(i) + 1];
        x[i] = signExtend11(packed) + offsetX;
        y[i] = signExtend11(packed >> 16) + offsetY;
    }
    drawTriangle(x[0], y[0], x[1], y[1], x[2], y[2], color);
    if (vertices == 4) {
        drawTriangle(x[1], y[1], x[2], y[2], x[3], y[3], color);
    }
}

void Gpu::drawTriangle(int x0, int y0, int x1, int y1,
                       int x2, int y2, std::uint16_t color) noexcept {
    const long long area = edgeFunction(x0, y0, x1, y1, x2, y2);
    if (area == 0) return;

    const int left = static_cast<int>(drawAreaTopLeft_ & 0x3FFu);
    const int top = static_cast<int>((drawAreaTopLeft_ >> 10) & 0x1FFu);
    const int right = static_cast<int>(drawAreaBottomRight_ & 0x3FFu);
    const int bottom = static_cast<int>((drawAreaBottomRight_ >> 10) & 0x1FFu);
    const int minX = std::max({0, left, std::min({x0, x1, x2})});
    const int minY = std::max({0, top, std::min({y0, y1, y2})});
    const int maxX = std::min({static_cast<int>(Vram::kWidth) - 1, right,
                               std::max({x0, x1, x2})});
    const int maxY = std::min({static_cast<int>(Vram::kHeight) - 1, bottom,
                               std::max({y0, y1, y2})});
    if (minX > maxX || minY > maxY) return;

    for (int py = minY; py <= maxY; ++py) {
        for (int px = minX; px <= maxX; ++px) {
            const long long e0 = edgeFunction(x0, y0, x1, y1, px, py);
            const long long e1 = edgeFunction(x1, y1, x2, y2, px, py);
            const long long e2 = edgeFunction(x2, y2, x0, y0, px, py);
            const bool inside = area > 0
                ? (e0 <= 0 && e1 <= 0 && e2 <= 0)
                : (e0 >= 0 && e1 >= 0 && e2 >= 0);
            if (inside) vram_.writePixel(static_cast<std::uint32_t>(px),
                                         static_cast<std::uint32_t>(py), color);
        }
    }
}

void Gpu::beginImageTransfer(TransferMode mode) {
    transferX_ = packet_[1] & 0x3FFu;
    transferY_ = (packet_[1] >> 16) & 0x1FFu;
    transferWidth_ = decodeTransferDimension(
        static_cast<std::uint16_t>(packet_[2]), Vram::kWidth);
    transferHeight_ = decodeTransferDimension(
        static_cast<std::uint16_t>(packet_[2] >> 16), Vram::kHeight);
    transferIndex_ = 0;
    readLatchValid_ = false;
    transferMode_ = (transferWidth_ == 0 || transferHeight_ == 0)
                        ? TransferMode::None : mode;
}

void Gpu::writeTransferPixel(std::uint16_t pixel) noexcept {
    if (transferMode_ != TransferMode::CpuToVram) return;
    const std::size_t count = static_cast<std::size_t>(transferWidth_) * transferHeight_;
    if (transferIndex_ >= count) {
        finishTransfer();
        return;
    }
    const std::uint32_t x = (transferX_ + static_cast<std::uint32_t>(transferIndex_ % transferWidth_)) % Vram::kWidth;
    const std::uint32_t y = (transferY_ + static_cast<std::uint32_t>(transferIndex_ / transferWidth_)) % Vram::kHeight;
    vram_.writePixel(x, y, pixel);
    ++transferIndex_;
    if (transferIndex_ >= count) finishTransfer();
}

bool Gpu::readTransferPixel(std::uint16_t& pixel) noexcept {
    if (transferMode_ != TransferMode::VramToCpu) return false;
    const std::size_t count = static_cast<std::size_t>(transferWidth_) * transferHeight_;
    if (transferIndex_ >= count) {
        finishTransfer();
        return false;
    }
    const std::uint32_t x = (transferX_ + static_cast<std::uint32_t>(transferIndex_ % transferWidth_)) % Vram::kWidth;
    const std::uint32_t y = (transferY_ + static_cast<std::uint32_t>(transferIndex_ / transferWidth_)) % Vram::kHeight;
    if (!vram_.readPixel(x, y, pixel)) return false;
    ++transferIndex_;
    if (transferIndex_ >= count) finishTransfer();
    return true;
}

void Gpu::finishTransfer() noexcept {
    transferMode_ = TransferMode::None;
    transferIndex_ = 0;
    readLatchValid_ = false;
}

void Gpu::executeEnvironmentCommand(std::uint32_t command) noexcept {
    switch ((command >> 24) & 0xFFu) {
        case 0xE1: drawMode_ = command & 0x00FFFFFFu; break;
        case 0xE2: textureWindow_ = command & 0x00FFFFFFu; break;
        case 0xE3: drawAreaTopLeft_ = command & 0x00FFFFFFu; break;
        case 0xE4: drawAreaBottomRight_ = command & 0x00FFFFFFu; break;
        case 0xE5: drawOffset_ = command & 0x00FFFFFFu; break;
        case 0xE6: maskSetting_ = command & 0x00FFFFFFu; break;
        default: break;
    }
}

void Gpu::writeGp1(std::uint32_t value) noexcept {
    const std::uint8_t command = static_cast<std::uint8_t>(value >> 24);
    const std::uint32_t data = value & 0x00FFFFFFu;
    switch (command) {
        case 0x00: reset(); break;
        case 0x01:
            packet_.clear();
            expectedWords_ = 0;
            transferMode_ = TransferMode::None;
            readLatchValid_ = false;
            break;
        case 0x02: irqPending_ = false; break;
        case 0x03: displayDisabled_ = (data & 1u) != 0; break;
        case 0x04: dmaDirection_ = data & 3u; break;
        case 0x05: displayStart_ = data & 0x0007FFFFu; break;
        case 0x06: horizontalRange_ = data; break;
        case 0x07: verticalRange_ = data; break;
        case 0x08: displayMode_ = data & 0x3Fu; break;
        default: break;
    }
}

std::uint32_t Gpu::readStatus() const noexcept {
    std::uint32_t status = 0x14802000u;
    status |= (displayDisabled_ ? 1u : 0u) << 23;
    status |= (dmaDirection_ & 3u) << 29;
    if (transferMode_ == TransferMode::None) status |= 1u << 26;
    if (transferMode_ != TransferMode::CpuToVram) status |= 1u << 28;
    if (transferMode_ == TransferMode::None) status |= 1u << 27;
    if (irqPending_) status |= 1u << 24;
    status |= (displayMode_ & 0x3Fu) << 0;
    return status;
}

} // namespace nexus::ps1
