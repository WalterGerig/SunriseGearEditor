#include "weapon_editor_panel.h"

#include "server/bap/runtime.h"

#include "core/filesystem/path.h"

#include "core/ui/toast/ui_toast_runtime.h"

#include "state/account/account_state.h"

#include "state/build_data/items/socket_plugs/socket_plug_catalog.h"

#include "state/build_data/runtime.h"

#include "state/runtime/runtime.h"

#include <algorithm>

#include <array>

#include <charconv>

#include <cctype>

#include <cfloat>

#include <cstddef>

#include <cstdint>

#include <cstdio>

#include <cstring>

#include <cmath>

#include <optional>

#include <span>

#include <string>

#include <string_view>

#include <vector>

#include <Windows.h>

#include <imgui.h>

#include "../../../../resources/resource.h"

#include "client/hooks/graphics/textures/graphics_texture_upload.h"

#ifdef interface

#undef interface

#endif

namespace sunrise::server::ui::weapon_editor {

namespace {

namespace inventory = state::account::inventory;

namespace item_details = state::build_data::items::details;

namespace socket_plugs = state::build_data::items::socket_plugs;

namespace gfx_textures = client::hooks::graphics::textures;

constexpr std::array<inventory::EquipmentSlot, 3> kWeaponSlots{

    inventory::EquipmentSlot::kinetic,

    inventory::EquipmentSlot::energy,

    inventory::EquipmentSlot::heavy,

};

constexpr std::array<const char*, 3> kWeaponSlotNames{"KINETIC", "ENERGY", "POWER"};

constexpr std::array<inventory::EquipmentSlot, 5> kArmorSlots{

    inventory::EquipmentSlot::helmet,

    inventory::EquipmentSlot::gauntlets,

    inventory::EquipmentSlot::chest,

    inventory::EquipmentSlot::legs,

    inventory::EquipmentSlot::classItem,

};

constexpr std::array<const char*, 5> kArmorSlotNames{

    "HELMET", "GAUNTLETS", "CHEST", "LEGS", "CLASS ITEM"

};

constexpr std::int8_t kNativeKineticSlot = 7;

constexpr std::int8_t kNativePowerSlot = 9;

constexpr std::size_t kMaximumVisibleWeaponsPerSlot = 10;

constexpr std::size_t kWeaponColumns = 5;

constexpr float kInventoryGap = 7.0F;

constexpr float kPlugCardHeight = 78.0F;

constexpr float kPlugIconSize = 50.0F;

constexpr float kWeaponCatalogCardHeight = 78.0F;

struct ResourceBytes {

    const std::byte* data{};

    std::size_t size{};

};

struct WeaponRow {

    inventory::Item item{};

    state::build_data::items::Definition definition{};

    item_details::Definition detail{};

    std::string_view name{};

    bool equipped{};

};

using PlugCategoryMask = std::uint16_t;

enum PlugCategory : PlugCategoryMask {

    kPlugCategoryNone = 0,

    kPlugCategoryWeaponPerk = 1U << 0U,

    kPlugCategoryIntrinsic = 1U << 1U,

    kPlugCategoryExoticIntrinsic = 1U << 2U,

    kPlugCategoryShader = 1U << 3U,

    kPlugCategoryOrnament = 1U << 4U,

    kPlugCategoryMod = 1U << 5U,

    kPlugCategoryCatalyst = 1U << 6U,

    kPlugCategoryMasterwork = 1U << 7U,

    kPlugCategoryTracker = 1U << 8U,

    kPlugCategoryArmorMod = 1U << 9U,
    kPlugCategoryArmorExoticPerk = 1U << 10U,

};

enum class WeaponRarity : std::uint8_t {

    unknown = 0,

    common = 1,

    uncommon = 2,

    rare = 3,

    legendary = 4,

    exotic = 5,

};

using WeaponRarityMask = std::uint8_t;

struct PlugChoice {

    state::build_data::items::Definition definition{};

    std::string_view name{};

    PlugCategoryMask categories{};

};

struct WeaponChoice {

    state::build_data::items::Definition definition{};

    item_details::Definition detail{};

    std::string_view name{};

    WeaponRarity rarity{WeaponRarity::unknown};

};

enum class PlugFilter : std::uint8_t {

    compatible,

    all,

};

enum class BrowserMode : std::uint8_t {

    plugs,

    weapons,

};

enum class EditorPage : std::uint8_t {

    weapons,

    armor,

};

enum class SocketKind : std::uint8_t {

    hidden,

    perk,

    ornament,

    mod,

    catalyst,

    shader,

    tracker,

    masterwork,

};

std::array<std::vector<WeaponRow>, 3> g_slotWeapons{};

std::array<std::size_t, 3> g_selectedWeaponBySlot{};

std::size_t g_activeSlot{};

std::array<std::vector<WeaponRow>, 5> g_slotArmor{};

std::array<std::size_t, 5> g_selectedArmorBySlot{};

std::size_t g_activeArmorSlot{};

EditorPage g_editorPage{EditorPage::weapons};

std::vector<PlugChoice> g_plugs{};

std::vector<std::size_t> g_visiblePlugs{};

std::size_t g_selectedPlug{};

std::uint8_t g_selectedLane{};

std::array<std::uint16_t, inventory::kPlugCapacity> g_pendingPlugDefinitions{};

std::array<bool, inventory::kPlugCapacity> g_pendingPlugLanes{};

std::uint64_t g_pendingInstanceSoid{};

PlugFilter g_filter{PlugFilter::compatible};

std::array<char, 64> g_search{};

PlugCategoryMask g_plugCategoryFilter{kPlugCategoryNone};

std::vector<PlugCategoryMask> g_plugCategoryByDefinition{};

std::size_t g_cachedPlugCategoryDefinitionCount{};

std::size_t g_cachedPlugCategoryRuleCount{};

std::vector<WeaponChoice> g_weaponCatalog{};

std::vector<std::size_t> g_visibleWeaponCatalog{};

std::size_t g_selectedReplacement{};

std::array<char, 64> g_weaponSearch{};

WeaponRarityMask g_weaponRarityFilter{};

BrowserMode g_browserMode{BrowserMode::plugs};

std::array<char, 192> g_message{};

std::uint16_t g_cachedItemDefinition{0xFFFFU};

std::uint8_t g_cachedLane{0xFFU};

PlugFilter g_cachedFilter{PlugFilter::compatible};

std::size_t g_cachedDefinitionCount{};

std::uint16_t g_cachedWeaponCatalogSource{0xFFFFU};

std::size_t g_cachedWeaponCatalogCount{};

ResourceBytes g_nameResource{};

bool g_nameResourceResolved{};

ResourceBytes g_rarityResource{};

bool g_rarityResourceResolved{};

constexpr std::array<std::byte, 8> kNameMagic{

    static_cast<std::byte>('S'), static_cast<std::byte>('W'),

    static_cast<std::byte>('N'), static_cast<std::byte>('A'),

    static_cast<std::byte>('M'), static_cast<std::byte>('E'),

    static_cast<std::byte>('0'), static_cast<std::byte>('1'),

};

constexpr std::size_t kNameHeaderSize = 12;

constexpr std::size_t kNameEntrySize = 12;

constexpr std::array<std::byte, 8> kRarityMagic{

    static_cast<std::byte>('S'), static_cast<std::byte>('W'),

    static_cast<std::byte>('R'), static_cast<std::byte>('A'),

    static_cast<std::byte>('R'), static_cast<std::byte>('I'),

    static_cast<std::byte>('T'), static_cast<std::byte>('1'),

};

constexpr std::size_t kRarityHeaderSize = 12;

constexpr std::size_t kRarityEntrySize = 8;

[[nodiscard]] constexpr float scaled(float value) noexcept {

    return value;

}

[[nodiscard]] ImU32 ui_color(ImU32 color) noexcept {

    ImVec4 value = ImGui::ColorConvertU32ToFloat4(color);

    value.w *= ImGui::GetStyle().Alpha;

    return ImGui::ColorConvertFloat4ToU32(value);

}

[[nodiscard]] HMODULE owning_module() noexcept {

    HMODULE module = nullptr;

    (void)GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS

                                 | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,

                             reinterpret_cast<LPCWSTR>(&owning_module),

                             &module);

    return module;

}

[[nodiscard]] bool resource_bytes(int identifier, ResourceBytes& output) noexcept {

    const HMODULE module = owning_module();

    const HRSRC resource = module != nullptr

                               ? FindResourceW(module, MAKEINTRESOURCEW(identifier), RT_RCDATA)

                               : nullptr;

    if (resource == nullptr) {

        return false;

    }

    const DWORD size = SizeofResource(module, resource);

    const HGLOBAL loaded = LoadResource(module, resource);

    const void* bytes = loaded != nullptr ? LockResource(loaded) : nullptr;

    if (size == 0 || bytes == nullptr) {

        return false;

    }

    output.data = static_cast<const std::byte*>(bytes);

    output.size = static_cast<std::size_t>(size);

    return true;

}

[[nodiscard]] std::uint32_t read_u32_le(const std::byte* bytes) noexcept {

    const auto* raw = reinterpret_cast<const unsigned char*>(bytes);

    return static_cast<std::uint32_t>(raw[0])

           | (static_cast<std::uint32_t>(raw[1]) << 8U)

           | (static_cast<std::uint32_t>(raw[2]) << 16U)

           | (static_cast<std::uint32_t>(raw[3]) << 24U);

}

[[nodiscard]] std::uint16_t read_u16_le(const std::byte* bytes) noexcept {

    const auto* raw = reinterpret_cast<const unsigned char*>(bytes);

    return static_cast<std::uint16_t>(raw[0])

           | static_cast<std::uint16_t>(static_cast<std::uint16_t>(raw[1]) << 8U);

}

[[nodiscard]] const ResourceBytes& name_resource() noexcept {

    if (!g_nameResourceResolved) {

        g_nameResourceResolved = true;

        ResourceBytes candidate{};

        if (resource_bytes(IDR_ITEM_NAMES, candidate) && candidate.size >= kNameHeaderSize

            && std::memcmp(candidate.data, kNameMagic.data(), kNameMagic.size()) == 0) {

            g_nameResource = candidate;

        }

    }

    return g_nameResource;

}

[[nodiscard]] std::string_view display_name(std::uint32_t hash) noexcept {

    const ResourceBytes& resource = name_resource();

    if (resource.data == nullptr || resource.size < kNameHeaderSize) {

        return {};

    }

    const std::size_t count = static_cast<std::size_t>(read_u32_le(resource.data + 8));

    if (count > (resource.size - kNameHeaderSize) / kNameEntrySize) {

        return {};

    }

    const std::byte* index = resource.data + kNameHeaderSize;

    std::size_t low = 0;

    std::size_t high = count;

    while (low < high) {

        const std::size_t middle = low + (high - low) / 2;

        const std::byte* entry = index + middle * kNameEntrySize;

        const std::uint32_t candidate = read_u32_le(entry);

        if (candidate < hash) {

            low = middle + 1;

            continue;

        }

        if (candidate > hash) {

            high = middle;

            continue;

        }

        const std::size_t offset = static_cast<std::size_t>(read_u32_le(entry + 4));

        const std::size_t length = static_cast<std::size_t>(read_u16_le(entry + 8));

        if (offset > resource.size || length > resource.size - offset) {

            return {};

        }

        return {reinterpret_cast<const char*>(resource.data + offset), length};

    }

    return {};

}

[[nodiscard]] const ResourceBytes& rarity_resource() noexcept {

    if (!g_rarityResourceResolved) {

        g_rarityResourceResolved = true;

        ResourceBytes candidate{};

        if (resource_bytes(IDR_ITEM_RARITY, candidate) && candidate.size >= kRarityHeaderSize

            && std::memcmp(candidate.data, kRarityMagic.data(), kRarityMagic.size()) == 0) {

            g_rarityResource = candidate;

        }

    }

    return g_rarityResource;

}

[[nodiscard]] WeaponRarity weapon_rarity(std::uint32_t hash) noexcept {

    const ResourceBytes& resource = rarity_resource();

    if (resource.data == nullptr || resource.size < kRarityHeaderSize) {

        return WeaponRarity::unknown;

    }

    const std::size_t count = static_cast<std::size_t>(read_u32_le(resource.data + 8));

    if (count > (resource.size - kRarityHeaderSize) / kRarityEntrySize) {

        return WeaponRarity::unknown;

    }

    const std::byte* index = resource.data + kRarityHeaderSize;

    std::size_t low = 0;

    std::size_t high = count;

    while (low < high) {

        const std::size_t middle = low + (high - low) / 2;

        const std::byte* entry = index + middle * kRarityEntrySize;

        const std::uint32_t candidate = read_u32_le(entry);

        if (candidate < hash) {

            low = middle + 1;

            continue;

        }

        if (candidate > hash) {

            high = middle;

            continue;

        }

        return static_cast<WeaponRarity>(std::to_integer<std::uint8_t>(entry[4]));

    }

    return WeaponRarity::unknown;

}

[[nodiscard]] constexpr WeaponRarityMask rarity_bit(WeaponRarity rarity) noexcept {

    const auto value = static_cast<std::uint8_t>(rarity);

    return value == 0 ? 0 : static_cast<WeaponRarityMask>(1U << (value - 1U));

}

constexpr WeaponRarityMask kAllWeaponRarities =

