#include "Gpu.hpp"

#include <algorithm>
#include <cstdlib>

namespace nexus::ps1 {

Gpu::Gpu(Vram& vram) noexcept : vram_(vram) {
    reset();
}

void Gpu::reset() noexcept {
    gp0Fifo_.clear();
    packet_.clear();
    expectedWords_ = 0;
    variablePolyline_ = false;
    transferMode_ = TransferMode::None;
    transferX_ = transferY_ = transferWidth_ = transferHeight_ = 0;
    transferIndex_ = 0;
    readLatch_ = 0;
    readLatchValid_ = false;
    gpuInfoLatch_ = 0;
    gpuInfoPending_ = false;
    oddField_ = false;
    irqPending_ = false;
    displayDisabled_ = true;
    dmaDirection_ = 0;
    displayStart_ = 0;
    horizontalRange_ = 0xC60260u;
    verticalRange_ = 0x3FC10u;
    displayMode_ = 0;
    gpuClockRemainder_ = 0;
    gp0ServiceCycleRemainder_ = 0;
    scanlineClock_ = 0;
    scanline_ = 0;
    dotClockRemainder_ = 0;
    hBlankActive_ = false;
    vBlankActive_ = true;
    drawMode_ = textureWindow_ = drawAreaTopLeft_ = 0;
    drawAreaBottomRight_ = drawOffset_ = maskSetting_ = 0;
    unsupportedCommandCount_ = 0;
}

GpuTimingEvents Gpu::advanceTiming(std::uint32_t cpuCycles) noexcept {
    GpuTimingEvents events{};
    // Approximate NTSC scanout clock domains: one modeled scanline is 3413
    // GPU clocks and a frame contains 263 lines (314 in PAL mode). This is a
    // timing counter, not a per-pixel renderer or cycle-accurate bus model.
    constexpr std::uint32_t kGpuClocksPerCpuNumerator = 11u;
    constexpr std::uint32_t kGpuClocksPerCpuDenominator = 7u;
    constexpr std::uint32_t kGpuClocksPerScanline = 3413u;
    constexpr std::uint32_t kGp0CpuCyclesPerWord = 10u;
    const std::uint32_t linesPerFrame = (displayMode_ & (1u << 3)) ? 314u : 263u;
    const std::uint32_t scaled = gpuClockRemainder_ +
        cpuCycles * kGpuClocksPerCpuNumerator;
    const std::uint32_t gpuClocks = scaled / kGpuClocksPerCpuDenominator;

    // Fixed approximate GP0 budget: one queued word per 10 CPU cycles.
    // Retain fractional cycle budget between CPU steps; no GPU opcode cost
    // table or cycle-by-cycle PS1 bus arbitration is attempted.
    const std::uint32_t serviceScaled = gp0ServiceCycleRemainder_ + cpuCycles;
    events.gp0ServiceWords = serviceScaled / kGp0CpuCyclesPerWord;
    gp0ServiceCycleRemainder_ = serviceScaled % kGp0CpuCyclesPerWord;
    gpuClockRemainder_ = scaled % kGpuClocksPerCpuDenominator;

    const std::uint32_t horizontalMode = displayMode_ & 3u;
    const bool highResolution368 = (displayMode_ & (1u << 6)) != 0u;
    constexpr std::uint32_t kDotDividers[4] = {10u, 8u, 5u, 4u};
    const std::uint32_t dotDivider = highResolution368 ? 7u : kDotDividers[horizontalMode];

    // GP1(06h) specifies the active horizontal display interval in GPU clocks.
    std::uint32_t hStart = horizontalRange_ & 0xFFFu;
    std::uint32_t hEnd = (horizontalRange_ >> 12) & 0xFFFu;
    if (hEnd <= hStart || hEnd > kGpuClocksPerScanline) {
        hStart = 608u;
        hEnd = 3168u;
    }

    // GP1(07h) specifies the active vertical display interval in scanlines.
    const std::uint32_t vStart = std::min(verticalRange_ & 0x3FFu, linesPerFrame);
    std::uint32_t vEnd = (verticalRange_ >> 10) & 0x3FFu;
    if (vEnd <= vStart || vEnd > linesPerFrame) vEnd = linesPerFrame - 8u;

    for (std::uint32_t i = 0; i < gpuClocks; ++i) {
        ++dotClockRemainder_;
        if (dotClockRemainder_ >= dotDivider) {
            dotClockRemainder_ -= dotDivider;
            ++events.dotClockTicks;
        }

        ++scanlineClock_;
        if (scanlineClock_ == hStart) {
            hBlankActive_ = false;
        }
        if (scanlineClock_ == hEnd) {
            hBlankActive_ = true;
            ++events.hBlankPulses;
        }

        if (scanlineClock_ >= kGpuClocksPerScanline) {
            scanlineClock_ = 0;
            scanline_ = (scanline_ + 1u) % linesPerFrame;
            if (scanline_ == 0u) oddField_ = !oddField_;
            hBlankActive_ = true;
            const bool wasVBlank = vBlankActive_;
            vBlankActive_ = scanline_ < vStart || scanline_ >= vEnd;
            if (!wasVBlank && vBlankActive_) events.vBlankStart = true;
        }
    }

    events.hBlankActive = hBlankActive_;
    events.vBlankActive = vBlankActive_;
    return events;
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
    // Synchronous clients (including DMA) still pass each word through FIFO.
    if (enqueueGp0(value)) serviceGp0Fifo(1u);
}

bool Gpu::enqueueGp0(std::uint32_t value) noexcept {
    return gp0Fifo_.push(value);
}

std::size_t Gpu::serviceGp0Fifo(std::size_t maxWords) {
    std::size_t serviced = 0;
    std::uint32_t value = 0;
    while (serviced < maxWords && gp0Fifo_.pop(value)) {
        processGp0Word(value);
        ++serviced;
    }
    return serviced;
}

void Gpu::processGp0Word(std::uint32_t value) {
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
    if (gpuInfoPending_) { gpuInfoPending_ = false; return gpuInfoLatch_; }
    if (gpuInfoPending_) { gpuInfoPending_ = false; return gpuInfoLatch_; }
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
        variablePolyline_ = false;
        if (opcode == 0x00 || opcode == 0x01) return;
        if (opcode >= 0xE1 && opcode <= 0xE6) {
            executeEnvironmentCommand(value);
            return;
        }
        if (opcode == 0x02) expectedWords_ = 3;
        else if (opcode >= 0x20 && opcode <= 0x3F) {
            const bool gouraud = (opcode & 0x10u) != 0u;
            const bool textured = (opcode & 0x04u) != 0u;
            const std::size_t vertices = (opcode & 0x08u) != 0u ? 4u : 3u;
            expectedWords_ = 1u + vertices * (1u + (textured ? 1u : 0u)) +
                             (gouraud ? vertices - 1u : 0u);
        }
        else if (opcode >= 0x40 && opcode <= 0x47) expectedWords_ = 3;
        else if (opcode >= 0x50 && opcode <= 0x57) expectedWords_ = 4;
        else if ((opcode >= 0x48 && opcode <= 0x4F) ||
                 (opcode >= 0x58 && opcode <= 0x5F)) {
            // Poly-lines are variable-length. Both flat and Gouraud variants
            // end when an XY word has the 5xxx5xxx terminator pattern.
            variablePolyline_ = true;
        }
        else if (opcode >= 0x60 && opcode <= 0x63) expectedWords_ = 3;
        else if (opcode >= 0x64 && opcode <= 0x67) expectedWords_ = 4;
        else if ((opcode >= 0x68 && opcode <= 0x6B) ||
                 (opcode >= 0x70 && opcode <= 0x73) ||
                 (opcode >= 0x78 && opcode <= 0x7B)) expectedWords_ = 2;
        else if ((opcode >= 0x6C && opcode <= 0x6F) ||
                 (opcode >= 0x74 && opcode <= 0x77) ||
                 (opcode >= 0x7C && opcode <= 0x7F)) expectedWords_ = 3;
        else if (opcode == 0x80) expectedWords_ = 4;
        else if (opcode == 0xA0 || opcode == 0xC0) expectedWords_ = 3;
        else {
            ++unsupportedCommandCount_;
            return;
        }

    }

