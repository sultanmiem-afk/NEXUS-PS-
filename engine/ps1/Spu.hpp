#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace nexus::ps1 {

// SPU sound RAM, ADPCM voices, ADSR, approximate stereo mixer and sample output.
class Spu final {
public:
    static constexpr std::size_t kRamSize = 512u * 1024u;
    static constexpr std::size_t kVoiceCount = 24u;

    enum class AdsrPhase : std::uint8_t { Off, Attack, Decay, Sustain, Release };

    struct StereoSample { std::int16_t left = 0; std::int16_t right = 0; };

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
        bool adpcmBlockValid = false;
        bool adpcmEnd = false;
        bool adpcmRepeat = false;
        std::uint16_t currentAddress = 0;
        std::uint8_t sampleIndex = 0;
        AdsrPhase adsrPhase = AdsrPhase::Off;
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
    bool decodeAdpcmBlock(std::uint32_t ramAddress, std::array<std::int16_t, 28>& samples,
                          std::int32_t& history1, std::int32_t& history2,
                          bool& endFlag, bool& repeatFlag) const noexcept;
    bool voiceSamples(std::size_t index, std::array<std::int16_t, 28>& samples) const noexcept;
    // Returns the next decoded sample; false means the voice has stopped or is invalid.
    bool nextVoiceSample(std::size_t index, std::int16_t& sample) noexcept;
    // Advance one voice envelope by output-sample ticks; each voice owns its own phase/counter.
    bool advanceAdsr(std::size_t index, std::uint32_t sampleTicks = 1u) noexcept;

    // Mix one output frame from all active voices. This is a simplified model;
    // pitch modulation, hardware volume sweeps and the full reverb unit are not modeled.
    StereoSample mixSample() noexcept;
    bool popOutputSample(StereoSample& sample) noexcept;
    std::size_t queuedOutputSamples() const noexcept;

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
    std::array<std::array<std::int16_t, 28>, kVoiceCount> voiceSamples_{};
    std::array<std::int32_t, kVoiceCount> adpcmHistory1_{};
    std::array<std::int32_t, kVoiceCount> adpcmHistory2_{};
    std::array<std::uint32_t, kVoiceCount> adsrRateCounters_{};
    static constexpr std::size_t kOutputQueueSize = 4096u;
    std::array<StereoSample, kOutputQueueSize> outputQueue_{};
    std::size_t outputRead_ = 0;
    std::size_t outputWrite_ = 0;
    std::size_t outputCount_ = 0;
    std::array<std::int32_t, 512u> reverbDelayLeft_{};
    std::array<std::int32_t, 512u> reverbDelayRight_{};
    std::size_t reverbPosition_ = 0;
    std::uint32_t keyOnMask_ = 0;
    std::uint32_t keyOffMask_ = 0;

    static bool isMappedRegister(std::uint32_t offset) noexcept;
    void applyKeyMask(std::uint32_t mask, bool keyOn) noexcept;
    bool decodeVoiceBlock(std::size_t index) noexcept;
    void updateAdsrPhase(std::size_t index) noexcept;
    static std::int16_t clampSample(std::int64_t value) noexcept;
};

} // namespace nexus::ps1