    rarity_bit(WeaponRarity::common)

    | rarity_bit(WeaponRarity::uncommon)

    | rarity_bit(WeaponRarity::rare)

    | rarity_bit(WeaponRarity::legendary)

    | rarity_bit(WeaponRarity::exotic);

[[nodiscard]] std::size_t selected_character_index(const state::AccountState& account) noexcept {

    for (std::size_t index = 0; index < account.characterCount; ++index) {

        if (account.characters[index].selected) {

            return index;

        }

    }

    return account.characterCount;

}

[[nodiscard]] bool resolve_item(const inventory::Item& item,

                                state::build_data::items::Definition& definition,

                                item_details::Definition& detail) noexcept {

    return item.instanceSoid != 0

           && state::build_data::find_item_definition_hash(item.definitionHash, definition)

           && definition.definitionHash == item.definitionHash

           && state::build_data::find_configured_item_detail(definition.definitionIndex, detail)

           && detail.definitionIndex == definition.definitionIndex

           && detail.definitionHash == definition.definitionHash

           && detail.ordinarySocketState == item_details::OrdinarySocketState::present

           && detail.ordinarySocketCount != 0

           && detail.ordinarySocketCount <= inventory::kPlugCapacity;

}

[[nodiscard]] bool same_instance(const inventory::Item& left,

                                 const inventory::Item& right) noexcept {

    return left.instanceSoid != 0 && left.instanceSoid == right.instanceSoid;

}

void append_weapon(std::vector<WeaponRow>& rows, const inventory::Item& item, bool equipped) {

    if (rows.size() >= kMaximumVisibleWeaponsPerSlot) {

        return;

    }

    WeaponRow row{};

    if (!resolve_item(item, row.definition, row.detail)) {

        return;

    }

    row.item = item;

    row.name = display_name(item.definitionHash);

    row.equipped = equipped;

    rows.push_back(row);

}

void rebuild_weapons(const state::CharacterState& character) {

    std::array<std::uint8_t, 3> bucketIds{};

    std::array<bool, 3> bucketKnown{};

    std::array<inventory::Item, 3> equippedItems{};

    std::array<bool, 3> equippedKnown{};

    for (auto& slot : g_slotWeapons) {

        slot.clear();

    }

    for (std::size_t slot = 0; slot < kWeaponSlots.size(); ++slot) {

        const std::size_t semantic = static_cast<std::size_t>(kWeaponSlots[slot]);

        if (semantic >= character.equipment.slots.size()

            || !character.equipment.slots[semantic].has_value()) {

            continue;

        }

        const inventory::Item& item = *character.equipment.slots[semantic];

        state::build_data::items::Definition definition{};

        item_details::Definition detail{};

        if (!resolve_item(item, definition, detail)) {

            continue;

        }

        bucketIds[slot] = definition.bucketId;

        bucketKnown[slot] = true;

        equippedItems[slot] = item;

        equippedKnown[slot] = true;

        append_weapon(g_slotWeapons[slot], item, true);

    }

    for (std::size_t index = 0; index < character.inventory.count; ++index) {

        const inventory::Item& item = character.inventory.values[index];

        state::build_data::items::Definition definition{};

        item_details::Definition detail{};

        if (!resolve_item(item, definition, detail)) {

            continue;

        }

        for (std::size_t slot = 0; slot < g_slotWeapons.size(); ++slot) {

            if (!bucketKnown[slot] || definition.bucketId != bucketIds[slot]

                || (equippedKnown[slot] && same_instance(item, equippedItems[slot]))) {

                continue;

            }

            append_weapon(g_slotWeapons[slot], item, false);

            break;

        }

    }

    for (std::size_t slot = 0; slot < g_slotWeapons.size(); ++slot) {

        if (g_selectedWeaponBySlot[slot] >= g_slotWeapons[slot].size()) {

            g_selectedWeaponBySlot[slot] = 0;

        }

    }

    if (g_activeSlot >= g_slotWeapons.size()) {

        g_activeSlot = 0;

    }

}

[[nodiscard]] WeaponRow* selected_weapon() noexcept {

    if (g_activeSlot >= g_slotWeapons.size()) {

        return nullptr;

    }

    auto& rows = g_slotWeapons[g_activeSlot];

    if (rows.empty()) {

        return nullptr;

    }

    const std::size_t selected = (std::min)(g_selectedWeaponBySlot[g_activeSlot], rows.size() - 1);

    return &rows[selected];

}

void rebuild_armor(const state::CharacterState& character) {

    std::array<std::uint8_t, 5> bucketIds{};

    std::array<bool, 5> bucketKnown{};

    std::array<inventory::Item, 5> equippedItems{};

    std::array<bool, 5> equippedKnown{};

    for (auto& slot : g_slotArmor) {

        slot.clear();

    }

    for (std::size_t slot = 0; slot < kArmorSlots.size(); ++slot) {

        const std::size_t semantic = static_cast<std::size_t>(kArmorSlots[slot]);

        if (semantic >= character.equipment.slots.size()

            || !character.equipment.slots[semantic].has_value()) {

            continue;

        }

        const inventory::Item& item = *character.equipment.slots[semantic];

        state::build_data::items::Definition definition{};

        item_details::Definition detail{};

        if (!resolve_item(item, definition, detail)) {

            continue;

        }

        bucketIds[slot] = definition.bucketId;

        bucketKnown[slot] = true;

        equippedItems[slot] = item;

        equippedKnown[slot] = true;

        append_weapon(g_slotArmor[slot], item, true);

    }

    for (std::size_t index = 0; index < character.inventory.count; ++index) {

        const inventory::Item& item = character.inventory.values[index];

        state::build_data::items::Definition definition{};

        item_details::Definition detail{};

        if (!resolve_item(item, definition, detail)) {

            continue;

        }

        for (std::size_t slot = 0; slot < g_slotArmor.size(); ++slot) {

            if (!bucketKnown[slot] || definition.bucketId != bucketIds[slot]

                || (equippedKnown[slot] && same_instance(item, equippedItems[slot]))) {

                continue;

            }

            append_weapon(g_slotArmor[slot], item, false);

            break;

        }

    }

    for (std::size_t slot = 0; slot < g_slotArmor.size(); ++slot) {

        if (g_selectedArmorBySlot[slot] >= g_slotArmor[slot].size()) {

            g_selectedArmorBySlot[slot] = 0;

        }

    }

    if (g_activeArmorSlot >= g_slotArmor.size()) {

        g_activeArmorSlot = 0;

    }

}

[[nodiscard]] WeaponRow* selected_armor() noexcept {

    if (g_activeArmorSlot >= g_slotArmor.size()) {

        return nullptr;

    }

    auto& rows = g_slotArmor[g_activeArmorSlot];

    if (rows.empty()) {

        return nullptr;

    }

    const std::size_t selected = (std::min)(g_selectedArmorBySlot[g_activeArmorSlot], rows.size() - 1);

    return &rows[selected];

}

[[nodiscard]] WeaponRow* selected_item() noexcept {

    return g_editorPage == EditorPage::weapons ? selected_weapon() : selected_armor();

}

[[nodiscard]] const char* active_slot_name() noexcept {

    if (g_editorPage == EditorPage::weapons) {

        return g_activeSlot < kWeaponSlotNames.size() ? kWeaponSlotNames[g_activeSlot] : "WEAPON";

    }

    return g_activeArmorSlot < kArmorSlotNames.size() ? kArmorSlotNames[g_activeArmorSlot] : "ARMOR";

}

[[nodiscard]] std::optional<std::uint32_t>

current_plug_hash(const WeaponRow& weapon, std::uint8_t lane) noexcept {

    if (lane >= weapon.detail.ordinarySocketCount) {

        return std::nullopt;

    }

    if (weapon.item.sockets.policy == inventory::SocketPolicy::authored) {

        if (lane >= weapon.item.sockets.plugCount || !weapon.item.sockets.plugs[lane].has_value()) {

            return std::nullopt;

        }

        return *weapon.item.sockets.plugs[lane];

    }

    const std::uint16_t plugIndex = weapon.detail.initialPlugIndices[lane];

    if (plugIndex == item_details::kUnavailableItemIndex) {

        return std::nullopt;

    }

    state::build_data::items::Definition plug{};

    if (!state::build_data::find_item_definition_index(plugIndex, plug)) {

        return std::nullopt;

    }

    return plug.definitionHash;

}

void clear_pending_changes() noexcept {

    g_pendingPlugLanes.fill(false);

    g_pendingPlugDefinitions.fill(0);

    g_pendingInstanceSoid = 0;

}

[[nodiscard]] bool has_pending_plug(const WeaponRow& weapon, std::uint8_t lane) noexcept {

    return lane < inventory::kPlugCapacity

        && g_pendingInstanceSoid == weapon.item.instanceSoid

        && g_pendingPlugLanes[lane];

}

[[nodiscard]] std::optional<std::uint32_t>

effective_plug_hash(const WeaponRow& weapon, std::uint8_t lane) noexcept {

    if (has_pending_plug(weapon, lane)) {

        state::build_data::items::Definition plug{};

        if (state::build_data::find_item_definition_index(g_pendingPlugDefinitions[lane], plug)) {

            return plug.definitionHash;

        }

    }

    return current_plug_hash(weapon, lane);

}

[[nodiscard]] std::size_t pending_change_count(const WeaponRow& weapon) noexcept {

    if (g_pendingInstanceSoid != weapon.item.instanceSoid) {

        return 0;

    }

    std::size_t count = 0;

    for (std::uint8_t lane = 0; lane < weapon.detail.ordinarySocketCount; ++lane) {

        if (g_pendingPlugLanes[lane]) {

            ++count;

        }

    }

    return count;

}

void stage_plug(const WeaponRow& weapon,

                std::uint8_t lane,

                std::uint16_t definitionIndex) noexcept {

    if (lane >= weapon.detail.ordinarySocketCount) {

        return;

    }

    if (g_pendingInstanceSoid != 0 && g_pendingInstanceSoid != weapon.item.instanceSoid) {

        clear_pending_changes();

    }

    g_pendingInstanceSoid = weapon.item.instanceSoid;

    state::build_data::items::Definition plug{};

    const std::optional<std::uint32_t> current = current_plug_hash(weapon, lane);

    if (state::build_data::find_item_definition_index(definitionIndex, plug)

        && current.has_value() && *current == plug.definitionHash) {

        g_pendingPlugLanes[lane] = false;

        if (pending_change_count(weapon) == 0) {

            g_pendingInstanceSoid = 0;

        }

        return;

    }

    g_pendingPlugDefinitions[lane] = definitionIndex;

    g_pendingPlugLanes[lane] = true;

}

[[nodiscard]] bool text_ends_with(std::string_view text, std::string_view suffix) noexcept {

    return text.size() >= suffix.size()

           && text.substr(text.size() - suffix.size(), suffix.size()) == suffix;

}

[[nodiscard]] SocketKind socket_kind(const item_details::Definition& detail,

                                     std::uint8_t lane) noexcept {

    if (lane >= detail.ordinarySocketCount

        || detail.socketTypes[lane] == item_details::kUnavailableSocketType) {

        return SocketKind::hidden;

    }

    std::string_view name{};

    const std::uint16_t initialIndex = detail.initialPlugIndices[lane];

    state::build_data::items::Definition initialPlug{};

    if (initialIndex != item_details::kUnavailableItemIndex

        && state::build_data::find_item_definition_index(initialIndex, initialPlug)) {

        name = display_name(initialPlug.definitionHash);

    }

    if (name == "Default Shader") {

        return SocketKind::shader;

    }

    if (name == "Default Ornament" || text_ends_with(name, " Ornament")) {

        return SocketKind::ornament;

    }

    if (name == "Tracker Disabled" || name.find("Kill Tracker") != std::string_view::npos

        || name == "Crucible Tracker") {

        return SocketKind::tracker;

    }

    if (name == "Empty Catalyst Socket" || text_ends_with(name, " Catalyst")) {

        return SocketKind::catalyst;

    }

    if (name.find("Masterwork") != std::string_view::npos || name == "Upgrade Armor") {

        return SocketKind::masterwork;

    }

    if (name.find("Mod Socket") != std::string_view::npos) {

        return SocketKind::mod;

    }

    const std::uint16_t type = detail.socketTypes[lane];

    if (type == 180 || type == 182) {

        return SocketKind::shader;

    }

    if (type == 518 || type == 49 || type == 50) {

        return SocketKind::tracker;

    }

    if (type == 68 || type == 69 || (type >= 687 && type <= 703 && type != 693)) {

        return SocketKind::mod;

    }

    if (type == 483 || (type >= 185 && type <= 191) || type == 193

        || (type >= 197 && type <= 206) || type == 216 || (type >= 218 && type <= 220)) {

        return SocketKind::masterwork;

    }

    if ((type >= 421 && type <= 450) || (type >= 562 && type <= 582)

        || (type >= 639 && type <= 642) || (type >= 680 && type <= 685)

        || (type >= 714 && type <= 716) || (type >= 725 && type <= 727)

        || (type >= 736 && type <= 738)) {

        return SocketKind::catalyst;

    }

    return SocketKind::perk;

}

[[nodiscard]] SocketKind socket_kind(const WeaponRow& weapon, std::uint8_t lane) noexcept {

    return socket_kind(weapon.detail, lane);

}

[[nodiscard]] bool is_weapon_detail(const item_details::Definition& detail) noexcept {

    if (!detail.equipmentSlot.has_value()) {

        return false;

    }

    const std::int8_t slot = *detail.equipmentSlot;

    return slot >= kNativeKineticSlot && slot <= kNativePowerSlot;

}

[[nodiscard]] bool is_exotic_weapon_detail(const item_details::Definition& detail) noexcept {

    if (!is_weapon_detail(detail)) {

        return false;

    }

    if (weapon_rarity(detail.definitionHash) == WeaponRarity::exotic) {

        return true;

    }

    for (std::uint8_t lane = 0; lane < detail.ordinarySocketCount; ++lane) {

        if (socket_kind(detail, lane) == SocketKind::catalyst) {

            return true;

        }

    }

    return false;

}

[[nodiscard]] PlugCategoryMask category_for_socket(const item_details::Definition& detail,

                                                    std::uint8_t lane) noexcept {

    if (lane >= detail.ordinarySocketCount) {

        return kPlugCategoryNone;

    }

    const bool weapon = is_weapon_detail(detail);

    const std::uint16_t type = detail.socketTypes[lane];

    if (weapon && type == 176) {

        return is_exotic_weapon_detail(detail)

            ? kPlugCategoryExoticIntrinsic

            : kPlugCategoryIntrinsic;

    }

    switch (socket_kind(detail, lane)) {

        case SocketKind::shader: return kPlugCategoryShader;

        case SocketKind::ornament: return kPlugCategoryOrnament;

        case SocketKind::mod: return weapon ? kPlugCategoryMod : kPlugCategoryArmorMod;

        case SocketKind::catalyst: return kPlugCategoryCatalyst;

        case SocketKind::masterwork: return kPlugCategoryMasterwork;

        case SocketKind::tracker: return kPlugCategoryTracker;

        case SocketKind::perk: return weapon ? kPlugCategoryWeaponPerk : kPlugCategoryArmorMod;

        default: return kPlugCategoryNone;

    }

}

void rebuild_plug_categories() {

    const std::size_t definitionCount = state::build_data::item_definition_count();

    const std::size_t ruleCount = socket_plugs::rule_count();

    if (g_cachedPlugCategoryDefinitionCount == definitionCount

        && g_cachedPlugCategoryRuleCount == ruleCount

        && g_plugCategoryByDefinition.size() == definitionCount) {

        return;

    }

    g_plugCategoryByDefinition.assign(definitionCount, kPlugCategoryNone);

    std::vector<socket_plugs::Rule> rules(socket_plugs::kRuleCapacity);

    std::vector<socket_plugs::Pool> pools(socket_plugs::kPoolCapacity);

    std::vector<socket_plugs::Member> members(socket_plugs::kMemberCapacity);

    std::size_t copiedRules = 0;

    std::size_t copiedPools = 0;

    std::size_t copiedMembers = 0;

    if (!socket_plugs::snapshot(rules, copiedRules, pools, copiedPools, members, copiedMembers)) {

        g_cachedPlugCategoryDefinitionCount = definitionCount;

        g_cachedPlugCategoryRuleCount = ruleCount;

        return;

    }

    for (std::size_t index = 0; index < copiedRules; ++index) {

        const socket_plugs::Rule& rule = rules[index];

        if (rule.poolIndex >= copiedPools) {

            continue;

        }

        item_details::Definition detail{};

        if (!state::build_data::find_configured_item_detail(rule.itemDefinitionIndex, detail)) {

            continue;

        }

        const PlugCategoryMask category = category_for_socket(detail, rule.lane);

        if (category == kPlugCategoryNone) {

            continue;

        }

        const socket_plugs::Pool& pool = pools[rule.poolIndex];

        const std::size_t first = static_cast<std::size_t>(pool.memberOffset);

        const std::size_t count = static_cast<std::size_t>(pool.memberCount);

        if (first > copiedMembers || count > copiedMembers - first) {

            continue;

        }

        for (std::size_t memberIndex = first; memberIndex < first + count; ++memberIndex) {

            const std::size_t definitionIndex = static_cast<std::size_t>(members[memberIndex]);

            if (definitionIndex < g_plugCategoryByDefinition.size()) {

                g_plugCategoryByDefinition[definitionIndex] |= category;

            }

        }

    }

    for (PlugCategoryMask& categories : g_plugCategoryByDefinition) {

        if ((categories & kPlugCategoryExoticIntrinsic) != 0) {

            categories &= static_cast<PlugCategoryMask>(~kPlugCategoryIntrinsic);

        }

    }

    g_cachedPlugCategoryDefinitionCount = definitionCount;

    g_cachedPlugCategoryRuleCount = ruleCount;

}

[[nodiscard]] const char* socket_kind_label(SocketKind kind) noexcept {

    switch (kind) {

        case SocketKind::ornament: return "ORNAMENT";

        case SocketKind::mod: return "MOD";

        case SocketKind::catalyst: return "CATALYST";

        case SocketKind::shader: return "SHADER";

        case SocketKind::tracker: return "TRACKER";

        case SocketKind::masterwork: return "MASTERWORK";

        default: return "PERK";

    }

}

[[nodiscard]] const char* editor_socket_label(SocketKind kind) noexcept {

    if (g_editorPage == EditorPage::armor && kind == SocketKind::perk) {

        return "ARMOR MOD";

    }

    return socket_kind_label(kind);

}

[[nodiscard]] bool editable_lane(const WeaponRow& weapon, std::uint8_t lane) noexcept {

    return socket_kind(weapon, lane) != SocketKind::hidden;

}

[[nodiscard]] std::uint8_t first_editable_lane(const WeaponRow& weapon) noexcept {

    for (std::uint8_t lane = 0; lane < weapon.detail.ordinarySocketCount; ++lane) {

        if (editable_lane(weapon, lane)) {

            return lane;

        }

    }

    return 0;

}

void invalidate_plug_cache() noexcept {

    g_cachedItemDefinition = 0xFFFFU;

    g_cachedLane = 0xFFU;

    g_cachedDefinitionCount = 0;

    g_plugs.clear();

    g_visiblePlugs.clear();

    g_selectedPlug = 0;

}

void invalidate_weapon_catalog() noexcept {

    g_cachedWeaponCatalogSource = 0xFFFFU;

    g_cachedWeaponCatalogCount = 0;

    g_weaponCatalog.clear();

    g_visibleWeaponCatalog.clear();

    g_selectedReplacement = 0;

}

void reset_selection_for_weapon(const WeaponRow& weapon) noexcept {

    clear_pending_changes();

    g_selectedLane = first_editable_lane(weapon);

    g_message[0] = '\0';

    g_browserMode = BrowserMode::plugs;

    invalidate_plug_cache();

    invalidate_weapon_catalog();

}

void rebuild_plugs(const WeaponRow& weapon) {

    const std::size_t definitionCount = state::build_data::item_definition_count();

    rebuild_plug_categories();

    if (g_cachedItemDefinition == weapon.definition.definitionIndex

        && g_cachedLane == g_selectedLane && g_cachedFilter == g_filter

        && g_cachedDefinitionCount == definitionCount) {

        return;

    }

    g_plugs.clear();

    g_selectedPlug = 0;

    const std::optional<std::uint32_t> current = current_plug_hash(weapon, g_selectedLane);

    const bool hasPending = has_pending_plug(weapon, g_selectedLane);

    const std::uint16_t pendingDefinition = hasPending ? g_pendingPlugDefinitions[g_selectedLane] : 0;

    for (std::size_t index = 0; index < definitionCount && index <= 0xFFFFU; ++index) {

        const auto definitionIndex = static_cast<std::uint16_t>(index);

        if (!socket_plugs::contains(definitionIndex)) {

            continue;

        }

        if (g_filter == PlugFilter::compatible

            && !state::build_data::is_socket_plug_allowed(

                weapon.definition.definitionIndex, g_selectedLane, definitionIndex)) {

            continue;

        }

        state::build_data::items::Definition definition{};

        if (!state::build_data::find_item_definition_index(definitionIndex, definition)

            || definition.definitionHash == inventory::kNoDefinitionHash) {

            continue;

        }

        PlugChoice choice{};

        choice.definition = definition;

        choice.name = display_name(definition.definitionHash);

        if (index < g_plugCategoryByDefinition.size()) {

            choice.categories = g_plugCategoryByDefinition[index];

        }

        if ((hasPending && definition.definitionIndex == pendingDefinition)

            || (!hasPending && current.has_value() && definition.definitionHash == *current)) {

            g_selectedPlug = g_plugs.size();

        }

        g_plugs.push_back(choice);

    }

    g_cachedItemDefinition = weapon.definition.definitionIndex;

    g_cachedLane = g_selectedLane;

    g_cachedFilter = g_filter;

    g_cachedDefinitionCount = definitionCount;

}

[[nodiscard]] bool contains_ascii_case_insensitive(std::string_view text,

                                                    std::string_view needle) noexcept {

    if (needle.empty()) {

        return true;

    }

    if (text.size() < needle.size()) {

        return false;

    }

    for (std::size_t start = 0; start + needle.size() <= text.size(); ++start) {

        bool match = true;

        for (std::size_t index = 0; index < needle.size(); ++index) {

            const auto left = static_cast<unsigned char>(text[start + index]);

            const auto right = static_cast<unsigned char>(needle[index]);

            if (std::tolower(left) != std::tolower(right)) {

                match = false;

                break;

            }

        }

        if (match) {

            return true;

        }

    }

    return false;

}

[[nodiscard]] bool plug_matches_search(const PlugChoice& plug) noexcept {

    if (g_plugCategoryFilter != kPlugCategoryNone

        && (plug.categories & g_plugCategoryFilter) == 0) {

        return false;

    }

    const std::string_view needle{g_search.data(), std::strlen(g_search.data())};

    if (needle.empty()) {

        return true;

    }

    if (!plug.name.empty() && contains_ascii_case_insensitive(plug.name, needle)) {

        return true;

    }

    std::array<char, 48> technical{};

    (void)std::snprintf(technical.data(),

                        technical.size(),

                        "0x%08X %u",

                        static_cast<unsigned>(plug.definition.definitionHash),

                        static_cast<unsigned>(plug.definition.definitionIndex));

    return contains_ascii_case_insensitive(technical.data(), needle);

}

void rebuild_visible_plugs() {

    g_visiblePlugs.clear();

    g_visiblePlugs.reserve(g_plugs.size());

    for (std::size_t index = 0; index < g_plugs.size(); ++index) {

        if (plug_matches_search(g_plugs[index])) {

            g_visiblePlugs.push_back(index);

        }

    }

}

[[nodiscard]] bool json_string_u64(std::string_view document,

                                   std::size_t field,

                                   std::uint64_t& value) noexcept {

    const std::size_t colon = document.find(':', field);

    if (colon == std::string_view::npos) {

        return false;

    }

    std::size_t begin = document.find('"', colon + 1);

    if (begin == std::string_view::npos) {

        return false;

    }

    ++begin;

    const std::size_t end = document.find('"', begin);

    if (end == std::string_view::npos) {

        return false;

    }

    std::string_view text = document.substr(begin, end - begin);

    if (text.size() >= 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {

        text.remove_prefix(2);

    }

    if (text.empty()) {

        return false;

    }

    value = 0;

    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value, 16);

    return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();

}

[[nodiscard]] std::size_t matching_object_end(std::string_view document,

                                              std::size_t objectStart) noexcept {

    std::size_t depth = 0;

    bool inString = false;

    bool escaped = false;

    for (std::size_t index = objectStart; index < document.size(); ++index) {

        const char c = document[index];

        if (inString) {

            if (escaped) {

                escaped = false;

            } else if (c == '\\') {

                escaped = true;

            } else if (c == '"') {

                inString = false;

            }

            continue;

        }

        if (c == '"') {

            inString = true;

        } else if (c == '{') {

            ++depth;

        } else if (c == '}') {

            if (depth == 0) {

                return std::string_view::npos;

            }

            --depth;

            if (depth == 0) {

                return index + 1;

            }

        }

    }

    return std::string_view::npos;

}

[[nodiscard]] bool replace_object_field(std::string& document,

                                        std::size_t objectStart,

                                        std::size_t& objectEnd,

                                        std::string_view key,

                                        std::string_view replacement) {

    const std::string quotedKey = std::string("\"") + std::string(key) + "\"";

    const std::size_t field = document.find(quotedKey, objectStart);

    if (field == std::string::npos || field >= objectEnd) {

        return false;

    }

    const std::size_t colon = document.find(':', field + quotedKey.size());

    if (colon == std::string::npos || colon >= objectEnd) {

        return false;

    }

    std::size_t valueStart = colon + 1;

    while (valueStart < objectEnd

           && (document[valueStart] == ' ' || document[valueStart] == '\t'

               || document[valueStart] == '\r' || document[valueStart] == '\n')) {

        ++valueStart;

    }

    if (valueStart >= objectEnd) {

        return false;

    }

    std::size_t valueEnd = valueStart;

    if (document[valueStart] == '"') {

        ++valueEnd;

        bool escaped = false;

        while (valueEnd < objectEnd) {

            const char c = document[valueEnd++];

            if (escaped) {

                escaped = false;

            } else if (c == '\\') {

                escaped = true;

            } else if (c == '"') {

                break;

            }

        }

    } else if (document[valueStart] == '[' || document[valueStart] == '{') {

        const char opener = document[valueStart];

        const char closer = opener == '[' ? ']' : '}';

        std::size_t depth = 0;

        bool inString = false;

        bool escaped = false;

        for (; valueEnd < objectEnd; ++valueEnd) {

            const char c = document[valueEnd];

            if (inString) {

                if (escaped) {

                    escaped = false;

                } else if (c == '\\') {

                    escaped = true;

                } else if (c == '"') {

                    inString = false;

                }

                continue;

            }

            if (c == '"') {

                inString = true;

            } else if (c == opener) {

                ++depth;

            } else if (c == closer) {

                if (depth == 0) {

                    return false;

                }

                --depth;

                if (depth == 0) {

                    ++valueEnd;

                    break;

                }

            }

        }

    } else {

        while (valueEnd < objectEnd && document[valueEnd] != ',' && document[valueEnd] != '}') {

            ++valueEnd;

        }

        while (valueEnd > valueStart

               && (document[valueEnd - 1] == ' ' || document[valueEnd - 1] == '\t'

                   || document[valueEnd - 1] == '\r' || document[valueEnd - 1] == '\n')) {

            --valueEnd;

        }

    }

    if (valueEnd <= valueStart || valueEnd > objectEnd) {

        return false;

    }

    const std::size_t oldLength = valueEnd - valueStart;

    document.replace(valueStart, oldLength, replacement.data(), replacement.size());

    if (replacement.size() >= oldLength) {

        objectEnd += replacement.size() - oldLength;

    } else {

        objectEnd -= oldLength - replacement.size();

    }

    return true;

}

[[nodiscard]] std::string serialized_plugs(const inventory::Item& item) {

    if (item.sockets.policy == inventory::SocketPolicy::nativeDefaults) {

        return "null";

    }

    std::string output{"["};

    for (std::size_t lane = 0; lane < item.sockets.plugCount; ++lane) {

        if (lane != 0) {

            output += ',';

        }

        if (!item.sockets.plugs[lane].has_value()) {

            output += "null";

            continue;

        }

        std::array<char, 16> hash{};

        (void)std::snprintf(hash.data(), hash.size(), "\"0x%08X\"",

                            static_cast<unsigned>(*item.sockets.plugs[lane]));

        output += hash.data();

    }

    output += ']';

    return output;

}

[[nodiscard]] std::string serialized_item(const inventory::Item& item) {

    std::array<char, 160> prefix{};

    (void)std::snprintf(prefix.data(),

                        prefix.size(),

                        "{\"instance_soid\":\"0x%016llX\",\"definition_hash\":\"0x%08X\","

                        "\"level\":%d,\"quantity\":%d,\"plugs\":",

                        static_cast<unsigned long long>(item.instanceSoid),

                        static_cast<unsigned>(item.definitionHash),

                        static_cast<int>(item.level),

                        static_cast<int>(item.quantity));

    std::string output{prefix.data()};

    output += serialized_plugs(item);

    if (item.flags != 0) {

        std::array<char, 32> flags{};

        (void)std::snprintf(flags.data(), flags.size(), ",\"flags\":%u", item.flags);

        output += flags.data();

    }

    output += '}';

    return output;

}

constexpr std::array<std::string_view, inventory::kEquipmentSlotCount> kEquipmentSlotJsonNames{

    "kinetic", "energy", "heavy", "helmet", "gauntlets", "chest", "legs", "class_item",

    "ghost", "vehicle", "ship", "subclass", "clan_banner", "emblem", "emote", "finisher",

};

[[nodiscard]] std::string serialized_equipment(const inventory::Equipment& equipment) {

    std::string output{"{"};

    for (std::size_t slot = 0; slot < equipment.slots.size(); ++slot) {

        if (slot != 0) {

            output += ',';

        }

        output += '"';

        output += kEquipmentSlotJsonNames[slot];

        output += "\":";

        if (equipment.slots[slot].has_value()) {

            output += serialized_item(*equipment.slots[slot]);

        } else {

            output += "null";

        }

    }

    output += '}';

    return output;

}

[[nodiscard]] std::string serialized_inventory(const inventory::CharacterItems& items) {

    std::string output{"["};

    for (std::size_t index = 0; index < items.count; ++index) {

        if (index != 0) {

            output += ',';

        }

        output += serialized_item(items.values[index]);

    }

    output += ']';

    return output;

}

[[nodiscard]] bool find_character_object(std::string_view document,

                                         std::uint64_t characterSoid,

                                         std::size_t& objectStart,

                                         std::size_t& objectEnd) noexcept {

    constexpr std::string_view key = "\"soid\"";

    std::size_t search = 0;

    while (search < document.size()) {

        const std::size_t field = document.find(key, search);

        if (field == std::string_view::npos) {

            return false;

        }

        std::uint64_t candidate = 0;

        if (json_string_u64(document, field, candidate) && candidate == characterSoid) {

            objectStart = document.rfind('{', field);

            if (objectStart == std::string_view::npos) {

                return false;

            }

            objectEnd = matching_object_end(document, objectStart);

            return objectEnd != std::string_view::npos;

        }

        search = field + key.size();

    }

    return false;

}

[[nodiscard]] bool persist_character(const state::CharacterState& character) noexcept {

    core::path::Buffer settingsPath{};

    if (!core::path::artifact_directory(owning_module(), settingsPath)

        || !core::path::append(settingsPath, L"\\settings.json")) {

        return false;

    }

    const HANDLE input = CreateFileW(settingsPath.chars.data(),

                                     GENERIC_READ,

                                     FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,

                                     nullptr,

                                     OPEN_EXISTING,

                                     FILE_ATTRIBUTE_NORMAL,

                                     nullptr);

    if (input == INVALID_HANDLE_VALUE) {

        return false;

    }

    LARGE_INTEGER fileSize{};

    if (!GetFileSizeEx(input, &fileSize) || fileSize.QuadPart <= 0

        || fileSize.QuadPart > 1024 * 1024) {

        CloseHandle(input);

        return false;

    }

    std::string document(static_cast<std::size_t>(fileSize.QuadPart), '\0');

    DWORD read = 0;

    const bool readOk = ReadFile(input,

                                 document.data(),

                                 static_cast<DWORD>(document.size()),

                                 &read,

                                 nullptr) != FALSE

                     && static_cast<std::size_t>(read) == document.size();

    const bool closed = CloseHandle(input) != FALSE;

    if (!readOk || !closed) {

        return false;

    }

    std::size_t objectStart = 0;

    std::size_t objectEnd = 0;

    if (!find_character_object(document, character.soid, objectStart, objectEnd)) {

        return false;

    }

    const std::string equipment = serialized_equipment(character.equipment);

    if (!replace_object_field(document, objectStart, objectEnd, "equipment", equipment)) {

        return false;

    }

    const std::string heldItems = serialized_inventory(character.inventory);

    if (!replace_object_field(document, objectStart, objectEnd, "inventory", heldItems)) {

        return false;

    }

    core::path::Buffer stagePath = settingsPath;

    if (!core::path::append(stagePath, L".gear-editor.tmp")) {

        return false;

    }

    const HANDLE output = CreateFileW(stagePath.chars.data(),

                                      GENERIC_WRITE,

                                      0,

                                      nullptr,

                                      CREATE_ALWAYS,

                                      FILE_ATTRIBUTE_NORMAL,

                                      nullptr);

    if (output == INVALID_HANDLE_VALUE) {

        return false;

    }

    DWORD written = 0;

    bool stored = WriteFile(output,

                            document.data(),

                            static_cast<DWORD>(document.size()),

                            &written,

                            nullptr) != FALSE

               && static_cast<std::size_t>(written) == document.size();

    stored = FlushFileBuffers(output) != FALSE && stored;

    stored = CloseHandle(output) != FALSE && stored;

    stored = stored

          && MoveFileExW(stagePath.chars.data(),

                         settingsPath.chars.data(),

                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;

    if (!stored) {

        (void)DeleteFileW(stagePath.chars.data());

    }

    return stored;

}

[[nodiscard]] bool persist_runtime_item(std::uint64_t instanceSoid) noexcept {

    if (instanceSoid == 0) {

        return false;

    }

    const state::AccountState account = state::account_snapshot();

    for (std::size_t characterIndex = 0; characterIndex < account.characterCount; ++characterIndex) {

        const state::CharacterState& character = account.characters[characterIndex];

        for (const auto& equipped : character.equipment.slots) {

            if (equipped.has_value() && equipped->instanceSoid == instanceSoid) {

                return persist_character(character);

            }

        }

        for (std::size_t itemIndex = 0; itemIndex < character.inventory.count; ++itemIndex) {

            if (character.inventory.values[itemIndex].instanceSoid == instanceSoid) {

                return persist_character(character);

            }

        }

    }

    return false;

}

void apply_pending(const WeaponRow& weapon) noexcept {

    const std::size_t requested = pending_change_count(weapon);

    if (requested == 0) {

        return;

    }

    std::size_t applied = 0;

    std::size_t failed = 0;

    for (std::uint8_t lane = 0; lane < weapon.detail.ordinarySocketCount; ++lane) {

        if (!has_pending_plug(weapon, lane)) {

            continue;

        }

        state::PendingSocketPlug mutation{};

        if (!state::prepare_socket_plug_unrestricted(

                weapon.item.instanceSoid, lane, g_pendingPlugDefinitions[lane], mutation)

            || !state::commit_socket_plug(mutation)) {

            ++failed;

            continue;

        }

        g_pendingPlugLanes[lane] = false;

        ++applied;

    }

    const bool persisted = applied != 0 && persist_runtime_item(weapon.item.instanceSoid);

    const bool refreshQueued = applied != 0 && server::bap::request_account_resync();

    if (failed != 0) {
        (void)std::snprintf(g_message.data(),
                            g_message.size(),
                            "Applied %zu change(s); %zu failed.",
                            applied,
                            failed);
        core::ui::toast::post(core::ui::toast::Type::warning, "Gear Editor", "Applied with some failures.");
    } else if (persisted) {
        (void)std::snprintf(g_message.data(),
                            g_message.size(),
                            refreshQueued ? "Applied %zu change(s) and saved. Live refresh queued."
                                          : "Applied %zu change(s) and saved.",
                            applied);
        core::ui::toast::post(core::ui::toast::Type::success, "Gear Editor", "Perk changes applied and saved!");

    } else {

        (void)std::snprintf(g_message.data(),

                            g_message.size(),

                            "Applied %zu change(s), but settings.json could not be saved.",

                            applied);

    }

    if (pending_change_count(weapon) == 0) {

        g_pendingInstanceSoid = 0;

    }

    invalidate_plug_cache();

}

void rebuild_weapon_catalog(const WeaponRow& weapon) {

    const std::size_t definitionCount = state::build_data::item_definition_count();

    if (g_cachedWeaponCatalogSource == weapon.definition.definitionIndex

        && g_cachedWeaponCatalogCount == definitionCount) {

        return;

    }

    g_weaponCatalog.clear();

    g_selectedReplacement = 0;

    for (std::size_t index = 0; index < definitionCount && index <= 0xFFFFU; ++index) {

        state::build_data::items::Definition definition{};

        item_details::Definition detail{};

        if (!state::build_data::find_item_definition_index(static_cast<std::uint16_t>(index), definition)

            || definition.definitionHash == inventory::kNoDefinitionHash

            || definition.bucketId != weapon.definition.bucketId

            || !state::build_data::find_configured_item_detail(definition.definitionIndex, detail)

            || detail.bucketId != definition.bucketId

            || detail.equipmentSlot != weapon.detail.equipmentSlot

            || detail.ordinarySocketState != item_details::OrdinarySocketState::present

            || detail.ordinarySocketCount == 0

            || detail.ordinarySocketCount > inventory::kPlugCapacity) {

            continue;

        }

        const std::string_view name = display_name(definition.definitionHash);

        if (name.empty()) {

            continue;

        }

        if (definition.definitionHash == weapon.item.definitionHash) {

            g_selectedReplacement = g_weaponCatalog.size();

        }

        WeaponRarity rarity = weapon_rarity(definition.definitionHash);

        if (rarity == WeaponRarity::unknown && is_exotic_weapon_detail(detail)) {

            rarity = WeaponRarity::exotic;

        }

        g_weaponCatalog.push_back(WeaponChoice{definition, detail, name, rarity});

    }

    g_cachedWeaponCatalogSource = weapon.definition.definitionIndex;

    g_cachedWeaponCatalogCount = definitionCount;

}

[[nodiscard]] bool weapon_matches_search(const WeaponChoice& choice) noexcept {

    if (g_weaponRarityFilter != 0 && g_weaponRarityFilter != kAllWeaponRarities

        && (rarity_bit(choice.rarity) & g_weaponRarityFilter) == 0) {

        return false;

    }

    const std::string_view needle{g_weaponSearch.data(), std::strlen(g_weaponSearch.data())};

    if (needle.empty()) {

        return true;

    }

    if (contains_ascii_case_insensitive(choice.name, needle)) {

        return true;

    }

    std::array<char, 40> technical{};

    (void)std::snprintf(technical.data(),

                        technical.size(),

                        "0x%08X %u",

                        static_cast<unsigned>(choice.definition.definitionHash),

                        static_cast<unsigned>(choice.definition.definitionIndex));

    return contains_ascii_case_insensitive(technical.data(), needle);

}

void rebuild_visible_weapon_catalog() {

    g_visibleWeaponCatalog.clear();

    g_visibleWeaponCatalog.reserve(g_weaponCatalog.size());

    for (std::size_t index = 0; index < g_weaponCatalog.size(); ++index) {

        if (weapon_matches_search(g_weaponCatalog[index])) {

            g_visibleWeaponCatalog.push_back(index);

        }

    }

}

void replace_selected_weapon(const WeaponRow& weapon) noexcept {

    if (g_selectedReplacement >= g_weaponCatalog.size()) {

        return;

    }

    const WeaponChoice& replacement = g_weaponCatalog[g_selectedReplacement];

    if (replacement.definition.definitionHash == weapon.item.definitionHash) {

        (void)std::snprintf(g_message.data(),

                            g_message.size(),

                            g_editorPage == EditorPage::armor

                                ? "That armor piece is already selected."

                                : "That weapon is already selected.");

        return;

    }

    if (!state::replace_item_definition_unrestricted(

            weapon.item.instanceSoid, replacement.definition.definitionHash)) {

        (void)std::snprintf(g_message.data(),

                            g_message.size(),

                            g_editorPage == EditorPage::armor

                                ? "Armor replacement failed validation."

                                : "Weapon replacement failed validation.");

        return;

    }

    clear_pending_changes();

    const bool persisted = persist_runtime_item(weapon.item.instanceSoid);

    const bool refreshQueued = server::bap::request_account_resync();

    const bool armor = g_editorPage == EditorPage::armor;

    (void)std::snprintf(g_message.data(),

                        g_message.size(),

                        persisted

                            ? (refreshQueued

                                ? (armor ? "Armor replaced, saved, and refreshed."

                                         : "Weapon replaced, saved, and refreshed.")

                                : (armor ? "Armor replaced and saved."

                                         : "Weapon replaced and saved."))

                            : (armor ? "Armor replaced live, but settings.json could not be saved."

                                     : "Weapon replaced live, but settings.json could not be saved."));

    g_browserMode = BrowserMode::plugs;

    g_selectedLane = 0;

    invalidate_plug_cache();

    invalidate_weapon_catalog();

}

[[nodiscard]] ImU32 accent_for_hash(std::uint32_t hash, float alpha = 1.0F) noexcept {

    const float hue = static_cast<float>((hash ^ (hash >> 13U)) % 360U) / 360.0F;

    return ImGui::ColorConvertFloat4ToU32(ImColor::HSV(hue, 0.42F, 0.78F, alpha));

}

void draw_fallback_art(std::uint32_t hash,

                       ImVec2 start,

                       ImVec2 end,

                       float rounding) noexcept {

    ImDrawList* draw = ImGui::GetWindowDrawList();

    draw->AddRectFilled(start, end, ui_color(IM_COL32(34, 39, 49, 255)), rounding);

    const float stripHeight = (end.y - start.y) * 0.28F;

    draw->AddRectFilled(ImVec2(start.x, end.y - stripHeight),

                        end,

                        ui_color(accent_for_hash(hash)),

                        rounding,

                        ImDrawFlags_RoundCornersBottom);

    draw->AddLine(ImVec2(start.x + (end.x - start.x) * 0.20F,

                         start.y + (end.y - start.y) * 0.32F),

                  ImVec2(start.x + (end.x - start.x) * 0.80F,

                         start.y + (end.y - start.y) * 0.48F),

                  ui_color(IM_COL32(220, 225, 235, 180)),

                  scaled(4.0F));

}

void draw_icon(std::uint32_t hash,

               ImVec2 start,

               ImVec2 end,

               float rounding = 4.0F) noexcept {

    const ImTextureID texture = gfx_textures::item_icon(hash);

    if (texture == ImTextureID_Invalid) {

        draw_fallback_art(hash, start, end, rounding);

        return;

    }

    ImGui::GetWindowDrawList()->AddImageRounded(ImTextureRef(texture),

                                                start,

                                                end,

                                                ImVec2(0.0F, 0.0F),

                                                ImVec2(1.0F, 1.0F),

                                                ui_color(IM_COL32_WHITE),

                                                rounding);

}

void tooltip_name_hash(std::string_view name,

                       std::uint32_t hash,

                       std::uint16_t definitionIndex) noexcept {

    if (!ImGui::IsItemHovered()) {

        return;

    }

    ImGui::BeginTooltip();

    if (!name.empty()) {

        ImGui::TextUnformatted(name.data(), name.data() + name.size());

    } else {

        ImGui::TextUnformatted("Unknown item");

    }

    ImGui::TextDisabled("Hash 0x%08X", static_cast<unsigned>(hash));

    ImGui::TextDisabled("Definition %u", static_cast<unsigned>(definitionIndex));

    ImGui::EndTooltip();

}

[[nodiscard]] bool draw_weapon_card(const WeaponRow& weapon,

                                    std::size_t index,

                                    bool selected,

                                    float size) noexcept {

    ImGui::PushID(static_cast<int>(index));

    const ImVec2 start = ImGui::GetCursorScreenPos();

    ImGui::InvisibleButton("weapon_card", ImVec2(size, size));

    const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

    const bool hovered = ImGui::IsItemHovered();

    const ImVec2 end{start.x + size, start.y + size};

    draw_icon(weapon.item.definitionHash, start, end, scaled(5.0F));

    ImDrawList* draw = ImGui::GetWindowDrawList();

    draw->AddRect(ImVec2(start.x + 1.0F, start.y + 1.0F),

                  ImVec2(end.x - 1.0F, end.y - 1.0F),

                  ui_color(selected ? IM_COL32(246, 198, 52, 255)

                                    : (hovered ? IM_COL32(126, 137, 157, 255)

                                               : IM_COL32(72, 82, 99, 255))),

                  scaled(5.0F),

                  0,

                  selected ? scaled(2.5F) : scaled(1.0F));

    if (weapon.equipped) {

        draw->AddRectFilled(ImVec2(start.x + scaled(4.0F), start.y + scaled(4.0F)),

                            ImVec2(start.x + scaled(29.0F), start.y + scaled(22.0F)),

                            ui_color(IM_COL32(15, 18, 24, 235)),

                            scaled(3.0F));

        draw->AddText(ImVec2(start.x + scaled(8.0F), start.y + scaled(6.0F)),

                      ui_color(IM_COL32(246, 198, 52, 255)),

                      "EQ");

    }

    tooltip_name_hash(weapon.name, weapon.item.definitionHash, weapon.definition.definitionIndex);

    ImGui::PopID();

    return clicked;

}

[[nodiscard]] bool draw_perk_socket(const WeaponRow& weapon,

                                    std::uint8_t lane,

                                    bool selected,

                                    float size) noexcept {

    const std::optional<std::uint32_t> hash = effective_plug_hash(weapon, lane);

    ImGui::PushID(static_cast<int>(lane));

    const ImVec2 start = ImGui::GetCursorScreenPos();

    ImGui::InvisibleButton("perk_socket", ImVec2(size, size));

    const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

    const bool hovered = ImGui::IsItemHovered();

    const ImVec2 end{start.x + size, start.y + size};

    const float rounding = size * 0.5F;

    if (hash.has_value()) {

        draw_icon(*hash, start, end, rounding);

    } else {

        ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(start.x + size * 0.5F,

                                                           start.y + size * 0.5F),

                                                    size * 0.47F,

                                                    ui_color(IM_COL32(28, 33, 42, 255)));

    }

    ImGui::GetWindowDrawList()->AddCircle(ImVec2(start.x + size * 0.5F, start.y + size * 0.5F),

                                          size * 0.47F,

                                          ui_color(selected ? IM_COL32(246, 198, 52, 255)

                                                            : (hovered ? IM_COL32(155, 166, 185, 255)

                                                                       : IM_COL32(82, 92, 110, 255))),

                                          0,

                                          selected ? scaled(3.0F) : scaled(1.5F));

    if (has_pending_plug(weapon, lane)) {

        ImGui::GetWindowDrawList()->AddCircleFilled(

            ImVec2(end.x - scaled(7.0F), start.y + scaled(7.0F)),

            scaled(4.0F),

            ui_color(IM_COL32(246, 198, 52, 255)));

    }

    if (hovered) {

        ImGui::BeginTooltip();

        ImGui::Text("Socket %u", static_cast<unsigned>(lane));

        ImGui::TextDisabled("Type %u", static_cast<unsigned>(weapon.detail.socketTypes[lane]));

        if (hash.has_value()) {

            const std::string_view name = display_name(*hash);

            if (!name.empty()) {

                ImGui::TextUnformatted(name.data(), name.data() + name.size());

            }

            ImGui::TextDisabled("0x%08X", static_cast<unsigned>(*hash));

        } else {

            ImGui::TextDisabled("Empty");

        }

        ImGui::EndTooltip();

    }

    ImGui::PopID();

    return clicked;

}

[[nodiscard]] bool draw_extra_socket(const WeaponRow& weapon,

                                     std::uint8_t lane,

                                     SocketKind kind,

                                     bool selected,

                                     float width) noexcept {

    const float iconSize = (std::min)(width - scaled(10.0F), scaled(58.0F));

    const float labelFontSize = ImGui::GetFontSize() * 0.76F;

    const float height = iconSize + scaled(28.0F);

    const std::optional<std::uint32_t> hash = effective_plug_hash(weapon, lane);

    ImGui::PushID(static_cast<int>(lane));

    const ImVec2 start = ImGui::GetCursorScreenPos();

    ImGui::InvisibleButton("extra_socket", ImVec2(width, height));

    const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

    const bool hovered = ImGui::IsItemHovered();

    ImDrawList* draw = ImGui::GetWindowDrawList();

    const ImVec2 iconStart{start.x + (width - iconSize) * 0.5F, start.y + scaled(2.0F)};

    const ImVec2 iconEnd{iconStart.x + iconSize, iconStart.y + iconSize};

    if (hash.has_value()) {

        draw_icon(*hash, iconStart, iconEnd, scaled(5.0F));

    } else {

        draw->AddRect(iconStart,

                      iconEnd,

                      ui_color(IM_COL32(72, 82, 99, 255)),

                      scaled(5.0F),

                      0,

                      scaled(1.2F));

    }

    if (selected || hovered) {

        draw->AddRect(ImVec2(iconStart.x - scaled(2.0F), iconStart.y - scaled(2.0F)),

                      ImVec2(iconEnd.x + scaled(2.0F), iconEnd.y + scaled(2.0F)),

                      ui_color(selected ? IM_COL32(246, 198, 52, 255)

                                        : IM_COL32(126, 137, 157, 255)),

                      scaled(6.0F),

                      0,

                      selected ? scaled(2.0F) : scaled(1.0F));

    }

    if (has_pending_plug(weapon, lane)) {

        draw->AddCircleFilled(ImVec2(iconEnd.x - scaled(3.0F), iconStart.y + scaled(3.0F)),

                              scaled(4.0F),

                              ui_color(IM_COL32(246, 198, 52, 255)));

    }

    const char* label = socket_kind_label(kind);

    const ImVec2 textSize = ImGui::GetFont()->CalcTextSizeA(labelFontSize, FLT_MAX, 0.0F, label);

    const float labelX = start.x + (width - textSize.x) * 0.5F;

    const float labelY = iconEnd.y + scaled(6.0F);

    draw->AddText(ImGui::GetFont(),

                  labelFontSize,

                  ImVec2(labelX, labelY),

                  ui_color(IM_COL32(188, 197, 212, 255)),

                  label);

    if (hovered) {

        ImGui::BeginTooltip();

        ImGui::TextUnformatted(label);

        ImGui::TextDisabled("Socket %u / Type %u",

                            static_cast<unsigned>(lane),

                            static_cast<unsigned>(weapon.detail.socketTypes[lane]));

        if (hash.has_value()) {

            const std::string_view name = display_name(*hash);

            if (!name.empty()) {

                ImGui::TextUnformatted(name.data(), name.data() + name.size());

            }

            ImGui::TextDisabled("0x%08X", static_cast<unsigned>(*hash));

        } else {

            ImGui::TextDisabled("Empty");

        }

        ImGui::EndTooltip();

    }

    ImGui::PopID();

    return clicked;

}

void select_lane(const WeaponRow& weapon, std::uint8_t lane) noexcept {

    if (!editable_lane(weapon, lane)) {

        return;

    }

    g_selectedLane = lane;

    g_browserMode = BrowserMode::plugs;

    g_message[0] = '\0';

    invalidate_plug_cache();

}

void draw_weapon_details(WeaponRow& weapon) noexcept {

    const bool armor = g_editorPage == EditorPage::armor;

    ImGui::TextDisabled(armor ? "SELECTED ARMOR" : "SELECTED WEAPON");

    ImGui::Spacing();

    const float imageSize = scaled(96.0F);

    const ImVec2 imageStart = ImGui::GetCursorScreenPos();

    ImGui::InvisibleButton("change_weapon", ImVec2(imageSize, imageSize));

    const bool changeClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

    const bool imageHovered = ImGui::IsItemHovered();

    const ImVec2 imageEnd{imageStart.x + imageSize, imageStart.y + imageSize};

    draw_icon(weapon.item.definitionHash, imageStart, imageEnd, scaled(5.0F));

    ImGui::GetWindowDrawList()->AddRect(imageStart,

                                        imageEnd,

                                        ui_color(imageHovered ? IM_COL32(246, 198, 52, 255)

                                                              : IM_COL32(92, 102, 122, 255)),

                                        scaled(5.0F),

                                        0,

                                        imageHovered ? scaled(2.0F) : scaled(1.0F));

    if (imageHovered) {

        ImGui::BeginTooltip();

        ImGui::TextUnformatted(armor ? "Change armor definition" : "Change weapon definition");

        ImGui::EndTooltip();

    }

    if (changeClicked) {

        g_browserMode = BrowserMode::weapons;

        g_message[0] = '\0';

        invalidate_weapon_catalog();

    }

    ImGui::SameLine(0.0F, scaled(14.0F));

    ImGui::BeginGroup();

    if (!weapon.name.empty()) {

        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);

        ImGui::TextUnformatted(weapon.name.data(), weapon.name.data() + weapon.name.size());

        ImGui::PopTextWrapPos();

    } else {

        ImGui::TextUnformatted(armor ? "Unknown armor" : "Unknown weapon");

    }