    packet_.push_back(value);
    if (variablePolyline_) {
        const std::uint8_t opcode = static_cast<std::uint8_t>(packet_[0] >> 24);
        const bool gouraudPolyline = (opcode & 0x10u) != 0u;
        const bool isVertexWord = gouraudPolyline
            ? (packet_.size() >= 4u && (packet_.size() % 2u) == 0u)
            : packet_.size() >= 3u;
        if (isVertexWord && (value & 0xF000F000u) == 0x50005000u) {
            // The terminator occupies an XY slot and is not a drawable vertex.
            packet_.pop_back();
            executePolyline();
            packet_.clear();
            expectedWords_ = 0;
            variablePolyline_ = false;
        } else if (packet_.size() >= 4096u) {
            // Bound malformed streams that never provide a polyline terminator.
            packet_.clear();
            expectedWords_ = 0;
            variablePolyline_ = false;
            ++unsupportedCommandCount_;
        }
        return;
    }
    if (packet_.size() == expectedWords_) {
        executePacket();
        packet_.clear();
        expectedWords_ = 0;
        variablePolyline_ = false;
    }
}

void Gpu::executePolyline() noexcept {
    if (packet_.empty()) return;
    const std::uint8_t opcode = static_cast<std::uint8_t>(packet_[0] >> 24);
    const bool gouraud = (opcode & 0x10u) != 0u;
    const bool semiTransparent = (opcode & 0x02u) != 0u;
    const auto decodeCoordinate = [](std::uint32_t value) noexcept {
        value &= 0x7FFu;
        return (value & 0x400u) ? static_cast<int>(value) - 0x800
                                : static_cast<int>(value);
    };
    const int ox = decodeCoordinate(drawOffset_);
    const int oy = decodeCoordinate(drawOffset_ >> 11);

    if (!gouraud) {
        if (packet_.size() < 3u) return;
        const std::uint16_t color =
            static_cast<std::uint16_t>(color24To15(packet_[0]));
        int previousX = decodeCoordinate(packet_[1]) + ox;
        int previousY = decodeCoordinate(packet_[1] >> 16) + oy;
        for (std::size_t i = 2u; i < packet_.size(); ++i) {
            const int x = decodeCoordinate(packet_[i]) + ox;
            const int y = decodeCoordinate(packet_[i] >> 16) + oy;
            drawLine(previousX, previousY, x, y, color, semiTransparent);
            previousX = x;
            previousY = y;
        }
        return;
    }

    // Gouraud packet layout: command(color0), XY0, color1, XY1, ...
    // The terminator is in the next XY slot, so the preceding color word
    // belongs to no vertex and must be discarded.
    if ((packet_.size() & 1u) != 0u) packet_.pop_back();
    if (packet_.size() < 4u) return;
    const std::vector<std::uint32_t> polyline = packet_;
    std::vector<std::uint32_t> segment(4u);
    segment[0] = (static_cast<std::uint32_t>(opcode) << 24u) |
                 (polyline[0] & 0x00FFFFFFu);
    for (std::size_t xyIndex = 1u; xyIndex + 2u < polyline.size(); xyIndex += 2u) {
        segment[1] = polyline[xyIndex];
        segment[2] = polyline[xyIndex + 1u];
        segment[3] = polyline[xyIndex + 2u];
        packet_ = segment;
        executeGouraudLine();
        segment[0] = (static_cast<std::uint32_t>(opcode) << 24u) |
                     (polyline[xyIndex + 1u] & 0x00FFFFFFu);
    }
    packet_ = polyline;
}

