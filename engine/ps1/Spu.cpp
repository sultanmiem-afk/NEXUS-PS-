#include "Spu.hpp"

#include <algorithm>

namespace {
constexpr std::uint16_t kAdsrMaximum = 0x7FFFu;

std::uint32_t ratePeriod(unsigned rate) noexcept {
    rate = std::min(rate, 127u);
    return 1u + ((127u - rate) / 16u);
}

std::uint32_t rateStep(unsigned rate) noexcept {
    return 1u + (std::min(rate, 127u) / 8u);
}
} // namespace

namespace nexus::ps1 {

void Spu::reset() noexcept {
    clearRam();
    registers_.fill(0);
    voices_.fill(VoiceState{});
    for (auto& samples : voiceSamples_) samples.fill(0);
    adpcmHistory1_.fill(0);
    adpcmHistory2_.fill(0);
    adsrRateCounters_.fill(0);
    outputQueue_.fill(StereoSample{});
    outputRead_ = outputWrite_ = outputCount_ = 0;
    reverbDelayLeft_.fill(0);
    reverbDelayRight_.fill(0);
    reverbPosition_ = 0;
    keyOnMask_ = 0;
    keyOffMask_ = 0;
}

void Spu::clearRam(std::uint8_t value) noexcept { ram_.fill(value); }

bool Spu::readRam8(std::uint32_t address, std::uint8_t& value) const noexcept {
    if (address >= kRamSize) return false;
    value = ram_[address];
    return true;
}

bool Spu::readRam16(std::uint32_t address, std::uint16_t& value) const noexcept {
    if ((address & 1u) != 0 || address > kRamSize - 2u) return false;
    value = static_cast<std::uint16_t>(ram_[address]) |
            (static_cast<std::uint16_t>(ram_[address + 1u]) << 8);
    return true;
}

bool Spu::readRam32(std::uint32_t address, std::uint32_t& value) const noexcept {
    if ((address & 3u) != 0 || address > kRamSize - 4u) return false;
    value = static_cast<std::uint32_t>(ram_[address]) |
            (static_cast<std::uint32_t>(ram_[address + 1u]) << 8) |
            (static_cast<std::uint32_t>(ram_[address + 2u]) << 16) |
            (static_cast<std::uint32_t>(ram_[address + 3u]) << 24);
    return true;
}

bool Spu::writeRam8(std::uint32_t address, std::uint8_t value) noexcept {
    if (address >= kRamSize) return false;
    ram_[address] = value;
    return true;
}

bool Spu::writeRam16(std::uint32_t address, std::uint16_t value) noexcept {
    if ((address & 1u) != 0 || address > kRamSize - 2u) return false;
    ram_[address] = static_cast<std::uint8_t>(value & 0xFFu);
    ram_[address + 1u] = static_cast<std::uint8_t>(value >> 8);
    return true;
}

bool Spu::writeRam32(std::uint32_t address, std::uint32_t value) noexcept {
    if ((address & 3u) != 0 || address > kRamSize - 4u) return false;
    ram_[address] = static_cast<std::uint8_t>(value & 0xFFu);
    ram_[address + 1u] = static_cast<std::uint8_t>((value >> 8) & 0xFFu);
    ram_[address + 2u] = static_cast<std::uint8_t>((value >> 16) & 0xFFu);
    ram_[address + 3u] = static_cast<std::uint8_t>(value >> 24);
    return true;
}

bool Spu::isMappedRegister(std::uint32_t offset) noexcept {
    if ((offset & 1u) != 0 || offset >= 0x200u) return false;
    if (offset < 0x180u) return true;
    return offset >= 0x180u && offset <= 0x1FEu;
}

bool Spu::voiceState(std::size_t index, VoiceState& value) const noexcept {
    if (index >= kVoiceCount) return false;
    value = voices_[index];
    return true;
}

void Spu::applyKeyMask(std::uint32_t mask, bool keyOn) noexcept {
    mask &= 0x00FFFFFFu;
    for (std::size_t i = 0; i < kVoiceCount; ++i) {
        if ((mask & (1u << i)) == 0) continue;
        if (keyOn) {
            voices_[i].keyedOn = true;
            voices_[i].currentAdsrVolume = 0;
            voices_[i].adsrPhase = AdsrPhase::Attack;
            adsrRateCounters_[i] = 0;
            voices_[i].currentAddress = voices_[i].startAddress;
            voices_[i].sampleIndex = 0;
            voices_[i].adpcmBlockValid = false;
            adpcmHistory1_[i] = 0;
            adpcmHistory2_[i] = 0;
            (void)decodeVoiceBlock(i);
        } else if (voices_[i].adsrPhase != AdsrPhase::Off) {
            // Key-off closes the gate; the envelope continues through Release.
            voices_[i].keyedOn = false;
            voices_[i].adsrPhase = AdsrPhase::Release;
            adsrRateCounters_[i] = 0;
        } else {
            voices_[i].keyedOn = false;
        }
    }
}

bool Spu::decodeAdpcmBlock(std::uint32_t ramAddress,
                             std::array<std::int16_t, 28>& samples,
                             std::int32_t& history1, std::int32_t& history2,
                             bool& endFlag, bool& repeatFlag) const noexcept {
    if ((ramAddress & 7u) != 0 || ramAddress > kRamSize - 16u) return false;

    const std::uint8_t header = ram_[ramAddress];
    const std::uint8_t flags = ram_[ramAddress + 1u];
    const unsigned shift = (header & 0x0Fu) > 12u ? 9u : (header & 0x0Fu);
    const unsigned filter = (header >> 4) & 0x0Fu;
    static constexpr std::int32_t kFilter[5][2] = {
        {0, 0}, {60, 0}, {115, -52}, {98, -55}, {122, -60}
    };
    if (filter >= 5u) return false;

    std::int32_t h1 = history1;
    std::int32_t h2 = history2;
    for (std::size_t i = 0; i < samples.size(); ++i) {
        const std::uint8_t packed = ram_[ramAddress + 2u + i / 2u];
        const unsigned rawNibble = (i & 1u) == 0 ? (packed & 0x0Fu) : (packed >> 4);
        const std::int32_t signedNibble = (rawNibble & 0x08u) != 0
            ? static_cast<std::int32_t>(rawNibble) - 16
            : static_cast<std::int32_t>(rawNibble);
        const std::int32_t residual = signedNibble * (1 << (12u - shift));
        const std::int32_t prediction =
            (h1 * kFilter[filter][0] + h2 * kFilter[filter][1] + 32) >> 6;
        std::int32_t decoded = residual + prediction;
        if (decoded > 32767) decoded = 32767;
        if (decoded < -32768) decoded = -32768;
        samples[i] = static_cast<std::int16_t>(decoded);
        h2 = h1;
        h1 = decoded;
    }

    history1 = h1;
    history2 = h2;
    endFlag = (flags & 0x01u) != 0;
    repeatFlag = (flags & 0x02u) != 0;
    return true;
}

bool Spu::decodeVoiceBlock(std::size_t index) noexcept {
    if (index >= kVoiceCount) return false;
    const std::uint32_t address = static_cast<std::uint32_t>(voices_[index].currentAddress) * 8u;
    bool end = false;
    bool repeat = false;
    if (!decodeAdpcmBlock(address, voiceSamples_[index], adpcmHistory1_[index],
                          adpcmHistory2_[index], end, repeat)) {
        voices_[index].adpcmBlockValid = false;
        return false;
    }
    voices_[index].adpcmBlockValid = true;
    voices_[index].adpcmEnd = end;
    voices_[index].adpcmRepeat = repeat;
    return true;
}

bool Spu::voiceSamples(std::size_t index,
                       std::array<std::int16_t, 28>& samples) const noexcept {
    if (index >= kVoiceCount || !voices_[index].adpcmBlockValid) return false;
    samples = voiceSamples_[index];
    return true;
}

bool Spu::nextVoiceSample(std::size_t index, std::int16_t& sample) noexcept {
    if (index >= kVoiceCount) return false;
    VoiceState& voice = voices_[index];
    if (voice.adsrPhase == AdsrPhase::Off || !voice.adpcmBlockValid) return false;

    sample = voiceSamples_[index][voice.sampleIndex];
    ++voice.sampleIndex;
    if (voice.sampleIndex < 28u) return true;

    voice.sampleIndex = 0;
    if (voice.adpcmEnd) {
        if (!voice.adpcmRepeat) {
            voice.keyedOn = false;
            voice.adsrPhase = AdsrPhase::Release;
            adsrRateCounters_[index] = 0;
            voice.adpcmBlockValid = false;
            return true; // The final sample was delivered; the envelope now releases.
        }
        voice.currentAddress = voice.repeatAddress;
    } else {
        // Each sound-RAM ADPCM block is 16 bytes = two 8-byte address units.
        if (voice.currentAddress > 0xFFFDu) {
            voice.keyedOn = false;
            voice.adsrPhase = AdsrPhase::Release;
            adsrRateCounters_[index] = 0;
            voice.adpcmBlockValid = false;
            return true;
        }
        voice.currentAddress = static_cast<std::uint16_t>(voice.currentAddress + 2u);
    }

    if (!decodeVoiceBlock(index)) {
        voice.keyedOn = false;
        voice.adsrPhase = AdsrPhase::Release;
        adsrRateCounters_[index] = 0;
        voice.adpcmBlockValid = false;
    }
    return true;
}

void Spu::updateAdsrPhase(std::size_t index) noexcept {
    VoiceState& voice = voices_[index];
    if (voice.currentAdsrVolume == 0u) {
        voice.adsrPhase = AdsrPhase::Off;
        voice.adpcmBlockValid = false;
    }
}

bool Spu::advanceAdsr(std::size_t index, std::uint32_t sampleTicks) noexcept {
    if (index >= kVoiceCount) return false;
    VoiceState& voice = voices_[index];
    if (voice.adsrPhase == AdsrPhase::Off) return false;

    for (std::uint32_t tick = 0; tick < sampleTicks; ++tick) {
        unsigned rate = 0;
        bool exponential = false;
        switch (voice.adsrPhase) {
            case AdsrPhase::Attack:
                rate = (voice.adsrLow >> 8u) & 0x7Fu;
                exponential = (voice.adsrLow & 0x8000u) != 0;
                break;
            case AdsrPhase::Decay:
                rate = ((voice.adsrLow >> 4u) & 0x0Fu) * 8u;
                exponential = true;
                break;
            case AdsrPhase::Sustain:
                rate = (voice.adsrHigh >> 6u) & 0x7Fu;
                exponential = (voice.adsrHigh & 0x8000u) != 0;
                break;
            case AdsrPhase::Release:
                rate = (voice.adsrHigh & 0x1Fu) * 4u;
                exponential = (voice.adsrHigh & 0x20u) != 0;
                break;
            case AdsrPhase::Off:
                return false;
        }

        const std::uint32_t period = ratePeriod(rate);
        if (++adsrRateCounters_[index] < period) continue;
        adsrRateCounters_[index] = 0;

        std::uint32_t step = rateStep(rate);
        if (exponential) {
            if (voice.adsrPhase == AdsrPhase::Attack &&
                voice.currentAdsrVolume >= 0x6000u) {
                step = std::max(1u, step / 4u);
            } else if (voice.adsrPhase != AdsrPhase::Attack) {
                step = std::max(1u, (static_cast<std::uint32_t>(voice.currentAdsrVolume) * step) >> 15u);
            }
        }

        std::uint32_t level = voice.currentAdsrVolume;
        switch (voice.adsrPhase) {
            case AdsrPhase::Attack:
                level = std::min<std::uint32_t>(kAdsrMaximum, level + step);
                voice.currentAdsrVolume = static_cast<std::uint16_t>(level);
                if (level == kAdsrMaximum) {
                    voice.adsrPhase = AdsrPhase::Decay;
                    adsrRateCounters_[index] = 0;
                }
                break;
            case AdsrPhase::Decay: {
                const std::uint32_t sustain = (((voice.adsrLow & 0x0Fu) + 1u) * 0x800u);
                level = level > step ? level - step : 0u;
                if (level <= sustain) {
                    level = sustain;
                    voice.adsrPhase = AdsrPhase::Sustain;
                    adsrRateCounters_[index] = 0;
                }
                voice.currentAdsrVolume = static_cast<std::uint16_t>(level);
                break;
            }
            case AdsrPhase::Sustain:
                if ((voice.adsrHigh & 0x4000u) != 0) {
                    level = level > step ? level - step : 0u;
                } else {
                    level = std::min<std::uint32_t>(kAdsrMaximum, level + step);
                }
                voice.currentAdsrVolume = static_cast<std::uint16_t>(level);
                if (level == 0u) updateAdsrPhase(index);
                break;
            case AdsrPhase::Release:
                level = level > step ? level - step : 0u;
                voice.currentAdsrVolume = static_cast<std::uint16_t>(level);
                if (level == 0u) {
                    voice.adsrPhase = AdsrPhase::Off;
                    voice.adpcmBlockValid = false;
                    adsrRateCounters_[index] = 0;
                }
                break;
            case AdsrPhase::Off:
                return false;
        }
    }
    return voice.adsrPhase != AdsrPhase::Off;
}

std::int16_t Spu::clampSample(std::int64_t value) noexcept {
    if (value > 32767) value = 32767;
    if (value < -32768) value = -32768;
    return static_cast<std::int16_t>(value);
}

Spu::StereoSample Spu::mixSample() noexcept {
    std::int64_t left = 0;
    std::int64_t right = 0;
    constexpr std::int64_t kFullScale = 32767;

    for (std::size_t i = 0; i < kVoiceCount; ++i) {
        VoiceState& voice = voices_[i];
        if (voice.adsrPhase == AdsrPhase::Off) continue;

        (void)advanceAdsr(i, 1u);
        if (voice.adsrPhase == AdsrPhase::Off) continue;

        std::int16_t decoded = 0;
        if (!nextVoiceSample(i, decoded)) continue;

        const std::int64_t envelope = voice.currentAdsrVolume;
        const std::int64_t voiceLeft = voice.volumeLeft & 0x7FFFu;
        const std::int64_t voiceRight = voice.volumeRight & 0x7FFFu;
        const std::int64_t enveloped = static_cast<std::int64_t>(decoded) * envelope / kFullScale;
        left += enveloped * voiceLeft / kFullScale;
        right += enveloped * voiceRight / kFullScale;
    }

    const std::int64_t masterLeft = registers_[0x180u / 2u] & 0x7FFFu;
    const std::int64_t masterRight = registers_[0x182u / 2u] & 0x7FFFu;
    left = left * masterLeft / kFullScale;
    right = right * masterRight / kFullScale;

    // Approximate feedback delay reverb. Bit 7 of the simplified SPU control
    // register enables a short stereo delay; this is not the PS1 reverb algorithm.
    if ((registers_[0x1A8u / 2u] & 0x0080u) != 0u) {
        const std::int64_t wetLeft = reverbDelayLeft_[reverbPosition_];
        const std::int64_t wetRight = reverbDelayRight_[reverbPosition_];
        left += wetLeft / 4;
        right += wetRight / 4;
        reverbDelayLeft_[reverbPosition_] = static_cast<std::int32_t>(
            std::clamp<std::int64_t>(left + wetLeft / 2, -65536, 65535));
        reverbDelayRight_[reverbPosition_] = static_cast<std::int32_t>(
            std::clamp<std::int64_t>(right + wetRight / 2, -65536, 65535));
        reverbPosition_ = (reverbPosition_ + 1u) % reverbDelayLeft_.size();
    } else {
        reverbDelayLeft_.fill(0);
        reverbDelayRight_.fill(0);
        reverbPosition_ = 0;
    }

    const StereoSample mixed{clampSample(left), clampSample(right)};
    if (outputCount_ == kOutputQueueSize) {
        outputRead_ = (outputRead_ + 1u) % kOutputQueueSize;
        --outputCount_;
    }
    outputQueue_[outputWrite_] = mixed;
    outputWrite_ = (outputWrite_ + 1u) % kOutputQueueSize;
    ++outputCount_;
    return mixed;
}

bool Spu::popOutputSample(StereoSample& sample) noexcept {
    if (outputCount_ == 0u) return false;
    sample = outputQueue_[outputRead_];
    outputRead_ = (outputRead_ + 1u) % kOutputQueueSize;
    --outputCount_;
    return true;
}

std::size_t Spu::queuedOutputSamples() const noexcept { return outputCount_; }

bool Spu::readRegister16(std::uint32_t offset, std::uint16_t& value) const noexcept {
    if (!isMappedRegister(offset)) return false;

    if (offset < 0x180u) {
        const std::size_t voiceIndex = offset / 0x10u;
        const std::uint32_t voiceOffset = offset & 0x0Fu;
        const VoiceState& v = voices_[voiceIndex];
        switch (voiceOffset) {
            case 0x0: value = v.volumeLeft; return true;
            case 0x2: value = v.volumeRight; return true;
            case 0x4: value = v.pitch; return true;
            case 0x6: value = v.startAddress; return true;
            case 0x8: value = v.adsrLow; return true;
            case 0xA: value = v.adsrHigh; return true;
            case 0xC: value = v.currentAdsrVolume; return true;
            case 0xE: value = v.repeatAddress; return true;
            default: return false;
        }
    }

    if (offset == 0x188u) { value = static_cast<std::uint16_t>(keyOnMask_); return true; }
    if (offset == 0x18Au) { value = static_cast<std::uint16_t>(keyOnMask_ >> 16); return true; }
    if (offset == 0x18Cu) { value = static_cast<std::uint16_t>(keyOffMask_); return true; }
    if (offset == 0x18Eu) { value = static_cast<std::uint16_t>(keyOffMask_ >> 16); return true; }

    value = registers_[offset / 2u];
    return true;
}

bool Spu::writeRegister16(std::uint32_t offset, std::uint16_t value) noexcept {
    if (!isMappedRegister(offset)) return false;

    if (offset < 0x180u) {
        const std::size_t voiceIndex = offset / 0x10u;
        const std::uint32_t voiceOffset = offset & 0x0Fu;
        VoiceState& v = voices_[voiceIndex];
        switch (voiceOffset) {
            case 0x0: v.volumeLeft = value; break;
            case 0x2: v.volumeRight = value; break;
            case 0x4: v.pitch = value; break;
            case 0x6: v.startAddress = value; break;
            case 0x8: v.adsrLow = value; break;
            case 0xA: v.adsrHigh = value; break;
            case 0xC: return false; // Current ADSR volume is read-only.
            case 0xE: v.repeatAddress = value; break;
            default: return false;
        }
        registers_[offset / 2u] = value;
        return true;
    }

    // Key-on and key-off are write-triggered command masks, not ordinary storage.
    if (offset == 0x188u || offset == 0x18Au) {
        const unsigned shift = offset == 0x18Au ? 16u : 0u;
        const std::uint32_t mask = static_cast<std::uint32_t>(value) << shift;
        keyOnMask_ = (keyOnMask_ & ~(0xFFFFu << shift)) | mask;
        applyKeyMask(mask, true);
        return true;
    }
    if (offset == 0x18Cu || offset == 0x18Eu) {
        const unsigned shift = offset == 0x18Eu ? 16u : 0u;
        const std::uint32_t mask = static_cast<std::uint32_t>(value) << shift;
        keyOffMask_ = (keyOffMask_ & ~(0xFFFFu << shift)) | mask;
        applyKeyMask(mask, false);
        return true;
    }

    // The SPU control register is at 0x1A8; transfer control is 0x1AA.
    registers_[offset / 2u] = value;
    return true;
}

} // namespace nexus::ps1