    ImGui::TextDisabled("%s", active_slot_name());

    ImGui::Spacing();

    ImGui::TextDisabled("Hash  0x%08X", static_cast<unsigned>(weapon.item.definitionHash));

    ImGui::TextDisabled("Definition  %u", static_cast<unsigned>(weapon.definition.definitionIndex));

    ImGui::TextDisabled("Instance  0x%llX", static_cast<unsigned long long>(weapon.item.instanceSoid));

    ImGui::Spacing();

    ImGui::TextColored(ImVec4(0.72F, 0.76F, 0.83F, 1.0F),

                       armor ? "Click the armor to replace it" : "Click the weapon to replace it");

    ImGui::EndGroup();

    ImGui::Spacing();

    ImGui::Separator();

    ImGui::Spacing();

    std::array<std::uint8_t, inventory::kPlugCapacity> perkLanes{};

    std::array<std::uint8_t, inventory::kPlugCapacity> extraLanes{};

    std::array<SocketKind, inventory::kPlugCapacity> extraKinds{};

    std::size_t perkCount = 0;

    std::size_t extraCount = 0;

    for (std::uint8_t lane = 0; lane < weapon.detail.ordinarySocketCount; ++lane) {

        const SocketKind kind = socket_kind(weapon, lane);

        if (kind == SocketKind::hidden) {

            continue;

        }

        if (kind == SocketKind::perk) {

            perkLanes[perkCount++] = lane;

        } else {

            extraLanes[extraCount] = lane;

            extraKinds[extraCount] = kind;

            ++extraCount;

        }

    }