void Gpu::executePacket() {
    const std::uint8_t opcode = static_cast<std::uint8_t>(packet_[0] >> 24);
    if (opcode == 0x02) {
        executeFillRectangle();
    } else if (opcode >= 0x20 && opcode <= 0x3F) {
        if ((opcode & 0x04u) != 0u) executeTexturedPolygon();
        else if ((opcode & 0x10u) != 0u) executeGouraudPolygon();
        else executeFlatPolygon();
    } else if (opcode >= 0x40 && opcode <= 0x47) {
        executeFlatLine();
    } else if (opcode >= 0x50 && opcode <= 0x57) {
        executeGouraudLine();
    } else if ((opcode >= 0x64 && opcode <= 0x67) ||
               (opcode >= 0x6C && opcode <= 0x6F) ||
               (opcode >= 0x74 && opcode <= 0x77) ||
               (opcode >= 0x7C && opcode <= 0x7F)) {
        executeTexturedRectangle();
    } else if ((opcode >= 0x60 && opcode <= 0x63) ||
               (opcode >= 0x68 && opcode <= 0x6B) ||
               (opcode >= 0x70 && opcode <= 0x73) ||
               (opcode >= 0x78 && opcode <= 0x7B)) {
        executeRectangle();
    } else if (opcode == 0x80) {
        executeVramCopy();
    } else if (opcode == 0xA0) {
        beginImageTransfer(TransferMode::CpuToVram);
    } else if (opcode == 0xC0) {
        beginImageTransfer(TransferMode::VramToCpu);
    }
}

void Gpu::executeFillRectangle() noexcept {
    const std::uint32_t color = color24To15(packet_[0]);
    const std::uint32_t rawX = packet_[1] & 0x3FFu;
    const std::uint32_t rawY = (packet_[1] >> 16) & 0x1FFu;
    const std::uint32_t ew = packet_[2] & 0x3FFu, eh = (packet_[2] >> 16) & 0x1FFu;
    const std::uint32_t width = ew == 0u ? 1024u : ew, height = eh == 0u ? 512u : eh;
    const std::uint32_t x = rawX & ~15u;
    const std::uint32_t alignedWidth = std::min(1024u, (width + 15u) & ~15u);
    for (std::uint32_t row=0; row<height; ++row)
        for (std::uint32_t col=0; col<alignedWidth; ++col)
            vram_.writePixel((x+col)&0x3FFu,(rawY+row)&0x1FFu,static_cast<std::uint16_t>(color));
}

void Gpu::executeVramCopy() noexcept {
    const std::uint32_t sx=packet_[1]&0x3FFu, sy=(packet_[1]>>16)&0x1FFu;
    const std::uint32_t dx=packet_[2]&0x3FFu, dy=(packet_[2]>>16)&0x1FFu;
    const std::uint32_t ew=packet_[3]&0x3FFu, eh=(packet_[3]>>16)&0x1FFu;
    const std::uint32_t width=ew==0u?1024u:ew, height=eh==0u?512u:eh;
    std::vector<std::uint16_t> copy(static_cast<std::size_t>(width)*height);
    for(std::uint32_t y=0;y<height;++y) for(std::uint32_t x=0;x<width;++x) {
        std::uint16_t p=0; vram_.readPixel((sx+x)&0x3FFu,(sy+y)&0x1FFu,p);
        copy[static_cast<std::size_t>(y)*width+x]=p;
    }
    for(std::uint32_t y=0;y<height;++y) for(std::uint32_t x=0;x<width;++x)
        vram_.writePixel((dx+x)&0x3FFu,(dy+y)&0x1FFu,copy[static_cast<std::size_t>(y)*width+x]);
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

std::uint16_t ditheredColor24To15(std::uint32_t rgb, int x, int y) noexcept {
    // PS1 4x4 signed dither matrix, applied before reducing 8-bit channels
    // to the 5-bit RGB values stored in VRAM.
    constexpr int matrix[4][4] = {
        {-4,  0, -3,  1},
        { 2, -2,  3, -1},
        {-3,  1, -4,  0},
        { 3, -1,  2, -2}
    };
    const int adjustment = matrix[y & 3][x & 3];
    auto reduce = [adjustment](std::uint32_t channel) noexcept {
        const int adjusted = std::clamp(static_cast<int>(channel) + adjustment, 0, 255);
        return static_cast<std::uint16_t>(adjusted >> 3);
    };
    const std::uint16_t r = reduce(rgb & 0xFFu);
    const std::uint16_t g = reduce((rgb >> 8) & 0xFFu);
    const std::uint16_t b = reduce((rgb >> 16) & 0xFFu);
    return static_cast<std::uint16_t>(r | (g << 5) | (b << 10));
}
} // namespace

void Gpu::executeFlatPolygon() noexcept {
    if (packet_.size() < 4) return;
    const std::uint32_t color = packet_[0] & 0x00FFFFFFu;
    const int offsetX = signExtend11(drawOffset_ & 0x7FFu);
    const int offsetY = signExtend11((drawOffset_ >> 11) & 0x7FFu);
    int x[4]{};
    int y[4]{};
    const std::uint8_t opcode = static_cast<std::uint8_t>(packet_[0] >> 24);
    const int vertices = (opcode & 0x08u) != 0u ? 4 : 3;
    const bool semiTransparent = (opcode & 0x02u) != 0u;
    for (int i = 0; i < vertices; ++i) {
        const std::uint32_t packed = packet_[static_cast<std::size_t>(i) + 1];
        x[i] = signExtend11(packed) + offsetX;
        y[i] = signExtend11(packed >> 16) + offsetY;
    }
    drawTriangle(x[0], y[0], x[1], y[1], x[2], y[2], color, semiTransparent);
    if (vertices == 4) {
        drawTriangle(x[1], y[1], x[2], y[2], x[3], y[3], color, semiTransparent);
    }
}

void Gpu::drawTriangle(int x0, int y0, int x1, int y1,
                       int x2, int y2, std::uint32_t rgb,
                       bool semiTransparent) noexcept {
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
                ? (e0 >= 0 && e1 >= 0 && e2 >= 0)
                : (e0 <= 0 && e1 <= 0 && e2 <= 0);
            if (inside) {
                const std::uint16_t color = (drawMode_ & (1u << 9)) != 0
                    ? ditheredColor24To15(rgb, px, py)
                    : static_cast<std::uint16_t>(color24To15(rgb));
                writeDrawPixel(px, py, color, semiTransparent);
            }
        }
    }
}




