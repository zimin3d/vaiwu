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

// Attempt a purchase: if the pet has enough coins, spend them and apply the
// item's effect. Returns true on success, false if too poor (or dead).
[[nodiscard]] bool buyShopItem(VaiwuPet& pet, ShopItem item);

// Sells an owned equipment item back for half its price (see
// equipSellPrice). Returns the gold paid, or 0 if the pet does not own it.
[[nodiscard]] std::uint32_t sellEquip(VaiwuPet& pet, EquipItem item);

} // namespace vaiwu::game