    ImGui::TextDisabled(armor ? "ARMOR SOCKETS" : "WEAPON PERKS");

    const float width = ImGui::GetContentRegionAvail().x;

    const std::size_t perkColumns = (std::max)(std::size_t{1}, (std::min)(perkCount, std::size_t{6}));

    const float perkGap = scaled(10.0F);

    const float perkSize = (std::clamp)(

        (width - perkGap * static_cast<float>(perkColumns - 1)) / static_cast<float>(perkColumns),

        scaled(50.0F),

        scaled(78.0F));

    for (std::size_t index = 0; index < perkCount; ++index) {

        if (index % perkColumns != 0) {

            ImGui::SameLine(0.0F, perkGap);

        }

        const std::uint8_t lane = perkLanes[index];

        if (draw_perk_socket(weapon, lane, lane == g_selectedLane, perkSize)) {

            select_lane(weapon, lane);

        }

    }

    if (perkCount == 0) {

        ImGui::TextDisabled(armor ? "No armor sockets" : "No perk sockets");

    }

    if (extraCount != 0) {

        ImGui::Spacing();

        ImGui::TextDisabled("EXTRA SLOTS");

        const float extraGap = scaled(14.0F);

        const std::size_t fitColumns = static_cast<std::size_t>((std::max)(

            1.0F,

            std::floor((width + extraGap) / (scaled(94.0F) + extraGap))));

        const std::size_t extraColumns = (std::max)(

            std::size_t{1},

            (std::min)(extraCount, (std::min)(fitColumns, std::size_t{4})));

        const float extraWidth = (std::min)(

            scaled(106.0F),

            (width - extraGap * static_cast<float>(extraColumns - 1))

                / static_cast<float>(extraColumns));

        for (std::size_t index = 0; index < extraCount; ++index) {

            if (index % extraColumns != 0) {

                ImGui::SameLine(0.0F, extraGap);

            }

            const std::uint8_t lane = extraLanes[index];

            if (draw_extra_socket(weapon,

                                  lane,

                                  extraKinds[index],

                                  lane == g_selectedLane,

                                  extraWidth)) {

                select_lane(weapon, lane);

            }

        }

    }