void Gpu::executeTexturedPolygon() noexcept {
    const std::uint8_t opcode = static_cast<std::uint8_t>(packet_[0] >> 24);
    const bool quad = (opcode & 0x08u) != 0u;
    const bool gouraud = (opcode & 0x10u) != 0u;
    const bool rawTexture = (opcode & 0x01u) != 0u;
    const bool semiTransparent = (opcode & 0x02u) != 0u;
    const int count = quad ? 4 : 3;
    int x[4]{}, y[4]{}, u[4]{}, v[4]{};
    std::uint32_t colors[4]{};
    std::uint16_t page = 0;
    const int ox = signExtend11(drawOffset_ & 0x7FFu);
    const int oy = signExtend11((drawOffset_ >> 11) & 0x7FFu);
    std::size_t cursor = 0;
    colors[0] = packet_[0];
    for (int i = 0; i < count; ++i) {
        if (i != 0 && gouraud) colors[i] = packet_[++cursor];
        const std::uint32_t xy = packet_[++cursor];
        const std::uint32_t uv = packet_[++cursor];
        x[i] = signExtend11(xy) + ox;
        y[i] = signExtend11(xy >> 16) + oy;
        u[i] = static_cast<int>(uv & 0xFFu);
        v[i] = static_cast<int>((uv >> 8) & 0xFFu);
        if (i == 1) page = static_cast<std::uint16_t>(uv >> 16);
    }
    const std::uint32_t c0[3] = {colors[0], colors[1], colors[2]};
    drawTexturedTriangle(x, y, u, v, page, rawTexture, semiTransparent,
                         gouraud ? c0 : nullptr);
    if (quad) {
        const int x2[3] = {x[1], x[2], x[3]};
        const int y2[3] = {y[1], y[2], y[3]};
        const int u2[3] = {u[1], u[2], u[3]};
        const int v2[3] = {v[1], v[2], v[3]};
        const std::uint32_t c1[3] = {colors[1], colors[2], colors[3]};
        drawTexturedTriangle(x2, y2, u2, v2, page, rawTexture, semiTransparent,
                             gouraud ? c1 : nullptr);
    }
}

void Gpu::drawTexturedTriangle(const int x[3], const int y[3],
                               const int u[3], const int v[3],
                               std::uint16_t page, bool rawTexture, bool semiTransparent,
                               const std::uint32_t colors[3]) noexcept {
    const unsigned depth = (page >> 7) & 3u;
    if (depth == 3u) {
        ++unsupportedCommandCount_;
        return;
    }
    const long long area = edgeFunction(x[0], y[0], x[1], y[1], x[2], y[2]);
    if (area == 0) return;
    const int left = static_cast<int>(drawAreaTopLeft_ & 0x3FFu);
    const int top = static_cast<int>((drawAreaTopLeft_ >> 10) & 0x1FFu);
    const int right = static_cast<int>(drawAreaBottomRight_ & 0x3FFu);
    const int bottom = static_cast<int>((drawAreaBottomRight_ >> 10) & 0x1FFu);
    const int minX = std::max({0, left, std::min({x[0], x[1], x[2]})});
    const int minY = std::max({0, top, std::min({y[0], y[1], y[2]})});
    const int maxX = std::min({static_cast<int>(Vram::kWidth)-1, right, std::max({x[0],x[1],x[2]})});
    const int maxY = std::min({static_cast<int>(Vram::kHeight)-1, bottom, std::max({y[0],y[1],y[2]})});
    if (minX > maxX || minY > maxY) return;

    const int pageX = static_cast<int>(page & 0xFu) * 64;
    const int pageY = static_cast<int>((page >> 4) & 1u) * 256;
    const std::uint32_t windowMaskX = textureWindow_ & 0x1Fu;
    const std::uint32_t windowMaskY = (textureWindow_ >> 5) & 0x1Fu;
    const std::uint32_t windowOffX = (textureWindow_ >> 10) & 0x1Fu;
    const std::uint32_t windowOffY = (textureWindow_ >> 15) & 0x1Fu;
    // CLUT coordinates are supplied in the high half of the first UV word.
    const std::uint32_t clut = packet_[2] >> 16;
    const std::uint32_t clutX = (clut & 0x3Fu) * 16u;
    const std::uint32_t clutY = (clut >> 6) & 0x1FFu;

    for (int py=minY; py<=maxY; ++py) for (int px=minX; px<=maxX; ++px) {
        long long w0=edgeFunction(x[1],y[1],x[2],y[2],px,py);
        long long w1=edgeFunction(x[2],y[2],x[0],y[0],px,py);
        long long w2=edgeFunction(x[0],y[0],x[1],y[1],px,py);
        if (area>0 ? (w0<0||w1<0||w2<0) : (w0>0||w1>0||w2>0)) continue;
        if (area<0) { w0=-w0; w1=-w1; w2=-w2; }
        const long long den=area>0?area:-area;
        int tu=static_cast<int>((w0*u[0]+w1*u[1]+w2*u[2])/den) & 0xFF;
        int tv=static_cast<int>((w0*v[0]+w1*v[1]+w2*v[2])/den) & 0xFF;
        tu = (tu & ~static_cast<int>(windowMaskX << 3)) |
             static_cast<int>((windowOffX & windowMaskX) << 3);
        tv = (tv & ~static_cast<int>(windowMaskY << 3)) |
             static_cast<int>((windowOffY & windowMaskY) << 3);

        std::uint16_t texel = 0;
        if (depth == 2u) {
            const std::uint32_t tx=static_cast<std::uint32_t>((pageX+tu)%static_cast<int>(Vram::kWidth));
            const std::uint32_t ty=static_cast<std::uint32_t>((pageY+tv)%static_cast<int>(Vram::kHeight));
            vram_.readPixel(tx,ty,texel);
        } else {
            const unsigned pixelsPerWord = depth == 0u ? 4u : 2u;
            const unsigned bitsPerPixel = depth == 0u ? 4u : 8u;
            const std::uint32_t wordX=static_cast<std::uint32_t>((pageX + tu / static_cast<int>(pixelsPerWord)) % static_cast<int>(Vram::kWidth));
            const std::uint32_t wordY=static_cast<std::uint32_t>((pageY+tv)%static_cast<int>(Vram::kHeight));
            std::uint16_t packed = 0;
            if (!vram_.readPixel(wordX,wordY,packed)) continue;
            const unsigned shift=static_cast<unsigned>(tu % static_cast<int>(pixelsPerWord))*bitsPerPixel;
            const std::uint16_t paletteIndex=static_cast<std::uint16_t>((packed >> shift) & (depth==0u?0xFu:0xFFu));
            const std::uint32_t paletteX=(clutX+paletteIndex)%Vram::kWidth;
            if (!vram_.readPixel(paletteX,clutY,texel)) continue;
        }
        // PS1 texture index/color zero is transparent.
        if (texel != 0) {
            const bool texelStp = (texel & 0x8000u) != 0;
            if (!rawTexture) {
                std::uint32_t modulation = packet_[0];
                if (colors != nullptr) {
                    const std::uint64_t den = static_cast<std::uint64_t>(area > 0 ? area : -area);
                    modulation = 0;
                    for (unsigned ch = 0; ch < 3; ++ch) {
                        const std::uint64_t a = (colors[0] >> (ch * 8u)) & 0xFFu;
                        const std::uint64_t b = (colors[1] >> (ch * 8u)) & 0xFFu;
                        const std::uint64_t c = (colors[2] >> (ch * 8u)) & 0xFFu;
                        const std::uint64_t value = (a * static_cast<std::uint64_t>(w0) +
                            b * static_cast<std::uint64_t>(w1) +
                            c * static_cast<std::uint64_t>(w2)) / den;
                        modulation |= static_cast<std::uint32_t>(value) << (ch * 8u);
                    }
                }
                const std::uint32_t tr = texel & 31u;
                const std::uint32_t tg = (texel >> 5) & 31u;
                const std::uint32_t tb = (texel >> 10) & 31u;
                const std::uint32_t cr = modulation & 0xFFu;
                const std::uint32_t cg = (modulation >> 8) & 0xFFu;
                const std::uint32_t cb = (modulation >> 16) & 0xFFu;
                const std::uint32_t r = std::min(31u, (tr * cr) / 128u);
                const std::uint32_t g = std::min(31u, (tg * cg) / 128u);
                const std::uint32_t b = std::min(31u, (tb * cb) / 128u);
                texel = static_cast<std::uint16_t>(r | (g << 5) | (b << 10));
            }
            const std::uint32_t dx = static_cast<std::uint32_t>(px);
            const std::uint32_t dy = static_cast<std::uint32_t>(py);
            std::uint16_t destination = 0;
            if (!vram_.readPixel(dx, dy, destination)) continue;
            if ((maskSetting_ & 0x02u) != 0 && (destination & 0x8000u) != 0) continue;
            if (semiTransparent && texelStp) {
                const std::uint32_t mode = (drawMode_ >> 5) & 3u;
                const std::uint32_t sr = texel & 31u, sg = (texel >> 5) & 31u, sb = (texel >> 10) & 31u;
                const std::uint32_t dr = destination & 31u, dg = (destination >> 5) & 31u, db = (destination >> 10) & 31u;
                auto blend = [mode](std::uint32_t source, std::uint32_t dest) noexcept {
                    switch (mode) {
                    case 0: return (source + dest) >> 1;
                    case 1: return std::min(31u, source + dest);
                    case 2: return dest > source ? dest - source : 0u;
                    default: return std::min(31u, dest + (source >> 2));
                    }
                };
                texel = static_cast<std::uint16_t>(blend(sr,dr) | (blend(sg,dg)<<5) | (blend(sb,db)<<10));
            }
            if ((maskSetting_ & 0x01u) != 0 || texelStp) texel |= 0x8000u;
            vram_.writePixel(dx, dy, texel);
        }
    }
}

