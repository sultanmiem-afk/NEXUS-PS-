#include "Spu.hpp"

#include <array>
#include <cassert>
#include <cstdint>

using namespace nexus::ps1;

int main() {
    Spu spu;
    assert(spu.ramSize() == 512u * 1024u);

    // Sound RAM starts cleared and uses little-endian storage.
    std::uint8_t byte = 0xFFu;
    std::uint16_t half = 0xFFFFu;
    std::uint32_t word = 0xFFFFFFFFu;
    assert(spu.readRam8(0x100u, byte) && byte == 0u);
    assert(spu.writeRam32(0x100u, 0x12345678u));
    assert(spu.readRam8(0x100u, byte) && byte == 0x78u);
    assert(spu.readRam16(0x100u, half) && half == 0x5678u);
    assert(spu.readRam32(0x100u, word) && word == 0x12345678u);

    // Byte and halfword access can update portions of a word.
    assert(spu.writeRam8(0x101u, 0xABu));
    assert(spu.readRam32(0x100u, word) && word == 0x1234AB78u);
    assert(spu.writeRam16(0x102u, 0xCDEFu));
    assert(spu.readRam32(0x100u, word) && word == 0xCDEFAB78u);

    // The last valid addresses are accessible; out-of-range accesses fail.
    assert(spu.writeRam8(Spu::kRamSize - 1u, 0x5Au));
    assert(spu.readRam8(Spu::kRamSize - 1u, byte) && byte == 0x5Au);
    assert(spu.writeRam16(Spu::kRamSize - 2u, 0xBEEFu));
    assert(spu.readRam16(Spu::kRamSize - 2u, half) && half == 0xBEEFu);
    assert(spu.writeRam32(Spu::kRamSize - 4u, 0x89ABCDEFu));
    assert(spu.readRam32(Spu::kRamSize - 4u, word) && word == 0x89ABCDEFu);
    assert(!spu.readRam8(static_cast<std::uint32_t>(Spu::kRamSize), byte));
    assert(!spu.writeRam8(static_cast<std::uint32_t>(Spu::kRamSize), 0x12u));
    assert(!spu.readRam16(static_cast<std::uint32_t>(Spu::kRamSize - 1u), half));
    assert(!spu.writeRam16(static_cast<std::uint32_t>(Spu::kRamSize - 1u), 0x1234u));
    assert(!spu.readRam32(static_cast<std::uint32_t>(Spu::kRamSize - 2u), word));
    assert(!spu.writeRam32(static_cast<std::uint32_t>(Spu::kRamSize - 2u), 0x12345678u));

    // Multi-byte operations require natural alignment.
    assert(!spu.readRam16(0x101u, half));
    assert(!spu.writeRam16(0x101u, 0x1234u));
    assert(!spu.readRam32(0x102u, word));
    assert(!spu.writeRam32(0x102u, 0x12345678u));

    // Reset clears the entire sound RAM.
    spu.clearRam(0xA5u);
    assert(spu.readRam32(0x100u, word) && word == 0xA5A5A5A5u);
    spu.reset();
    assert(spu.readRam32(0x100u, word) && word == 0u);

    // Documented 16-bit SPU register bank (offsets from 0x1F801C00).
    std::uint16_t reg = 0xFFFFu;
    assert(spu.readRegister16(0x000u, reg) && reg == 0u);
    assert(spu.writeRegister16(0x000u, 0x1111u)); // Voice 0 VOLL
    assert(spu.writeRegister16(0x010u, 0x2222u)); // Voice 1 VOLL
    assert(spu.writeRegister16(0x170u, 0xABCDu)); // Voice 23 VOLL
    assert(spu.readRegister16(0x000u, reg) && reg == 0x1111u);
    assert(spu.readRegister16(0x010u, reg) && reg == 0x2222u);
    assert(spu.readRegister16(0x170u, reg) && reg == 0xABCDu);

    // Per-voice register behavior: volume, pitch, sample address and ADSR.
    assert(spu.writeRegister16(0x004u, 0x1000u)); // Voice 0 pitch
    assert(spu.writeRegister16(0x006u, 0x0200u)); // Voice 0 start address
    assert(spu.writeRegister16(0x008u, 0x8F0Fu)); // Voice 0 ADSR low
    assert(spu.writeRegister16(0x00Au, 0x7F20u)); // Voice 0 ADSR high
    assert(spu.writeRegister16(0x00Eu, 0x0240u)); // Voice 0 repeat address
    assert(spu.readRegister16(0x004u, reg) && reg == 0x1000u);
    assert(spu.readRegister16(0x006u, reg) && reg == 0x0200u);
    assert(spu.readRegister16(0x008u, reg) && reg == 0x8F0Fu);
    assert(spu.readRegister16(0x00Au, reg) && reg == 0x7F20u);
    assert(spu.readRegister16(0x00Eu, reg) && reg == 0x0240u);
    assert(!spu.writeRegister16(0x00Cu, 0x1234u)); // Current ADSR volume is read-only.

    // Global volume, voice masks, transfer/control and reverb slots.
    assert(spu.writeRegister16(0x180u, 0x1234u)); // Main volume left
    assert(spu.readRegister16(0x180u, reg) && reg == 0x1234u);
    assert(spu.writeRegister16(0x188u, 0x00F0u)); // Key-on low mask
    assert(spu.readRegister16(0x188u, reg) && reg == 0x00F0u);
    assert(spu.writeRegister16(0x1A8u, 0x0041u)); // SPU control
    assert(spu.readRegister16(0x1A8u, reg) && reg == 0x0041u);
    assert(spu.writeRegister16(0x1AAu, 0x0003u)); // Transfer control
    assert(spu.readRegister16(0x1AAu, reg) && reg == 0x0003u);
    assert(spu.writeRegister16(0x1C0u, 0x0100u)); // Reverb configuration
    assert(spu.readRegister16(0x1C0u, reg) && reg == 0x0100u);

    // Key-on/key-off masks change the corresponding voice state.
    Spu::VoiceState voice{};
    assert(spu.writeRegister16(0x188u, 0x0001u)); // Key on voice 0
    assert(spu.voiceState(0u, voice) && voice.keyedOn);
    assert(voice.currentAdsrVolume == 0u);
    assert(spu.readRegister16(0x00Cu, reg) && reg == 0u);
    assert(spu.writeRegister16(0x18Au, 0x0080u)); // Key on voice 23
    assert(spu.voiceState(23u, voice) && voice.keyedOn);
    assert(spu.writeRegister16(0x18Cu, 0x0001u)); // Key off voice 0
    assert(spu.voiceState(0u, voice) && !voice.keyedOn);
    assert(spu.keyOnMask() == 0x00800001u);
    assert(spu.keyOffMask() == 0x00000001u);
    assert(!spu.voiceState(24u, voice));

    // Reverb work-area start is a valid global SPU register.
    assert(spu.writeRegister16(0x1A0u, 0x3456u));
    assert(spu.readRegister16(0x1A0u, reg) && reg == 0x3456u);


    // PS1 ADPCM block decoding: 16-byte block -> 28 signed PCM samples.
    std::array<std::int16_t, 28> decoded{};
    std::int32_t history1 = 0;
    std::int32_t history2 = 0;
    bool endFlag = false;
    bool repeatFlag = false;
    assert(spu.writeRam8(0u, 0x0Cu)); // Shift 12, predictor filter 0.
    assert(spu.writeRam8(1u, 0x03u)); // End + repeat flags.
    assert(spu.writeRam8(2u, 0xF1u)); // First samples: +1, -1.
    for (std::uint32_t i = 3u; i < 16u; ++i) assert(spu.writeRam8(i, 0u));
    assert(spu.decodeAdpcmBlock(0u, decoded, history1, history2, endFlag, repeatFlag));
    assert(decoded[0] == 1 && decoded[1] == -1);
    assert(endFlag && repeatFlag);
    assert(history1 == 0 && history2 == 0);

    // Invalid predictor filters and unaligned/out-of-range blocks are rejected.
    assert(spu.writeRam8(16u, 0x5Cu)); // Filter 5 is reserved.
    assert(!spu.decodeAdpcmBlock(16u, decoded, history1, history2, endFlag, repeatFlag));
    assert(!spu.decodeAdpcmBlock(1u, decoded, history1, history2, endFlag, repeatFlag));
    assert(!spu.decodeAdpcmBlock(static_cast<std::uint32_t>(Spu::kRamSize - 8u),
                                 decoded, history1, history2, endFlag, repeatFlag));

    // Key-on loads and decodes the first ADPCM block from the voice start address.
    spu.reset();
    assert(spu.writeRam8(0u, 0x0Cu));
    assert(spu.writeRam8(1u, 0x01u));
    assert(spu.writeRam8(2u, 0x21u)); // First decoded samples: +1, +2.
    for (std::uint32_t i = 3u; i < 16u; ++i) assert(spu.writeRam8(i, 0u));
    assert(spu.writeRegister16(0x000u, 0x3FFFu)); // Voice 0 left volume
    assert(spu.writeRegister16(0x006u, 0x0000u)); // Start at sound RAM byte 0
    assert(spu.writeRegister16(0x188u, 0x0001u)); // Key on voice 0
    Spu::VoiceState decodedVoice{};
    assert(spu.voiceState(0u, decodedVoice));
    assert(decodedVoice.keyedOn && decodedVoice.adpcmBlockValid && decodedVoice.adpcmEnd);
    assert(spu.voiceSamples(0u, decoded));
    assert(decoded[0] == 1 && decoded[1] == 2);


    // Sequential playback advances to the next 16-byte block after 28 samples.
    spu.reset();
    assert(spu.writeRam8(0u, 0x0Cu));
    assert(spu.writeRam8(1u, 0x00u));
    for (std::uint32_t i = 2u; i < 16u; ++i) assert(spu.writeRam8(i, 0x11u));
    assert(spu.writeRam8(16u, 0x0Cu));
    assert(spu.writeRam8(17u, 0x01u)); // End on second block.
    for (std::uint32_t i = 18u; i < 32u; ++i) assert(spu.writeRam8(i, 0x22u));
    assert(spu.writeRegister16(0x006u, 0u));
    assert(spu.writeRegister16(0x188u, 1u));
    std::int16_t nextSample = 0;
    for (unsigned i = 0; i < 28u; ++i) {
        assert(spu.nextVoiceSample(0u, nextSample));
        assert(nextSample == 1);
    }
    assert(spu.voiceState(0u, decodedVoice) && decodedVoice.currentAddress == 2u);
    assert(spu.nextVoiceSample(0u, nextSample) && nextSample == 2);
    for (unsigned i = 1; i < 28u; ++i) assert(spu.nextVoiceSample(0u, nextSample));
    assert(spu.voiceState(0u, decodedVoice) && !decodedVoice.keyedOn);
    assert(!spu.nextVoiceSample(0u, nextSample));

    // End+repeat jumps to the configured repeat address.
    spu.reset();
    assert(spu.writeRam8(0u, 0x0Cu));
    assert(spu.writeRam8(1u, 0x03u));
    for (std::uint32_t i = 2u; i < 16u; ++i) assert(spu.writeRam8(i, 0x11u));
    assert(spu.writeRam8(32u, 0x0Cu));
    assert(spu.writeRam8(33u, 0x01u));
    for (std::uint32_t i = 34u; i < 48u; ++i) assert(spu.writeRam8(i, 0x33u));
    assert(spu.writeRegister16(0x00Eu, 4u)); // Repeat address in 8-byte units.
    assert(spu.writeRegister16(0x006u, 0u));
    assert(spu.writeRegister16(0x188u, 1u));
    for (unsigned i = 0; i < 28u; ++i) assert(spu.nextVoiceSample(0u, nextSample));
    assert(spu.voiceState(0u, decodedVoice) && decodedVoice.currentAddress == 4u);
    assert(spu.nextVoiceSample(0u, nextSample) && nextSample == 3);


    // Per-voice ADSR: Attack rises, Decay reaches sustain, Sustain holds/runs,
    // and Key-off enters a gradual Release instead of cutting the envelope.
    spu.reset();
    assert(spu.writeRegister16(0x008u, 0x7FF0u)); // Fast attack + fast decay, low sustain level.
    assert(spu.writeRegister16(0x00Au, 0x001Fu)); // Fast linear release.
    assert(spu.writeRegister16(0x188u, 0x0001u)); // Key-on voice 0.
    assert(spu.voiceState(0u, decodedVoice));
    assert(decodedVoice.adsrPhase == Spu::AdsrPhase::Attack);
    assert(decodedVoice.currentAdsrVolume == 0u);
    assert(spu.advanceAdsr(0u, 100u));
    assert(spu.voiceState(0u, decodedVoice));
    assert(decodedVoice.adsrPhase == Spu::AdsrPhase::Attack);
    assert(decodedVoice.currentAdsrVolume > 0u); // Attack is gradual.

    // A second channel has an independent envelope and has not advanced.
    assert(spu.writeRegister16(0x018u, 0x0010u)); // Voice 1 ADSR low, deliberately slow attack.
    assert(spu.writeRegister16(0x01Au, 0x001Fu));
    assert(spu.writeRegister16(0x188u, 0x0002u)); // Key-on voice 1.
    assert(spu.advanceAdsr(0u, 100000u));
    assert(spu.voiceState(0u, decodedVoice));
    assert(decodedVoice.adsrPhase == Spu::AdsrPhase::Sustain);
    assert(spu.voiceState(1u, voice));
    assert(voice.adsrPhase == Spu::AdsrPhase::Attack);
    assert(voice.currentAdsrVolume == 0u);

    const std::uint16_t beforeRelease = decodedVoice.currentAdsrVolume;
    assert(spu.writeRegister16(0x18Cu, 0x0001u)); // Key-off voice 0.
    assert(spu.voiceState(0u, decodedVoice));
    assert(!decodedVoice.keyedOn && decodedVoice.adsrPhase == Spu::AdsrPhase::Release);
    assert(spu.advanceAdsr(0u, 1u));
    assert(spu.voiceState(0u, decodedVoice));
    assert(decodedVoice.currentAdsrVolume < beforeRelease);
    assert(decodedVoice.currentAdsrVolume > 0u); // Release is gradual, not immediate.
    assert(spu.advanceAdsr(0u, 4096u) == false);
    assert(spu.voiceState(0u, decodedVoice));
    assert(decodedVoice.adsrPhase == Spu::AdsrPhase::Off);
    assert(decodedVoice.currentAdsrVolume == 0u);
    assert(spu.voiceState(1u, voice));
    assert(voice.adsrPhase == Spu::AdsrPhase::Attack); // Voice 1 was not affected.

    // Misaligned offsets and addresses outside the SPU map fail.
    assert(!spu.readRegister16(0x001u, reg));
    assert(!spu.writeRegister16(0x201u, 0x1234u));
    assert(!spu.readRegister16(0x200u, reg));
    assert(!spu.writeRegister16(0x200u, 0x1234u));

    // Reset clears register state as well as sound RAM.
    spu.reset();
    assert(spu.readRegister16(0x000u, reg) && reg == 0u);
    assert(spu.readRegister16(0x180u, reg) && reg == 0u);
    assert(spu.keyOnMask() == 0u && spu.keyOffMask() == 0u);
    assert(spu.voiceState(0u, voice) && !voice.keyedOn);

    return 0;
}