    ImGui::Spacing();

    ImGui::Separator();

    ImGui::Spacing();

    const std::optional<std::uint32_t> current = effective_plug_hash(weapon, g_selectedLane);

    ImGui::TextDisabled("Selected: %s / Socket %u",

                        editor_socket_label(socket_kind(weapon, g_selectedLane)),

                        static_cast<unsigned>(g_selectedLane));

    if (current.has_value()) {

        const std::string_view name = display_name(*current);

        if (!name.empty()) {

            ImGui::TextUnformatted(name.data(), name.data() + name.size());

        }

        ImGui::SameLine();

        ImGui::TextDisabled("0x%08X", static_cast<unsigned>(*current));

    } else {

        ImGui::TextDisabled("Empty");

    }

}

[[nodiscard]] bool draw_slot_tab(std::size_t slot,

                                 const char* label,

                                 bool active,

                                 float width,

                                 float height) noexcept {

    ImGui::PushID(static_cast<int>(slot));

    const ImVec2 start = ImGui::GetCursorScreenPos();

    ImGui::InvisibleButton("gear_slot_tab", ImVec2(width, height));

    const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

    const bool hovered = ImGui::IsItemHovered();

    const ImVec2 end{start.x + width, start.y + height};

    ImDrawList* draw = ImGui::GetWindowDrawList();

    draw->AddRectFilled(start,

                        end,

                        ui_color(active ? IM_COL32(76, 63, 18, 255)

                                        : (hovered ? IM_COL32(36, 42, 52, 255)

                                                   : IM_COL32(28, 33, 42, 255))),

                        scaled(6.0F));

    const ImVec2 textSize = ImGui::CalcTextSize(label);

    draw->AddText(ImVec2(start.x + (width - textSize.x) * 0.5F,

                         start.y + (height - textSize.y) * 0.5F - scaled(1.5F)),

                  ui_color(IM_COL32(238, 241, 247, 255)),

                  label);

    ImGui::PopID();

    return clicked;

}