void Gpu::executeGouraudPolygon() noexcept {
    const std::uint8_t opcode = static_cast<std::uint8_t>(packet_[0] >> 24);
    const bool quad = (opcode & 0x08u) != 0u;
    const bool semiTransparent = (opcode & 0x02u) != 0u;
    const int vertexCount = quad ? 4 : 3;
    int x[4]{};
    int y[4]{};
    std::uint32_t colors[4]{};
    const int offsetX = signExtend11(drawOffset_ & 0x7FFu);
    const int offsetY = signExtend11((drawOffset_ >> 11) & 0x7FFu);
    for (int i = 0; i < vertexCount; ++i) {
        const std::size_t colorWord = static_cast<std::size_t>(i) * 2;
        const std::size_t xyWord = colorWord + 1;
        colors[i] = packet_[colorWord];
        x[i] = signExtend11(packet_[xyWord]) + offsetX;
        y[i] = signExtend11(packet_[xyWord] >> 16) + offsetY;
    }
    drawGouraudTriangle(x[0], y[0], colors[0],
                        x[1], y[1], colors[1],
                        x[2], y[2], colors[2], semiTransparent);
    if (quad) {
        drawGouraudTriangle(x[1], y[1], colors[1],
                            x[2], y[2], colors[2],
                            x[3], y[3], colors[3], semiTransparent);
    }
}

