#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace nexus::ps1 {

class Cdrom final {
public:
    static constexpr std::uint32_t kBaseAddress = 0x1F801800u;
    static constexpr std::uint32_t kCyclesPerSector = 451584u;

    void reset() noexcept;
    bool loadImage(const std::vector<std::uint8_t>& image) noexcept;
    // Parses a single-BIN CUE sheet and binds its FILE entry to the supplied
    // filename and bytes. Multi-file CUE sheets are rejected explicitly.
    bool loadCue(const std::string& cueText, const std::string& binFilename,
                 const std::vector<std::uint8_t>& binImage) noexcept;
    bool imageLoaded() const noexcept { return !image_.empty() && !tracks_.empty(); }
    std::size_t sectorCount() const noexcept;
    std::size_t trackCount() const noexcept { return tracks_.size(); }
    bool cueLoaded() const noexcept { return cueLoaded_; }
    bool irqPending() const noexcept;

    void advance(std::uint32_t cpuCycles) noexcept;
    bool read8(std::uint32_t address, std::uint8_t& value) noexcept;
    bool write8(std::uint32_t address, std::uint8_t value) noexcept;

    std::uint8_t interruptFlags() const noexcept { return interruptFlags_; }
    std::uint8_t interruptEnable() const noexcept { return interruptEnable_; }
    std::uint32_t currentLba() const noexcept { return currentLba_; }
    bool reading() const noexcept { return reading_; }

private:
    struct Track {
        std::uint32_t discStartLba = 0;
        std::uint32_t fileStartSector = 0;
        std::uint32_t fileEndSector = 0;
        std::size_t sectorSize = 0;
        std::size_t dataOffset = 0;
        std::size_t userDataSize = 2048;
    };

    std::vector<std::uint8_t> image_{};
    std::vector<Track> tracks_{};
    bool cueLoaded_ = false;
    std::uint8_t index_ = 0;
    std::uint8_t interruptEnable_ = 0;
    std::uint8_t interruptFlags_ = 0;
    std::uint8_t mode_ = 0;
    std::uint8_t request_ = 0;
    std::uint32_t currentLba_ = 0;
    std::uint32_t cycleRemainder_ = 0;
    bool reading_ = false;
    bool dataReady_ = false;
    std::deque<std::uint8_t> parameters_{};
    std::deque<std::uint8_t> responses_{};
    std::deque<std::uint8_t> data_{};

    void executeCommand(std::uint8_t command) noexcept;
    void pushResponse(std::uint8_t value) noexcept;
    void raiseInterrupt(std::uint8_t code) noexcept;
    void produceSector() noexcept;
    bool readSector(std::uint32_t lba, std::vector<std::uint8_t>& output) const;
    static std::uint8_t fromBcd(std::uint8_t value) noexcept;
};

} // namespace nexus::ps1