void draw_inventory_panel() noexcept {

    ImGui::TextDisabled("WEAPON INVENTORY");

    ImGui::Spacing();

    const float tabGap = scaled(8.0F);

    const float tabWidth = (ImGui::GetContentRegionAvail().x - tabGap * 2.0F) / 3.0F;

    for (std::size_t slot = 0; slot < kWeaponSlotNames.size(); ++slot) {

        if (slot != 0) {

            ImGui::SameLine(0.0F, tabGap);

        }

        if (draw_slot_tab(slot,

                          kWeaponSlotNames[slot],

                          g_activeSlot == slot,

                          tabWidth,

                          scaled(38.0F))

            && g_activeSlot != slot) {

            g_activeSlot = slot;

            if (WeaponRow* weapon = selected_weapon(); weapon != nullptr) {

                reset_selection_for_weapon(*weapon);

            } else {

                g_selectedLane = 0;

                invalidate_plug_cache();

                invalidate_weapon_catalog();

            }

        }

    }

    ImGui::Spacing();

    auto& rows = g_slotWeapons[g_activeSlot];

    if (rows.empty()) {

        ImGui::TextWrapped("No weapon instances were found for this slot.");

        return;

    }

    const float availableWidth = ImGui::GetContentRegionAvail().x;

    const float availableHeight = ImGui::GetContentRegionAvail().y;

    const float horizontal = (availableWidth - scaled(kInventoryGap) * static_cast<float>(kWeaponColumns - 1))

                           / static_cast<float>(kWeaponColumns);

    const float vertical = (availableHeight - scaled(kInventoryGap)) * 0.5F;

    const float cardSize = (std::clamp)((std::min)(horizontal, vertical),

                                        scaled(68.0F),

                                        scaled(128.0F));

    for (std::size_t index = 0; index < rows.size(); ++index) {

        if (index % kWeaponColumns != 0) {

            ImGui::SameLine(0.0F, scaled(kInventoryGap));

        }

        if (draw_weapon_card(rows[index],

                             index,

                             index == g_selectedWeaponBySlot[g_activeSlot],

                             cardSize)) {

            if (g_selectedWeaponBySlot[g_activeSlot] != index) {

                g_selectedWeaponBySlot[g_activeSlot] = index;

                reset_selection_for_weapon(rows[index]);

            }

        }

    }

}

void draw_armor_inventory_panel() noexcept {

    ImGui::TextDisabled("ARMOR INVENTORY");

    ImGui::Spacing();

    const float tabGap = scaled(6.0F);

    const float tabWidth = (ImGui::GetContentRegionAvail().x - tabGap * 4.0F) / 5.0F;

    for (std::size_t slot = 0; slot < kArmorSlotNames.size(); ++slot) {

        if (slot != 0) {

            ImGui::SameLine(0.0F, tabGap);

        }

        if (draw_slot_tab(slot,

                          kArmorSlotNames[slot],

                          g_activeArmorSlot == slot,

                          tabWidth,

                          scaled(38.0F))

            && g_activeArmorSlot != slot) {

            g_activeArmorSlot = slot;

            if (WeaponRow* armor = selected_armor(); armor != nullptr) {

                reset_selection_for_weapon(*armor);

            } else {

                g_selectedLane = 0;

                invalidate_plug_cache();

                invalidate_weapon_catalog();

            }

        }

    }

    ImGui::Spacing();

    auto& rows = g_slotArmor[g_activeArmorSlot];

    if (rows.empty()) {

        ImGui::TextWrapped("No armor instances were found for this slot.");

        return;

    }

    const float availableWidth = ImGui::GetContentRegionAvail().x;

    const float availableHeight = ImGui::GetContentRegionAvail().y;

    const float horizontal = (availableWidth - scaled(kInventoryGap) * static_cast<float>(kWeaponColumns - 1))

                           / static_cast<float>(kWeaponColumns);

    const float vertical = (availableHeight - scaled(kInventoryGap)) * 0.5F;

    const float cardSize = (std::clamp)((std::min)(horizontal, vertical),

                                        scaled(68.0F),

                                        scaled(128.0F));

    for (std::size_t index = 0; index < rows.size(); ++index) {

        if (index % kWeaponColumns != 0) {

            ImGui::SameLine(0.0F, scaled(kInventoryGap));

        }

        if (draw_weapon_card(rows[index],

                             index,

                             index == g_selectedArmorBySlot[g_activeArmorSlot],

                             cardSize)) {

            if (g_selectedArmorBySlot[g_activeArmorSlot] != index) {

                g_selectedArmorBySlot[g_activeArmorSlot] = index;

                reset_selection_for_weapon(rows[index]);

            }

        }

    }

}