void Gpu::drawGouraudTriangle(int x0, int y0, std::uint32_t c0,
                              int x1, int y1, std::uint32_t c1,
                              int x2, int y2, std::uint32_t c2, bool semiTransparent) noexcept {
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
    const std::uint32_t channel0[3] = {c0 & 0xFFu, (c0 >> 8) & 0xFFu, (c0 >> 16) & 0xFFu};
    const std::uint32_t channel1[3] = {c1 & 0xFFu, (c1 >> 8) & 0xFFu, (c1 >> 16) & 0xFFu};
    const std::uint32_t channel2[3] = {c2 & 0xFFu, (c2 >> 8) & 0xFFu, (c2 >> 16) & 0xFFu};
    for (int py = minY; py <= maxY; ++py) {
        for (int px = minX; px <= maxX; ++px) {
            long long w2 = edgeFunction(x0, y0, x1, y1, px, py);
            long long w0 = edgeFunction(x1, y1, x2, y2, px, py);
            long long w1 = edgeFunction(x2, y2, x0, y0, px, py);
            const bool inside = area > 0 ? (w0 >= 0 && w1 >= 0 && w2 >= 0)
                                         : (w0 <= 0 && w1 <= 0 && w2 <= 0);
            if (!inside) continue;
            if (area < 0) { w0 = -w0; w1 = -w1; w2 = -w2; }
            const std::uint64_t denominator = static_cast<std::uint64_t>(area > 0 ? area : -area);
            std::uint32_t rgb = 0;
            for (int channel = 0; channel < 3; ++channel) {
                const std::uint64_t weighted =
                    static_cast<std::uint64_t>(channel0[channel]) * static_cast<std::uint64_t>(w0) +
                    static_cast<std::uint64_t>(channel1[channel]) * static_cast<std::uint64_t>(w1) +
                    static_cast<std::uint64_t>(channel2[channel]) * static_cast<std::uint64_t>(w2);
                const std::uint32_t value = static_cast<std::uint32_t>(weighted / denominator);
                rgb |= value << (channel * 8);
            }
            const std::uint16_t output = (drawMode_ & (1u << 9)) != 0
                ? ditheredColor24To15(rgb, px, py)
                : static_cast<std::uint16_t>(color24To15(rgb));
            writeDrawPixel(px, py, output, semiTransparent);
        }
    }
}

void Gpu::executeRectangle() noexcept {
    const std::uint8_t op=static_cast<std::uint8_t>(packet_[0]>>24);
    const bool variable=op>=0x60&&op<=0x63;
    const std::uint32_t ew=variable?(packet_[2]&0x3FFu):(op>=0x78?16u:(op>=0x70?8u:1u));
    const std::uint32_t eh=variable?((packet_[2]>>16)&0x1FFu):ew;
    const std::uint32_t w=variable&&ew==0u?1024u:ew, h=variable&&eh==0u?512u:eh;
    if(!w||!h)return;
    const std::uint16_t src=static_cast<std::uint16_t>(color24To15(packet_[0]));
    const int x=signExtend11(packet_[1])+signExtend11(drawOffset_&0x7FFu);
    const int y=signExtend11(packet_[1]>>16)+signExtend11((drawOffset_>>11)&0x7FFu);
    const int l=static_cast<int>(drawAreaTopLeft_&0x3FFu),t=static_cast<int>((drawAreaTopLeft_>>10)&0x1FFu);
    const int r=static_cast<int>(drawAreaBottomRight_&0x3FFu),b=static_cast<int>((drawAreaBottomRight_>>10)&0x1FFu);
    const int x0=std::max({0,l,x}),y0=std::max({0,t,y});
    const int x1=std::min({static_cast<int>(Vram::kWidth),r+1,x+static_cast<int>(w)});
    const int y1=std::min({static_cast<int>(Vram::kHeight),b+1,y+static_cast<int>(h)});
    const bool semi=(op&2u)!=0u; const unsigned mode=(drawMode_>>5)&3u;
    const unsigned sr=src&31u,sg=(src>>5)&31u,sb=(src>>10)&31u;
    auto blend=[mode](unsigned f,unsigned back){switch(mode){case 0:return(f+back)>>1;case 1:return std::min(31u,f+back);case 2:return back>f?back-f:0u;default:return std::min(31u,back+(f>>2));}};
    for(int py=y0;py<y1;++py)for(int px=x0;px<x1;++px){
        std::uint16_t d=0;if(!vram_.readPixel(px,py,d))continue;
        if((maskSetting_&2u)&&(d&0x8000u))continue;
        std::uint16_t out=src;
        if(semi)out=static_cast<std::uint16_t>(blend(sr,d&31u)|(blend(sg,(d>>5)&31u)<<5)|(blend(sb,(d>>10)&31u)<<10));
        if(maskSetting_&1u)out|=0x8000u;vram_.writePixel(px,py,out);
    }
}



void Gpu::executeTexturedRectangle() noexcept {
    const std::uint8_t op=static_cast<std::uint8_t>(packet_[0]>>24);
    const bool variable=op>=0x64&&op<=0x67;
    const std::uint32_t ew=variable?(packet_[3]&0x3FFu):(op>=0x7C?16u:(op>=0x74?8u:1u));
    const std::uint32_t eh=variable?((packet_[3]>>16)&0x1FFu):(op>=0x7C?16u:(op>=0x74?8u:1u));
    const std::uint32_t w=variable&&ew==0u?1024u:ew,h=variable&&eh==0u?512u:eh;
    if(!w||!h)return;
    const std::uint32_t xy=packet_[1],uv=packet_[2];
    const int ox=signExtend11(xy)+signExtend11(drawOffset_&0x7FFu),oy=signExtend11(xy>>16)+signExtend11((drawOffset_>>11)&0x7FFu);
    const int u0=uv&255u,v0=(uv>>8)&255u;
    const std::uint32_t clut=uv>>16,cx=(clut&63u)*16u,cy=(clut>>6)&511u;
    const unsigned depth=(drawMode_>>7)&3u;if(depth==3u){++unsupportedCommandCount_;return;}
    const std::uint32_t px=(drawMode_&15u)*64u,py=((drawMode_>>4)&1u)*256u;
    const std::uint32_t mx=textureWindow_&31u,my=(textureWindow_>>5)&31u,wx=(textureWindow_>>10)&31u,wy=(textureWindow_>>15)&31u;
    const bool raw=(op&1u)!=0u,semi=(op&2u)!=0u;const unsigned mode=(drawMode_>>5)&3u;
    const int l=drawAreaTopLeft_&1023u,t=(drawAreaTopLeft_>>10)&511u,r=drawAreaBottomRight_&1023u,b=(drawAreaBottomRight_>>10)&511u;
    const int x0=std::max({0,l,ox}),y0=std::max({0,t,oy}),x1=std::min({static_cast<int>(Vram::kWidth),r+1,ox+static_cast<int>(w)}),y1=std::min({static_cast<int>(Vram::kHeight),b+1,oy+static_cast<int>(h)});
    const unsigned cr=packet_[0]&255u,cg=(packet_[0]>>8)&255u,cb=(packet_[0]>>16)&255u;
    auto blend=[mode](unsigned f,unsigned back){switch(mode){case 0:return(f+back)>>1;case 1:return std::min(31u,f+back);case 2:return back>f?back-f:0u;default:return std::min(31u,back+(f>>2));}};
    for(int y=y0;y<y1;++y)for(int x=x0;x<x1;++x){
        int u=(u0+x-ox)&255,v=(v0+y-oy)&255;u=(u&~static_cast<int>(mx<<3))|static_cast<int>((wx&mx)<<3);v=(v&~static_cast<int>(my<<3))|static_cast<int>((wy&my)<<3);
        std::uint16_t tex=0;
        if(depth==2u){if(!vram_.readPixel((px+u)%Vram::kWidth,(py+v)%Vram::kHeight,tex))continue;}
        else{const unsigned ppw=depth==0u?4u:2u,bits=depth==0u?4u:8u;std::uint16_t packed=0;
            if(!vram_.readPixel((px+static_cast<unsigned>(u)/ppw)%Vram::kWidth,(py+v)%Vram::kHeight,packed))continue;
            const unsigned idx=(packed>>((static_cast<unsigned>(u)%ppw)*bits))&(depth==0u?15u:255u);
            if(!vram_.readPixel((cx+idx)%Vram::kWidth,cy,tex))continue;}
        if(tex==0u)continue;const bool stp=(tex&0x8000u)!=0u;
        if(!raw){const unsigned rr=std::min(31u,((tex&31u)*cr)/128u),gg=std::min(31u,(((tex>>5)&31u)*cg)/128u),bb=std::min(31u,(((tex>>10)&31u)*cb)/128u);tex=static_cast<std::uint16_t>(rr|(gg<<5)|(bb<<10));}
        std::uint16_t d=0;if(!vram_.readPixel(x,y,d))continue;if((maskSetting_&2u)&&(d&0x8000u))continue;
        if(semi&&stp)tex=static_cast<std::uint16_t>(blend(tex&31u,d&31u)|(blend((tex>>5)&31u,(d>>5)&31u)<<5)|(blend((tex>>10)&31u,(d>>10)&31u)<<10));
        if((maskSetting_&1u)||stp)tex|=0x8000u;vram_.writePixel(x,y,tex);
    }
}



