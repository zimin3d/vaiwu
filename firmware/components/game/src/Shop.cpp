#include "vaiwu/game/Rpg.hpp"
#include "vaiwu/game/Shop.hpp"

namespace vaiwu::game {

namespace {
constexpr ShopItemInfo kItems[kShopItemCount] = {
    // price  nameId                 hunger happy energy weight  cures
    {5, Str::ShopTastyFood, 40.0f, 10.0f, 0.0f, 2.0f, false, false, -1, false},
    {8, Str::ShopCake, 15.0f, 25.0f, 0.0f, 3.0f, false, false, -1, false},
    {6, Str::ShopToy, 0.0f, 25.0f, 0.0f, 0.0f, false, false, -1, false},
    {5, Str::ShopEnergyDrink, 0.0f, 5.0f, 45.0f, 0.0f, false, false, -1, false},
    {7, Str::ShopMedicine, 20.0f, 8.0f, 10.0f, -2.0f, true, false, -1, false},
    {10, Str::ShopRibbon, 0.0f, 35.0f, 0.0f, 0.0f, false, false, -1, false},
    {12, Str::ShopSushi, 50.0f, 20.0f, 0.0f, 2.0f, false, false, -1, false},
    {4, Str::ShopVitamins, 5.0f, 12.0f, 5.0f, 0.0f, true, false, -1, false},
    {8, Str::ShopSuperEnergy, 0.0f, 8.0f, 100.0f, 0.0f, false, false, -1, false},
    {6, Str::ShopHpPotion, 0.0f, 0.0f, 0.0f, 0.0f, false, true, -1, false},
    // Equipment (price, name; effects come from EquipInfo::bonus instead).
    {8, Str::EqWoodenSword, 0.0f, 0.0f, 0.0f, 0.0f, false, false, 0, false},
    {22, Str::EqSteelSword, 0.0f, 0.0f, 0.0f, 0.0f, false, false, 1, false},
    {18, Str::EqMagicStaff, 0.0f, 0.0f, 0.0f, 0.0f, false, false, 2, false},
    {16, Str::EqWindDagger, 0.0f, 0.0f, 0.0f, 0.0f, false, false, 3, false},
    {10, Str::EqLeatherVest, 0.0f, 0.0f, 0.0f, 0.0f, false, false, 4, false},
    {24, Str::EqIronArmor, 0.0f, 0.0f, 0.0f, 0.0f, false, false, 5, false},
    {14, Str::EqLuckyCharm, 0.0f, 0.0f, 0.0f, 0.0f, false, false, 6, false},
    {26, Str::EqStarAmulet, 0.0f, 0.0f, 0.0f, 0.0f, false, false, 7, false},
    {30, Str::EqMoonBlade, 0.0f, 0.0f, 0.0f, 0.0f, false, false, 8, false},
    {28, Str::EqScaleMail, 0.0f, 0.0f, 0.0f, 0.0f, false, false, 9, false},
    {20, Str::EqSilkCloak, 0.0f, 0.0f, 0.0f, 0.0f, false, false, 10, false},
    {24, Str::EqSageRing, 0.0f, 0.0f, 0.0f, 0.0f, false, false, 11, false},
    {6, Str::ShopMpPotion, 0.0f, 0.0f, 0.0f, 0.0f, false, false, -1, true},
};
constexpr ShopItem kFoodItems[] = {ShopItem::TastyFood, ShopItem::Cake, ShopItem::Sushi};
constexpr ShopItem kMedicineItems[] = {ShopItem::Medicine, ShopItem::Vitamins,
    ShopItem::EnergyDrink, ShopItem::HpPotion, ShopItem::MpPotion};
constexpr ShopItem kGiftItems[] = {ShopItem::Toy, ShopItem::Ribbon, ShopItem::SuperEnergy};
constexpr ShopItem kWeaponItems[] = {ShopItem::WoodenSword, ShopItem::SteelSword,
    ShopItem::MagicStaff, ShopItem::WindDagger, ShopItem::MoonBlade};
constexpr ShopItem kGearItems[] = {ShopItem::LeatherVest, ShopItem::IronArmor,
    ShopItem::ScaleMail, ShopItem::SilkCloak};
constexpr ShopItem kTrinketItems[] = {ShopItem::LuckyCharm, ShopItem::StarAmulet,
    ShopItem::SageRing};
} // namespace

Str shopCategoryName(ShopCategory category)
{
    switch (category) {
    case ShopCategory::Medicine: return Str::CatMedicine;
    case ShopCategory::Gifts: return Str::CatGifts;
    case ShopCategory::Weapons: return Str::CatWeapons;
    case ShopCategory::Gear: return Str::CatGear;
    case ShopCategory::Trinkets: return Str::CatTrinkets;
    case ShopCategory::Food:
    default: return Str::CatFood;
    }
}

const ShopItem* shopCategoryItems(ShopCategory category, int& outCount)
{
    switch (category) {
    case ShopCategory::Medicine:
        outCount = static_cast<int>(sizeof(kMedicineItems) / sizeof(kMedicineItems[0]));
        return kMedicineItems;
    case ShopCategory::Gifts:
        outCount = static_cast<int>(sizeof(kGiftItems) / sizeof(kGiftItems[0]));
        return kGiftItems;
    case ShopCategory::Weapons:
        outCount = static_cast<int>(sizeof(kWeaponItems) / sizeof(kWeaponItems[0]));
        return kWeaponItems;
    case ShopCategory::Gear:
        outCount = static_cast<int>(sizeof(kGearItems) / sizeof(kGearItems[0]));
        return kGearItems;
    case ShopCategory::Trinkets:
        outCount = static_cast<int>(sizeof(kTrinketItems) / sizeof(kTrinketItems[0]));
        return kTrinketItems;
    case ShopCategory::Food:
    default:
        outCount = static_cast<int>(sizeof(kFoodItems) / sizeof(kFoodItems[0]));
        return kFoodItems;
    }
}

const ShopItemInfo& shopItem(ShopItem item)
{
    const auto index = static_cast<unsigned>(item);
    return kItems[index < kShopItemCount ? index : 0U];
}

std::uint32_t sellEquip(VaiwuPet& pet, EquipItem item)
{
    RpgProgress& p = pet.rpg();
    if (!ownsEquip(p, item)) {
        return 0U;
    }
    // Find the shop price of this equipment item.
    int price = 0;
    for (int i = 0; i < kShopItemCount; ++i) {
        const ShopItemInfo& info = shopItem(static_cast<ShopItem>(i));
        if (info.equip == static_cast<int>(item)) {
            price = static_cast<int>(info.price);
            break;
        }
    }
    const auto gold = static_cast<std::uint32_t>(equipSellPrice(price));
    p.equipOwned &= ~(1U << static_cast<unsigned>(item));
    const int slot = static_cast<int>(equipInfo(item).slot);
    if (p.equipped[slot] == static_cast<int>(item)) {
        p.equipped[slot] = -1; // it was worn: take it off
    }
    normalizeRpg(pet.character(), p);
    pet.addCoins(gold);
    return gold;
}

bool buyShopItem(VaiwuPet& pet, ShopItem item)
{
    if (!pet.alive()) {
        return false;
    }
    const ShopItemInfo& info = shopItem(item);
    // Equipment is owned once; refuse a duplicate BEFORE spending anything
    // (the shop UI shows it as owned).
    if (info.equip >= 0 && ownsEquip(pet.rpg(), static_cast<EquipItem>(info.equip))) {
        return false;
    }
    if (!pet.spendCoins(info.price)) {
        return false;
    }
    if (info.equip >= 0) {
        grantEquip(pet.rpg(), static_cast<EquipItem>(info.equip));
        normalizeRpg(pet.character(), pet.rpg()); // gear may raise HP/MP maxima
        return true;
    }
    if (info.potion) {
        // Battle supplies are carried, not consumed on the spot.
        pet.rpg().potions = pet.rpg().potions >= 99 ? 99 : pet.rpg().potions + 1;
        return true;
    }
    if (info.mpPotion) {
        pet.rpg().mpPotions = pet.rpg().mpPotions >= 99 ? 99 : pet.rpg().mpPotions + 1;
        return true;
    }
    pet.applyGift(info.hunger, info.happiness, info.energy, info.weightDelta);
    if (info.cures) {
        pet.cure();
    }
    return true;
}

} // namespace vaiwu::game
