#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace nexus::ps1 {

// The retail PS1 BIOS image is 512 KiB. No copyrighted image is bundled;
// callers may load a BIOS image they are entitled to use.
class BiosRom {
public:
    static constexpr std::size_t kSize = 512u * 1024u;
    static constexpr std::uint32_t kBaseAddress = 0x1FC00000u;

    BiosRom() noexcept;

    constexpr std::size_t size() const noexcept { return kSize; }
    bool loaded() const noexcept { return loaded_; }
    bool load(const std::uint8_t* data, std::size_t length) noexcept;
    bool load(const std::vector<std::uint8_t>& data) noexcept {
        return load(data.data(), data.size());
    }
    void unload() noexcept;

    bool read8(std::uint32_t address, std::uint8_t& value) const noexcept;
    bool read16(std::uint32_t address, std::uint16_t& value) const noexcept;
    bool read32(std::uint32_t address, std::uint32_t& value) const noexcept;

private:
    std::array<std::uint8_t, kSize> bytes_{};
    bool loaded_ = false;
    bool translate(std::uint32_t address, std::size_t& offset) const noexcept;
};

} // namespace nexus::ps1