void Gpu::executeFlatLine() noexcept {
    const std::uint8_t opcode = static_cast<std::uint8_t>(packet_[0] >> 24);
    const std::uint16_t color = static_cast<std::uint16_t>(color24To15(packet_[0]));
    const int offsetX = signExtend11(drawOffset_ & 0x7FFu);
    const int offsetY = signExtend11((drawOffset_ >> 11) & 0x7FFu);
    drawLine(signExtend11(packet_[1]) + offsetX,
             signExtend11(packet_[1] >> 16) + offsetY,
             signExtend11(packet_[2]) + offsetX,
             signExtend11(packet_[2] >> 16) + offsetY,
             color, (opcode & 2u) != 0u);
}

void Gpu::writeDrawPixel(int x, int y, std::uint16_t source,
                         bool semiTransparent) noexcept {
    std::uint16_t destination = 0;
    if (!vram_.readPixel(static_cast<std::uint32_t>(x),
                         static_cast<std::uint32_t>(y), destination)) return;
    if ((maskSetting_ & 0x02u) != 0u && (destination & 0x8000u) != 0u) return;
    std::uint16_t output = source;
    if (semiTransparent) {
        const std::uint32_t mode = (drawMode_ >> 5) & 3u;
        const std::uint32_t sr = source & 31u, sg = (source >> 5) & 31u, sb = (source >> 10) & 31u;
        const std::uint32_t dr = destination & 31u, dg = (destination >> 5) & 31u, db = (destination >> 10) & 31u;
        const auto blend = [mode](std::uint32_t s, std::uint32_t d) noexcept {
            switch (mode) {
                case 0: return (s + d) >> 1;
                case 1: return std::min(31u, s + d);
                case 2: return d > s ? d - s : 0u;
                default: return std::min(31u, d + (s >> 2));
            }
        };
        output = static_cast<std::uint16_t>(blend(sr, dr) | (blend(sg, dg) << 5) | (blend(sb, db) << 10));
    }
    if ((maskSetting_ & 1u) != 0u) output |= 0x8000u;
    vram_.writePixel(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), output);
}

void Gpu::drawLine(int x0, int y0, int x1, int y1,
                   std::uint16_t color, bool semiTransparent) noexcept {
    const int left = static_cast<int>(drawAreaTopLeft_ & 0x3FFu);
    const int top = static_cast<int>((drawAreaTopLeft_ >> 10) & 0x1FFu);
    const int right = static_cast<int>(drawAreaBottomRight_ & 0x3FFu);
    const int bottom = static_cast<int>((drawAreaBottomRight_ >> 10) & 0x1FFu);
    const int dx = std::abs(x1 - x0);
    const int sx = x0 < x1 ? 1 : -1;
    const int dy = -std::abs(y1 - y0);
    const int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    for (;;) {
        if (x0 >= left && x0 <= right && y0 >= top && y0 <= bottom &&
            x0 >= 0 && x0 < static_cast<int>(Vram::kWidth) &&
            y0 >= 0 && y0 < static_cast<int>(Vram::kHeight)) {
            writeDrawPixel(x0, y0, color, semiTransparent);
        }
        if (x0 == x1 && y0 == y1) break;
        const int twiceError = 2 * error;
        if (twiceError >= dy) { error += dy; x0 += sx; }
        if (twiceError <= dx) { error += dx; y0 += sy; }
    }
}

