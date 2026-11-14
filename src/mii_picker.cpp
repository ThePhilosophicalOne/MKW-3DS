#include <mkw/mii_picker.hpp>
#include <3ds.h>
#include <cstddef>
#include <cstring>

namespace mkw {
MiiPick pickMii(const char* title)
{
    MiiPick pick;
    MiiSelectorConf conf;
    miiSelectorInit(&conf);
    // Console Miis and guests (retail's two tabs); Cancel returns to the page.
    miiSelectorSetOptions(&conf, MIISELECTOR_CANCEL | MIISELECTOR_GUESTS);
    if (title) miiSelectorSetTitle(&conf, title);
    MiiSelectorReturn result;
    std::memset(&result, 0, sizeof result);
    miiSelectorLaunch(&conf, &result);
    if (result.no_mii_selected || !miiSelectorChecksumIsValid(&result)) return pick;
    static_assert(sizeof(MiiData) == sizeof(NativeMii));
    std::memcpy(pick.mii.data(), &result.mii, pick.mii.size());
    pick.guest = result.guest_mii_was_selected != 0;
    // MiiData is packed: copy its UTF-16 name out instead of pointing into it.
    u16 name[MIISELECTOR_GUESTMII_NAME_LEN] = {};
    std::memcpy(name, reinterpret_cast<const u8*>(&result.mii) + offsetof(MiiData, mii_name), 10 * sizeof(u16));
    if (pick.guest && !name[0]) std::memcpy(name, result.guest_mii_name, sizeof name);
    for (unsigned i = 0; i < pick.name.size() && name[i]; ++i) pick.name[i] = char16_t(name[i]);
    pick.id = licenseMiiId(pick.mii);
    pick.picked = true;
    return pick;
}
}