[[nodiscard]] bool draw_plug_card(const PlugChoice& choice,

                                  std::size_t plugIndex,

                                  bool selected,

                                  float width) noexcept {

    const float cardHeight = scaled(kPlugCardHeight);

    const float iconSize = scaled(kPlugIconSize);

    ImGui::PushID(static_cast<int>(plugIndex));

    const ImVec2 start = ImGui::GetCursorScreenPos();

    ImGui::InvisibleButton("plug_card", ImVec2(width, cardHeight));

    const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

    const bool hovered = ImGui::IsItemHovered();

    ImDrawList* draw = ImGui::GetWindowDrawList();

    const ImVec2 end{start.x + width, start.y + cardHeight};

    draw->AddRectFilled(start,

                        end,

                        ui_color(hovered ? IM_COL32(42, 48, 60, 255) : IM_COL32(29, 34, 43, 255)),

                        scaled(5.0F));

    draw->AddRect(ImVec2(start.x + 1.0F, start.y + 1.0F),

                  ImVec2(end.x - 1.0F, end.y - 1.0F),

                  ui_color(selected ? IM_COL32(246, 198, 52, 255) : IM_COL32(65, 73, 88, 255)),

                  scaled(5.0F),

                  0,

                  selected ? scaled(2.0F) : scaled(1.0F));

    const ImVec2 iconStart{start.x + scaled(7.0F), start.y + scaled(9.0F)};

    const ImVec2 iconEnd{iconStart.x + iconSize, iconStart.y + iconSize};

    draw_icon(choice.definition.definitionHash, iconStart, iconEnd, iconSize * 0.5F);

    const float textX = iconEnd.x + scaled(8.0F);

    const float textWidth = (std::max)(scaled(20.0F), end.x - textX - scaled(7.0F));

    if (!choice.name.empty()) {

        const char* begin = choice.name.data();

        const char* finish = begin + choice.name.size();

        const ImVec2 clippedMax{end.x - scaled(6.0F), start.y + scaled(40.0F)};

        draw->PushClipRect(ImVec2(textX, start.y + scaled(5.0F)), clippedMax, true);

        draw->AddText(ImGui::GetFont(),

                      ImGui::GetFontSize(),

                      ImVec2(textX, start.y + scaled(8.0F)),

                      ui_color(IM_COL32_WHITE),

                      begin,

                      finish,

                      textWidth);

        draw->PopClipRect();

    } else {

        draw->AddText(ImVec2(textX, start.y + scaled(8.0F)),

                      ui_color(IM_COL32_WHITE),

                      "Unknown plug");

    }

    std::array<char, 40> technical{};

    (void)std::snprintf(technical.data(),

                        technical.size(),

                        "0x%08X",

                        static_cast<unsigned>(choice.definition.definitionHash));

    draw->AddText(ImVec2(textX, start.y + scaled(44.0F)),

                  ui_color(IM_COL32(153, 164, 181, 255)),

                  technical.data());

    tooltip_name_hash(choice.name, choice.definition.definitionHash, choice.definition.definitionIndex);

    ImGui::PopID();

    return clicked;

}

[[nodiscard]] bool draw_weapon_choice_card(const WeaponChoice& choice,

                                           std::size_t index,

                                           bool selected,

                                           float width) noexcept {

    const float height = scaled(kWeaponCatalogCardHeight);

    const float iconSize = height - scaled(12.0F);

    ImGui::PushID(static_cast<int>(index));

    const ImVec2 start = ImGui::GetCursorScreenPos();

    ImGui::InvisibleButton("weapon_choice", ImVec2(width, height));

    const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

    const bool hovered = ImGui::IsItemHovered();

    const ImVec2 end{start.x + width, start.y + height};

    ImDrawList* draw = ImGui::GetWindowDrawList();

    draw->AddRectFilled(start,

                        end,

                        ui_color(hovered ? IM_COL32(42, 48, 60, 255) : IM_COL32(29, 34, 43, 255)),

                        scaled(5.0F));

    draw->AddRect(start,

                  end,

                  ui_color(selected ? IM_COL32(246, 198, 52, 255) : IM_COL32(65, 73, 88, 255)),

                  scaled(5.0F),

                  0,

                  selected ? scaled(2.0F) : scaled(1.0F));

    const ImVec2 iconStart{start.x + scaled(6.0F), start.y + scaled(6.0F)};

    const ImVec2 iconEnd{iconStart.x + iconSize, iconStart.y + iconSize};

    draw_icon(choice.definition.definitionHash, iconStart, iconEnd, scaled(4.0F));

    const float textX = iconEnd.x + scaled(8.0F);

    if (!choice.name.empty()) {

        draw->AddText(ImVec2(textX, start.y + scaled(12.0F)),

                      ui_color(IM_COL32_WHITE),

                      choice.name.data(),

                      choice.name.data() + choice.name.size());

    }

    std::array<char, 32> technical{};

    (void)std::snprintf(technical.data(),

                        technical.size(),

                        "0x%08X",

                        static_cast<unsigned>(choice.definition.definitionHash));

    draw->AddText(ImVec2(textX, start.y + scaled(40.0F)),

                  ui_color(IM_COL32(153, 164, 181, 255)),

                  technical.data());

    tooltip_name_hash(choice.name, choice.definition.definitionHash, choice.definition.definitionIndex);

    ImGui::PopID();

    return clicked;

}

[[nodiscard]] std::size_t active_plug_filter_count() noexcept {

    std::size_t count = 0;

    for (PlugCategoryMask bit = 1; bit <= kPlugCategoryArmorExoticPerk; bit <<= 1U) {

        if ((g_plugCategoryFilter & bit) != 0) {

            ++count;

        }

    }

    return count;

}

void plug_filter_option(const char* label, PlugCategoryMask category) noexcept {

    const bool selected = (g_plugCategoryFilter & category) != 0;

    if (ImGui::Selectable(label, selected, ImGuiSelectableFlags_DontClosePopups)) {

        g_plugCategoryFilter ^= category;

    }

}

void draw_plug_filter_popup() noexcept {

    if (!ImGui::BeginPopup("plug_category_filters")) {

        return;

    }

    ImGui::TextDisabled("PLUG TYPES");

    ImGui::SameLine();

    const float resetX = (std::max)(ImGui::GetCursorPosX(), ImGui::GetContentRegionMax().x - scaled(46.0F));

    ImGui::SameLine(resetX);

    if (ImGui::SmallButton("x##reset_plug_filters")) {

        g_plugCategoryFilter = kPlugCategoryNone;

    }

    if (ImGui::IsItemHovered()) {

        ImGui::SetTooltip("Reset filters");

    }

    ImGui::Separator();

        if (g_editorPage == EditorPage::armor) {
        plug_filter_option("Exotic Perks", kPlugCategoryArmorExoticPerk);
        plug_filter_option("Armor Mods", kPlugCategoryArmorMod);
        plug_filter_option("Shaders", kPlugCategoryShader);
        plug_filter_option("Ornaments", kPlugCategoryOrnament);
        plug_filter_option("Masterworks", kPlugCategoryMasterwork);
    } else {

        plug_filter_option("Weapon Perks", kPlugCategoryWeaponPerk);

        plug_filter_option("Intrinsics", kPlugCategoryIntrinsic);

        plug_filter_option("Exotic Intrinsics", kPlugCategoryExoticIntrinsic);

        ImGui::Separator();

        plug_filter_option("Shaders", kPlugCategoryShader);

        plug_filter_option("Ornaments", kPlugCategoryOrnament);

        plug_filter_option("Mods", kPlugCategoryMod);

        plug_filter_option("Catalysts", kPlugCategoryCatalyst);

        plug_filter_option("Masterworks", kPlugCategoryMasterwork);

        plug_filter_option("Trackers", kPlugCategoryTracker);

    }

    ImGui::EndPopup();

}

[[nodiscard]] std::size_t active_weapon_rarity_count() noexcept {

    std::size_t count = 0;

    for (std::uint8_t bit = 1; bit <= rarity_bit(WeaponRarity::exotic); bit <<= 1U) {

        if ((g_weaponRarityFilter & bit) != 0) {

            ++count;

        }

    }

    return count;

}

void weapon_rarity_option(const char* label, WeaponRarity rarity) noexcept {

    const WeaponRarityMask bit = rarity_bit(rarity);

    const bool selected = (g_weaponRarityFilter & bit) != 0;

    if (ImGui::Selectable(label, selected, ImGuiSelectableFlags_DontClosePopups)) {

        g_weaponRarityFilter ^= bit;

    }

}

void draw_weapon_filter_popup() noexcept {

    if (!ImGui::BeginPopup("weapon_rarity_filters")) {

        return;

    }

    ImGui::TextDisabled("RARITY");

    ImGui::SameLine();

    const float resetX = (std::max)(ImGui::GetCursorPosX(), ImGui::GetContentRegionMax().x - scaled(46.0F));

    ImGui::SameLine(resetX);

    if (ImGui::SmallButton("x##reset_weapon_filters")) {

        g_weaponRarityFilter = 0;

    }

    if (ImGui::IsItemHovered()) {

        ImGui::SetTooltip("Reset filters");

    }

    ImGui::Separator();

    weapon_rarity_option("Exotic", WeaponRarity::exotic);

    weapon_rarity_option("Legendary", WeaponRarity::legendary);

    weapon_rarity_option("Rare", WeaponRarity::rare);

    weapon_rarity_option("Uncommon", WeaponRarity::uncommon);

    weapon_rarity_option("Common", WeaponRarity::common);

    ImGui::EndPopup();

}

void draw_unrestricted_warning() noexcept {

    if (g_filter != PlugFilter::all) {

        return;

    }

    ImGui::SameLine();

    const ImVec2 start = ImGui::GetCursorScreenPos();

    const ImVec2 size{scaled(18.0F), ImGui::GetFrameHeight()};

    ImGui::InvisibleButton("##unrestricted_warning", size);

    ImDrawList* draw = ImGui::GetWindowDrawList();

    const float centerY = start.y + size.y * 0.5F;

    draw->AddTriangleFilled(ImVec2(start.x + size.x * 0.5F, centerY - scaled(7.0F)),

                            ImVec2(start.x + scaled(2.0F), centerY + scaled(6.0F)),

                            ImVec2(start.x + size.x - scaled(2.0F), centerY + scaled(6.0F)),

                            ui_color(IM_COL32(237, 184, 59, 255)));

    draw->AddText(ImVec2(start.x + scaled(6.0F), centerY - scaled(5.5F)),

                  ui_color(IM_COL32(24, 27, 33, 255)),

                  "!");

    if (ImGui::IsItemHovered()) {

        ImGui::BeginTooltip();

        ImGui::TextUnformatted("Incompatible plugs may behave incorrectly or make the item unusable.");

        ImGui::EndTooltip();

    }

}

void draw_plug_browser(WeaponRow& weapon) noexcept {

    rebuild_plugs(weapon);

    ImGui::TextDisabled("PLUG BROWSER");

    ImGui::SameLine();

    ImGui::Text("%s / Socket %u",

                editor_socket_label(socket_kind(weapon, g_selectedLane)),

                static_cast<unsigned>(g_selectedLane));

    ImGui::Spacing();

    const float width = ImGui::GetContentRegionAvail().x;

    const float modeWidth = scaled(166.0F);

    const float filtersWidth = scaled(104.0F);

    const float warningWidth = g_filter == PlugFilter::all ? scaled(18.0F) : 0.0F;

    const float gaps = ImGui::GetStyle().ItemSpacing.x * (g_filter == PlugFilter::all ? 3.0F : 2.0F);

    const float searchWidth = (std::max)(scaled(130.0F),

                                         width - modeWidth - filtersWidth - warningWidth - gaps);

    ImGui::SetNextItemWidth(searchWidth);

    ImGui::InputTextWithHint("##plug_search",

                             "Search plug name or hash...",

                             g_search.data(),

                             g_search.size());

    ImGui::SameLine();

    const char* filterPreview = g_filter == PlugFilter::compatible ? "Compatible" : "All plugs";

    ImGui::SetNextItemWidth(modeWidth);

    if (ImGui::BeginCombo("##plug_filter", filterPreview)) {

        if (ImGui::Selectable("Compatible", g_filter == PlugFilter::compatible)) {

            g_filter = PlugFilter::compatible;

            invalidate_plug_cache();

        }

        if (ImGui::Selectable("All plugs", g_filter == PlugFilter::all)) {

            g_filter = PlugFilter::all;

            invalidate_plug_cache();

        }

        ImGui::EndCombo();

    }

    ImGui::SameLine();

    const std::size_t activeFilters = active_plug_filter_count();

    std::array<char, 24> filterLabel{};

    if (activeFilters == 0) {

        (void)std::snprintf(filterLabel.data(), filterLabel.size(), "Filters");

    } else {

        (void)std::snprintf(filterLabel.data(), filterLabel.size(), "Filters (%zu)", activeFilters);

    }

    if (ImGui::Button(filterLabel.data(), ImVec2(filtersWidth, 0.0F))) {

        ImGui::OpenPopup("plug_category_filters");

    }

    draw_plug_filter_popup();

    draw_unrestricted_warning();

    rebuild_visible_plugs();

    const std::size_t pendingCount = pending_change_count(weapon);

    ImGui::TextDisabled("%zu matching plugs", g_visiblePlugs.size());

    if (pendingCount != 0) {

        ImGui::SameLine();

        ImGui::TextColored(ImVec4(0.93F, 0.72F, 0.23F, 1.0F),

                           "%zu pending",

                           pendingCount);

    }

    ImGui::SameLine();

    if (pendingCount == 0) {

        ImGui::BeginDisabled();

    }

    if (ImGui::Button("Apply changes", ImVec2(scaled(178.0F), 0.0F))) {

        apply_pending(weapon);

    }

    if (pendingCount == 0) {

        ImGui::EndDisabled();

    }

    ImGui::SameLine();

    if (ImGui::Button("Refresh", ImVec2(scaled(94.0F), 0.0F))) {

        (void)server::bap::request_account_resync();

    }

    if (g_message[0] != '\0') {

        ImGui::SameLine();

        ImGui::TextDisabled("%s", g_message.data());

    }

    ImGui::Spacing();

    if (ImGui::BeginChild("plug_grid",

                          ImVec2(0.0F, 0.0F),

                          ImGuiChildFlags_Borders,

                          ImGuiWindowFlags_AlwaysVerticalScrollbar)) {

        const float gap = scaled(7.0F);

        const float available = ImGui::GetContentRegionAvail().x;

        const int columns = available >= scaled(570.0F)

            ? 3

            : (available >= scaled(380.0F) ? 2 : 1);

        const float cardWidth = (available - static_cast<float>(columns - 1) * gap)

                              / static_cast<float>(columns);

        const std::size_t rowCount =

            (g_visiblePlugs.size() + static_cast<std::size_t>(columns) - 1)

            / static_cast<std::size_t>(columns);

        ImGuiListClipper clipper;

        clipper.Begin(static_cast<int>(rowCount), scaled(kPlugCardHeight + 7.0F));

        while (clipper.Step()) {

            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {

                const std::size_t base =

                    static_cast<std::size_t>(row) * static_cast<std::size_t>(columns);

                for (int column = 0; column < columns; ++column) {

                    const std::size_t visibleIndex = base + static_cast<std::size_t>(column);

                    if (visibleIndex >= g_visiblePlugs.size()) {

                        break;

                    }

                    if (column != 0) {

                        ImGui::SameLine(0.0F, gap);

                    }

                    const std::size_t plugIndex = g_visiblePlugs[visibleIndex];

                    if (draw_plug_card(g_plugs[plugIndex],

                                       plugIndex,

                                       plugIndex == g_selectedPlug,

                                       cardWidth)) {

                        g_selectedPlug = plugIndex;

                        stage_plug(weapon,

                                   g_selectedLane,

                                   g_plugs[plugIndex].definition.definitionIndex);

                        g_message[0] = '\0';

                    }

                }

            }

        }

    }

    ImGui::EndChild();

}