void Gpu::executeGouraudLine() noexcept {
    const std::uint8_t opcode = static_cast<std::uint8_t>(packet_[0] >> 24);
    const bool semiTransparent = (opcode & 2u) != 0u;
    const int ox = signExtend11(drawOffset_ & 0x7FFu);
    const int oy = signExtend11((drawOffset_ >> 11) & 0x7FFu);
    int x = signExtend11(packet_[1]) + ox;
    int y = signExtend11(packet_[1] >> 16) + oy;
    const int endX = signExtend11(packet_[3]) + ox;
    const int endY = signExtend11(packet_[3] >> 16) + oy;
    const std::uint32_t c0 = packet_[0] & 0x00FFFFFFu;
    const std::uint32_t c1 = packet_[2] & 0x00FFFFFFu;
    const int dx = std::abs(endX - x), sx = x < endX ? 1 : -1;
    const int dy = -std::abs(endY - y), sy = y < endY ? 1 : -1;
    const int total = std::max(dx, -dy);
    const int left = static_cast<int>(drawAreaTopLeft_ & 0x3FFu);
    const int top = static_cast<int>((drawAreaTopLeft_ >> 10) & 0x1FFu);
    const int right = static_cast<int>(drawAreaBottomRight_ & 0x3FFu);
    const int bottom = static_cast<int>((drawAreaBottomRight_ >> 10) & 0x1FFu);
    int error = dx + dy, step = 0;
    for (;;) {
        if (x >= left && x <= right && y >= top && y <= bottom &&
            x >= 0 && x < static_cast<int>(Vram::kWidth) &&
            y >= 0 && y < static_cast<int>(Vram::kHeight)) {
            std::uint32_t rgb = 0;
            for (int channel = 0; channel < 3; ++channel) {
                const int a = static_cast<int>((c0 >> (channel * 8)) & 0xFFu);
                const int b = static_cast<int>((c1 >> (channel * 8)) & 0xFFu);
                const int value = total == 0 ? a : a + ((b - a) * step) / total;
                rgb |= static_cast<std::uint32_t>(std::clamp(value, 0, 255)) << (channel * 8);
            }
            const std::uint16_t color = (drawMode_ & (1u << 9)) != 0u
                ? ditheredColor24To15(rgb, x, y)
                : static_cast<std::uint16_t>(color24To15(rgb));
            writeDrawPixel(x, y, color, semiTransparent);
        }
        if (x == endX && y == endY) break;
        const int twice = 2 * error;
        if (twice >= dy) { error += dy; x += sx; }
        if (twice <= dx) { error += dx; y += sy; }
        ++step;
    }
}

void Gpu::beginImageTransfer(TransferMode mode) {
    transferX_ = packet_[1] & 0x3FFu;
    transferY_ = (packet_[1] >> 16) & 0x1FFu;
    transferWidth_ = decodeTransferDimension(
        static_cast<std::uint16_t>(packet_[2] & 0x3FFu), Vram::kWidth);
    transferHeight_ = decodeTransferDimension(
        static_cast<std::uint16_t>((packet_[2] >> 16) & 0x1FFu), Vram::kHeight);
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
            gp0Fifo_.clear();
            packet_.clear();
            expectedWords_ = 0;
            variablePolyline_ = false;
            transferMode_ = TransferMode::None;
            transferIndex_ = 0;
            readLatchValid_ = false;
            gpuInfoPending_ = false;
            break;
        case 0x02: irqPending_ = false; break;
        case 0x03: displayDisabled_ = (data & 1u) != 0; break;
        case 0x04: dmaDirection_ = data & 3u; break;
        case 0x05: displayStart_ = data & 0x0007FFFFu; break;
        case 0x06: horizontalRange_ = data; break;
        case 0x07: verticalRange_ = data; break;
        case 0x08: displayMode_ = data & 0xFFu; break;
        case 0x10: {
            const std::uint32_t selector = data & 0x0Fu;
            switch (selector) {
                case 0x02: gpuInfoLatch_ = textureWindow_; break;
                case 0x03: gpuInfoLatch_ = drawAreaTopLeft_; break;
                case 0x04: gpuInfoLatch_ = drawAreaBottomRight_; break;
                case 0x05: gpuInfoLatch_ = drawOffset_; break;
                case 0x07: gpuInfoLatch_ = 0x00000002u; break; // PS1 GPU type.
                default: gpuInfoLatch_ = 0u; break;
            }
            gpuInfoPending_ = true;
            break;
        }
        default: break;
    }
}

std::uint32_t Gpu::readStatus() const noexcept {
    std::uint32_t status = 0;

    // Drawing environment and display-mode fields.
    status |= drawMode_ & 0x7FFu;                 // bits 0-10
    status |= (maskSetting_ & 3u) << 11;          // bits 11-12
    status |= 1u << 13;                           // non-interlaced field default
    if (displayMode_ & (1u << 7)) status |= 1u << 14;
    status |= (drawMode_ & (1u << 11)) << 4;      // texture disable -> bit 15
    // GPUSTAT display fields: horizontal resolution is split across bits 16-18.
    status |= ((displayMode_ >> 6) & 1u) << 16;     // horizontal resolution 2
    status |= (displayMode_ & 3u) << 17;            // horizontal resolution 1
    status |= ((displayMode_ >> 2) & 1u) << 19;     // vertical resolution
    status |= ((displayMode_ >> 3) & 1u) << 20;     // video mode
    status |= ((displayMode_ >> 4) & 1u) << 21;     // display depth
    status |= ((displayMode_ >> 5) & 1u) << 22;     // interlace mode
    if (displayDisabled_) status |= 1u << 23;
    if (irqPending_) status |= 1u << 24;

    // GP0 readiness reflects queued words and packet/transfer state.
    const bool fifoHasSpace = !gp0Fifo_.full();
    const bool fifoEmpty = gp0Fifo_.empty();
    const bool idleForCommand = fifoEmpty &&
        packet_.empty() && transferMode_ == TransferMode::None;
    const bool readyToSendVram = transferMode_ == TransferMode::VramToCpu;
    const bool readyForDmaBlock = idleForCommand;

    if (dmaDirection_ == 1u && fifoHasSpace) {
        status |= 1u << 25; // FIFO mode: request while GP0 FIFO has space.
    } else if (dmaDirection_ == 2u && readyForDmaBlock) {
        status |= 1u << 25;
    } else if (dmaDirection_ == 3u && readyToSendVram) {
        status |= 1u << 25;
    }

    if (fifoHasSpace && transferMode_ == TransferMode::None)
        status |= 1u << 26;
    if (readyToSendVram) status |= 1u << 27;
    if (readyForDmaBlock) status |= 1u << 28;
    status |= (dmaDirection_ & 3u) << 29;
    if (oddField_) status |= 1u << 31;
    return status;
}

} // namespace nexus::ps1
