#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace nexus::ps1 {

// Early SPU model: sound RAM, voice register state, and key-on/key-off control.
// ADPCM decoding, ADSR envelope progression, mixing, and audio output are later stages.
class Spu final {
public:
    static constexpr std::size_t kRamSize = 512u * 1024u;
    static constexpr std::size_t kVoiceCount = 24u;

    struct VoiceState {
        std::uint16_t volumeLeft = 0;
        std::uint16_t volumeRight = 0;
        std::uint16_t pitch = 0;
        std::uint16_t startAddress = 0;
        std::uint16_t adsrLow = 0;
        std::uint16_t adsrHigh = 0;
        std::uint16_t currentAdsrVolume = 0;
        std::uint16_t repeatAddress = 0;
        bool keyedOn = false;
    };

    Spu() noexcept { reset(); }

    void reset() noexcept;
    void clearRam(std::uint8_t value = 0) noexcept;
    std::size_t ramSize() const noexcept { return ram_.size(); }

    // Byte offset relative to SPU register base 0x1F801C00.
    bool readRegister16(std::uint32_t offset, std::uint16_t& value) const noexcept;
    bool writeRegister16(std::uint32_t offset, std::uint16_t value) noexcept;

    bool voiceState(std::size_t index, VoiceState& value) const noexcept;
    std::uint32_t keyOnMask() const noexcept { return keyOnMask_; }
    std::uint32_t keyOffMask() const noexcept { return keyOffMask_; }

    bool readRam8(std::uint32_t address, std::uint8_t& value) const noexcept;
    bool readRam16(std::uint32_t address, std::uint16_t& value) const noexcept;
    bool readRam32(std::uint32_t address, std::uint32_t& value) const noexcept;
    bool writeRam8(std::uint32_t address, std::uint8_t value) noexcept;
    bool writeRam16(std::uint32_t address, std::uint16_t value) noexcept;
    bool writeRam32(std::uint32_t address, std::uint32_t value) noexcept;

private:
    static constexpr std::size_t kRegisterCount = 0x200u / 2u;
    std::array<std::uint8_t, kRamSize> ram_{};
    std::array<std::uint16_t, kRegisterCount> registers_{};
    std::array<VoiceState, kVoiceCount> voices_{};
    std::uint32_t keyOnMask_ = 0;
    std::uint32_t keyOffMask_ = 0;

    static bool isMappedRegister(std::uint32_t offset) noexcept;
    void applyKeyMask(std::uint32_t mask, bool keyOn) noexcept;
};

} // namespace nexus::ps1