void draw_weapon_browser(WeaponRow& weapon) noexcept {

    rebuild_weapon_catalog(weapon);

    rebuild_visible_weapon_catalog();

    ImGui::TextDisabled(g_editorPage == EditorPage::armor ? "ARMOR CATALOG" : "WEAPON CATALOG");

    ImGui::SameLine();

    ImGui::TextDisabled("%s", active_slot_name());

    ImGui::SameLine();

    if (ImGui::SmallButton("Back to plugs")) {

        g_browserMode = BrowserMode::plugs;

        return;

    }

    ImGui::Spacing();

    const float width = ImGui::GetContentRegionAvail().x;

    const bool armorCatalog = g_editorPage == EditorPage::armor;

    const float filterWidth = scaled(112.0F);

    ImGui::SetNextItemWidth(armorCatalog

                                ? width

                                : (std::max)(scaled(140.0F),

                                             width - filterWidth - ImGui::GetStyle().ItemSpacing.x));

    ImGui::InputTextWithHint("##weapon_search",

                             armorCatalog

                                 ? "Search armor name or hash..."

                                 : "Search weapon name or hash...",

                             g_weaponSearch.data(),

                             g_weaponSearch.size());

    if (!armorCatalog) {

        ImGui::SameLine();

        const std::size_t activeFilters = active_weapon_rarity_count();

        std::array<char, 24> filterLabel{};

        if (activeFilters == 0) {

            (void)std::snprintf(filterLabel.data(), filterLabel.size(), "Filters");

        } else {

            (void)std::snprintf(filterLabel.data(), filterLabel.size(), "Filters (%zu)", activeFilters);

        }

        if (ImGui::Button(filterLabel.data(), ImVec2(filterWidth, 0.0F))) {

            ImGui::OpenPopup("weapon_rarity_filters");

        }

        draw_weapon_filter_popup();

    }

    rebuild_visible_weapon_catalog();

    ImGui::TextDisabled("%zu matching %s",

                        g_visibleWeaponCatalog.size(),

                        g_editorPage == EditorPage::armor ? "armor pieces" : "weapons");

    ImGui::SameLine();

    const bool canReplace = g_selectedReplacement < g_weaponCatalog.size()

                         && g_weaponCatalog[g_selectedReplacement].definition.definitionHash

                                != weapon.item.definitionHash;

    if (!canReplace) {

        ImGui::BeginDisabled();

    }

    if (ImGui::Button(g_editorPage == EditorPage::armor ? "Replace armor" : "Replace weapon",

                      ImVec2(scaled(142.0F), 0.0F))) {

        replace_selected_weapon(weapon);

    }

    if (!canReplace) {

        ImGui::EndDisabled();

    }

    ImGui::SameLine();

    ImGui::TextDisabled("Native defaults are restored first.");

    if (g_message[0] != '\0') {

        ImGui::SameLine();

        ImGui::TextDisabled("%s", g_message.data());

    }

    ImGui::Spacing();

    const float available = ImGui::GetContentRegionAvail().x;

    const int columns = (std::max)(1, static_cast<int>(available / scaled(225.0F)));

    const float gap = scaled(7.0F);

    const float cardWidth = (available - static_cast<float>(columns - 1) * gap)

                          / static_cast<float>(columns);

    const std::size_t rowCount = (g_visibleWeaponCatalog.size() + static_cast<std::size_t>(columns) - 1)

                               / static_cast<std::size_t>(columns);

    if (ImGui::BeginChild("weapon_catalog_grid",

                          ImVec2(0.0F, 0.0F),

                          ImGuiChildFlags_Borders,

                          ImGuiWindowFlags_AlwaysVerticalScrollbar)) {

        ImGuiListClipper clipper;

        clipper.Begin(static_cast<int>(rowCount), scaled(kWeaponCatalogCardHeight + 7.0F));

        while (clipper.Step()) {

            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {

                const std::size_t base = static_cast<std::size_t>(row) * static_cast<std::size_t>(columns);

                for (int column = 0; column < columns; ++column) {

                    const std::size_t visibleIndex = base + static_cast<std::size_t>(column);

                    if (visibleIndex >= g_visibleWeaponCatalog.size()) {

                        break;

                    }

                    if (column != 0) {

                        ImGui::SameLine(0.0F, gap);

                    }

                    const std::size_t choiceIndex = g_visibleWeaponCatalog[visibleIndex];

                    if (draw_weapon_choice_card(g_weaponCatalog[choiceIndex],

                                                choiceIndex,

                                                choiceIndex == g_selectedReplacement,

                                                cardWidth)) {

                        g_selectedReplacement = choiceIndex;

                    }

                }

            }

        }

    }

    ImGui::EndChild();

}

void set_editor_page(EditorPage page) noexcept {

    if (g_editorPage == page) {

        return;

    }

    g_editorPage = page;

    clear_pending_changes();

    g_browserMode = BrowserMode::plugs;

    g_selectedLane = 0;

    g_plugCategoryFilter = kPlugCategoryNone;

    g_weaponRarityFilter = 0;

    g_search.fill('\0');

    g_weaponSearch.fill('\0');

    g_message[0] = '\0';

    invalidate_plug_cache();

    invalidate_weapon_catalog();

}

[[nodiscard]] bool draw_editor_page_tab(const char* label,

                                        bool active,

                                        float width,

                                        float height,

                                        float textYOffset) noexcept {

    ImGui::PushID(label);

    const ImVec2 start = ImGui::GetCursorScreenPos();

    ImGui::InvisibleButton("editor_page_tab", ImVec2(width, height));

    const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

    const bool hovered = ImGui::IsItemHovered();

    const ImVec2 end{start.x + width, start.y + height};

    ImDrawList* draw = ImGui::GetWindowDrawList();

    draw->AddRectFilled(start,

                        end,

                        ui_color(active ? IM_COL32(76, 63, 18, 255)

                                        : (hovered ? IM_COL32(36, 42, 52, 255)

                                                   : IM_COL32(28, 33, 42, 255))),

                        scaled(6.0F));

    const ImVec2 textSize = ImGui::CalcTextSize(label);

    draw->AddText(ImVec2(start.x + (width - textSize.x) * 0.5F,

                         start.y + (height - textSize.y) * 0.5F + textYOffset),

                  ui_color(IM_COL32(238, 241, 247, 255)),

                  label);

    ImGui::PopID();

    return clicked;

}

void draw_editor_header() noexcept {

    const float tabHeight = scaled(34.0F);

    const bool weapons = g_editorPage == EditorPage::weapons;

    if (draw_editor_page_tab("WEAPONS", weapons, scaled(132.0F), tabHeight, scaled(-0.5F))) {

        set_editor_page(EditorPage::weapons);

    }

    ImGui::SameLine(0.0F, scaled(8.0F));

    const bool armor = g_editorPage == EditorPage::armor;

    if (draw_editor_page_tab("ARMOR", armor, scaled(118.0F), tabHeight, scaled(-1.5F))) {

        set_editor_page(EditorPage::armor);

    }

    ImGui::SameLine(0.0F, scaled(18.0F));

    const float warningY = ImGui::GetCursorPosY();

    ImGui::SetCursorPosY(warningY + scaled(5.0F));

    ImGui::PushTextWrapPos(ImGui::GetContentRegionMax().x);

    ImGui::TextColored(ImVec4(0.95F, 0.72F, 0.18F, 1.0F),

                       "WARNING: Do NOT inspect an item in-game while replacing it - this can freeze the game.");

    ImGui::PopTextWrapPos();

    ImGui::SetCursorPosY((std::max)(ImGui::GetCursorPosY(), warningY + tabHeight));

    ImGui::Separator();

    ImGui::Spacing();

}

} // namespace

void draw() noexcept {

    if (!state::build_data::item_definitions_ready()

        || !state::build_data::configured_item_details_ready()

        || !state::build_data::socket_plug_rules_ready()) {

        ImGui::TextWrapped("Gear Editor is waiting for item/socket build data...");

        return;

    }

    const state::AccountState account = state::account_snapshot();

    const std::size_t characterIndex = selected_character_index(account);

    if (characterIndex >= account.characterCount) {

        ImGui::TextWrapped("Select a character first.");

        return;

    }

    draw_editor_header();

    if (g_editorPage == EditorPage::weapons) {

        rebuild_weapons(account.characters[characterIndex]);

    } else {

        rebuild_armor(account.characters[characterIndex]);

    }

    WeaponRow* item = selected_item();

    if (item == nullptr) {

        ImGui::TextWrapped(g_editorPage == EditorPage::armor

                               ? "No armor instances with editable sockets were found for this slot."

                               : "No weapon instances with editable sockets were found for this slot.");

        return;

    }

    if (g_selectedLane >= item->detail.ordinarySocketCount || !editable_lane(*item, g_selectedLane)) {

        g_selectedLane = first_editable_lane(*item);

        invalidate_plug_cache();

    }

    const ImVec2 available = ImGui::GetContentRegionAvail();

    const float gap = scaled(10.0F);

    const float detailsMin = scaled(350.0F);

    const float rightMin = scaled(430.0F);

    const float detailsTarget = available.x * 0.42F;

    const float detailsMax = (std::max)(detailsMin, available.x - rightMin - gap);

    const float detailsWidth = (std::clamp)(detailsTarget, detailsMin, detailsMax);

    const float rightWidth = (std::max)(scaled(300.0F), available.x - detailsWidth - gap);

    constexpr ImGuiWindowFlags kFixedPanelFlags =

        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

    if (ImGui::BeginChild("gear_details",

                          ImVec2(detailsWidth, 0.0F),

                          ImGuiChildFlags_Borders,

                          kFixedPanelFlags)) {

        draw_weapon_details(*item);

    }

    ImGui::EndChild();

    ImGui::SameLine(0.0F, gap);

    if (ImGui::BeginChild("gear_right_column",

                          ImVec2(rightWidth, 0.0F),

                          ImGuiChildFlags_None,

                          kFixedPanelFlags)) {

        const float availableHeight = ImGui::GetContentRegionAvail().y;

        const float inventoryHeight = (std::clamp)(availableHeight * 0.42F,

                                                    scaled(300.0F),

                                                    scaled(340.0F));

        if (ImGui::BeginChild("gear_inventory",

                              ImVec2(0.0F, inventoryHeight),

                              ImGuiChildFlags_Borders,

                              kFixedPanelFlags)) {

            if (g_editorPage == EditorPage::weapons) {

                draw_inventory_panel();

            } else {

                draw_armor_inventory_panel();

            }

        }

        ImGui::EndChild();

        ImGui::Spacing();

        WeaponRow* afterSelection = selected_item();

        if (afterSelection != nullptr) {

            if (g_selectedLane >= afterSelection->detail.ordinarySocketCount

                || !editable_lane(*afterSelection, g_selectedLane)) {

                g_selectedLane = first_editable_lane(*afterSelection);

                invalidate_plug_cache();

            }

            if (ImGui::BeginChild("gear_browser",

                                  ImVec2(0.0F, 0.0F),

                                  ImGuiChildFlags_Borders,

                                  kFixedPanelFlags)) {

                if (g_browserMode == BrowserMode::weapons) {

                    draw_weapon_browser(*afterSelection);

                } else {

                    draw_plug_browser(*afterSelection);

                }

            }

            ImGui::EndChild();

        }

    }

    ImGui::EndChild();

}

} // namespace sunrise::server::ui::weapon_editor
