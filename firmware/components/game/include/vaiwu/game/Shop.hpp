#pragma once

#include "vaiwu/game/I18n.hpp"
#include "vaiwu/game/VaiwuPet.hpp"

#include <cstdint>

namespace vaiwu::game {

enum class ShopItem : std::uint8_t {
    TastyFood = 0, // big hunger + happiness treat
    Cake,          // lots of happiness, some hunger + weight
    Toy,           // happiness only
    EnergyDrink,   // restores energy
    Medicine,      // tops up hunger to clear low-hunger sickness
    Ribbon,        // cosmetic gift: pure happiness
    Sushi,         // premium meal: big hunger + happiness
    Vitamins,      // cheap remedy: cures + small happiness
    SuperEnergy,   // full energy refill
    HpPotion,      // battle supply: goes into the RPG bag, not instant
    // Equipment (maps 1:1 to vaiwu::game::EquipItem via ShopItemInfo::equip):
    WoodenSword, SteelSword, MagicStaff, WindDagger,
    LeatherVest, IronArmor, LuckyCharm, StarAmulet,
    MoonBlade, ScaleMail, SilkCloak, SageRing,
    MpPotion,      // mana refill carried into battle
    Count_,
};

constexpr int kShopItemCount = static_cast<int>(ShopItem::Count_);

struct ShopItemInfo {
    std::uint32_t price;   // in coins
    Str nameId;            // localized display name
    float hunger;          // effect deltas passed to VaiwuPet::applyGift
    float happiness;
    float energy;
    float weightDelta;
    bool cures;            // also cure illness on purchase (medicines)
    bool potion;           // stored in the RPG bag (rpg().potions) instead
                           // of applying instantly
    int equip;             // EquipItem index (-1 = not equipment); owning is
                           // one-time: buyShopItem refuses a second purchase
    bool mpPotion;         // same as potion, but fills the mana bag
};

[[nodiscard]] const ShopItemInfo& shopItem(ShopItem item);

// Shop categories: the shop UI shows one category per page (like the
// character-select paging). Every ShopItem belongs to exactly one category.
enum class ShopCategory : std::uint8_t {
    Food = 0,     // meals & treats (also reachable from the FEED menu)
    Medicine,     // cures & energy boosters
    Gifts,        // toys & cosmetics: pure happiness
    Weapons,      // RPG equipment: weapon slot
    Gear,         // RPG equipment: armor slot
    Trinkets,     // RPG equipment: amulets and rings
    Count_,
};

constexpr int kShopCategoryCount = static_cast<int>(ShopCategory::Count_);

// Localized category title for the shop page header.
[[nodiscard]] Str shopCategoryName(ShopCategory category);

// Items of one category: returns a pointer to a static list and stores its
// length in outCount (never more than kShopItemCount).
[[nodiscard]] const ShopItem* shopCategoryItems(ShopCategory category, int& outCount);

// Attempt a purchase and apply the item's effect immediately (the ESP
// firmware shop behaves this way). Returns false if too poor or dead.
[[nodiscard]] bool buyShopItem(VaiwuPet& pet, ShopItem item);

// App-style purchase: equipment is granted once and potions go into the RPG
// bag, but every other item goes into the carried-consumables bag instead of
// being applied on the spot (useShopItem applies it later, so buying an energy
// drink at full energy does not waste it). Returns false if too poor, dead,
// the equipment is already owned, or the bag slot is full.
[[nodiscard]] bool buyShopItemToBag(VaiwuPet& pet, ShopItem item);

// Outcome of using a carried item (also used for the feed menu's
// buy-and-use shortcut).
enum class UseResult : std::uint8_t {
    Ok = 0,
    NotOwned,       // nothing in the bag (buy it first)
    NotApplicable,  // the effect would be wasted (e.g. full energy)
    NoCoins,        // buy-and-use: not enough gold
    RefusedDead,
};

// True when the item is in the bag and using it right now would actually
// change something: the main effect is not already at its cap, or the item
// cures an active illness. Side bonuses smaller than 10 points are ignored,
// so an energy drink at full energy is "not applicable" even though it also
// gives +5 happiness.
[[nodiscard]] bool canUseShopItem(const VaiwuPet& pet, ShopItem item);

// Consume one item from the bag and apply its effect. Returns NotOwned when
// the bag is empty and NotApplicable when the effect would be wasted.
[[nodiscard]] UseResult useShopItem(VaiwuPet& pet, ShopItem item);

// Feed menu shortcut: spend coins and apply the effect immediately without
// touching the bag. Nothing is spent when the effect would be wasted.
[[nodiscard]] UseResult buyAndUseShopItem(VaiwuPet& pet, ShopItem item);

// Sells an owned equipment item back for half its price (see
// equipSellPrice). Returns the gold paid, or 0 if the pet does not own it.
[[nodiscard]] std::uint32_t sellEquip(VaiwuPet& pet, EquipItem item);

} // namespace vaiwu::game
