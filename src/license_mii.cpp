#include <mkw/license_identity.hpp>
#include <cstdio>
#include <cstring>

namespace mkw {
namespace {
std::uint32_t checksum(const NativeMii& m)
{
    std::uint32_t crc = ~0u;
    for (auto c : m) {
        crc ^= c;
        for (unsigned bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1)));
    }
    return ~crc;
}
void path(char* out, std::size_t n, const char* folder, const MiiId& id)
{
    static constexpr char hex[] = "0123456789abcdef";
    char name[17];
    for (unsigned i = 0; i < 8; ++i) { name[2 * i] = hex[id[i] >> 4]; name[2 * i + 1] = hex[id[i] & 15]; }
    name[16] = 0;
    std::snprintf(out, n, "%s/mii-%s.3dsmii", folder, name);
}
constexpr std::uint8_t Magic[8] = {'M', 'K', 'W', 'M', 'I', 'I', 1, 0};
}

MiiId licenseMiiId(const NativeMii& m)
{
    // The 3DS Mii ID (big-endian bytes 0x0C..0x0F): bit 31 special, 28 bits of
    // two-second ticks since 2010. Wii IDs count four-second ticks since 2006.
    const std::uint32_t native = std::uint32_t(m[12]) << 24 | std::uint32_t(m[13]) << 16 | std::uint32_t(m[14]) << 8 | m[15];
    const std::uint32_t ticks = 31557600u + ((native & 0x0FFFFFFFu) >> 1);
    const std::uint32_t legacy = (native & 0x80000000u ? 0x80000000u : 0x40000000u) | ticks;
    return {std::uint8_t(legacy >> 24), std::uint8_t(legacy >> 16), std::uint8_t(legacy >> 8), std::uint8_t(legacy),
            m[4], m[5], m[6], m[7]};
}

bool loadNativeMii(const char* folder, const MiiId& id, NativeMii& out)
{
    char p[160];
    path(p, sizeof p, folder, id);
    std::FILE* f = std::fopen(p, "rb");
    if (!f) return false;
    std::uint8_t header[8], stored[4];
    NativeMii candidate{};
    bool ok = std::fread(header, 1, 8, f) == 8 && std::fread(candidate.data(), 1, candidate.size(), f) == candidate.size() &&
              std::fread(stored, 1, 4, f) == 4 && std::fgetc(f) == EOF;
    std::fclose(f);
    const std::uint32_t crc = checksum(candidate);
    ok = ok && !std::memcmp(header, Magic, 8) && candidate[0] == 3 && licenseMiiId(candidate) == id &&
         stored[0] == std::uint8_t(crc >> 24) && stored[1] == std::uint8_t(crc >> 16) &&
         stored[2] == std::uint8_t(crc >> 8) && stored[3] == std::uint8_t(crc);
    if (ok) out = candidate;
    return ok;
}

bool writeNativeMii(const char* folder, const NativeMii& m)
{
    if (m[0] != 3) return false;
    const MiiId id = licenseMiiId(m);
    NativeMii previous{};
    if (loadNativeMii(folder, id, previous) && previous == m) return true;
    char p[160], temporary[168];
    path(p, sizeof p, folder, id);
    std::snprintf(temporary, sizeof temporary, "%s.tmp", p);
    std::FILE* f = std::fopen(temporary, "wb");
    if (!f) return false;
    const std::uint32_t crc = checksum(m);
    const std::uint8_t stored[4] = {std::uint8_t(crc >> 24), std::uint8_t(crc >> 16), std::uint8_t(crc >> 8), std::uint8_t(crc)};
    bool ok = std::fwrite(Magic, 1, 8, f) == 8 && std::fwrite(m.data(), 1, m.size(), f) == m.size() && std::fwrite(stored, 1, 4, f) == 4;
    if (std::fflush(f)) ok = false;
    if (std::fclose(f)) ok = false;
    std::remove(p);
    if (!ok || std::rename(temporary, p)) { std::remove(temporary); return false; }
    return true;
}
}
