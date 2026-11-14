#pragma once
// The 3DS system Mii picker (libctru Mii selector applet) in place of retail's
// MiiSelectPage (page 0x60: "Wii Console" and "Guest" tabs). It takes both
// screens until the player picks a Mii or cancels.
#include <mkw/license_identity.hpp>
namespace mkw {
struct MiiPick {
    bool picked = false, guest = false;
    MiiName name{};
    MiiId id{};
    NativeMii mii{};
};
// title: UTF-8 prompt shown by the applet (retail Menu.bmg 0x84B).
MiiPick pickMii(const char* title);
}
