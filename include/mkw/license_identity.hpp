#pragma once
// The Mii of a licence. Retail stores only the Mii's name (RKPD +0x14) and its
// 8-byte ID (+0x28) and looks the face up in the console's Mii data. The 3DS
// Mii (CFL data, 0x5C bytes, from the system Mii picker) is kept beside the
// save so a later Mii release can draw the face without a new pick.
// ID conversion and sidecar format: upstream MiniBandit/MKW-3DS
// (src/license_mii_store.cpp), unchanged so saves move between builds.
#include <mkw/save_store.hpp>
#include <array>
#include <cstdint>
namespace mkw {
using NativeMii = std::array<std::uint8_t, 0x5C>;
// Wii-style Mii ID of a 3DS Mii: 2006 epoch / four-second ticks from the 3DS
// 2010 epoch / two-second ticks, then the first four system-ID bytes.
MiiId licenseMiiId(const NativeMii&);
// sidecar <folder>/mii-<16 hex>.3dsmii: "MKWMII" 1 0, the 0x5C bytes, CRC-32 (BE).
bool loadNativeMii(const char* folder, const MiiId&, NativeMii&);
bool writeNativeMii(const char* folder, const NativeMii&);
}
