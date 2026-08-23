#include <Windows.h>

#include <algorithm>
#include <string_view>

#include "../../../../../state/account/account_state.h"
#include "../../../../../state/activity/defaults/activity_defaults_snapshot.h"
#include "../../../../../state/activity/destination/activity_destination_snapshot.h"
#include "../../../../../state/activity/destination/activity_destination_spawn_binding.h"
#include "../../../../../state/activity/membership/activity_membership_query.h"
#include "../../../../../state/build_data/runtime.h"
#include "../../../../../state/runtime/runtime.h"
#include "activity_arrival.h"
#include "internal.h"

namespace sunrise::server::bap::encrypted::push::activity {
namespace {

namespace layouts = state::build_data::scenarios;

/**
 * The type-17 lifetime state the roster reports.
 * Only 3, 6 and 10 are safe: spawn gate G4 indexes a jump table with no bounds check, so any other
 * value is a wild jump, not a refusal. 3 is what a live activity measured.
 */
constexpr std::uint8_t kLifetimeState = 3;
/** Sends whose state byte moves regardless. A body absorbed while the world loads needs them. */
constexpr std::uint8_t kWarmupSends = 3;
/** The state byte is stored biased into one signed byte, so the sequence stays inside this. */
constexpr std::uint8_t kStateSequenceWrap = 128;
/** Standard 32-bit FNV-1a basis and prime fold the group set into one comparable value. */
constexpr std::uint32_t kFoldBasis = 2166136261U;
constexpr std::uint32_t kFoldPrime = 16777619U;
/** Only a type-13 slot binds the player, so only a group holding one may carry the key. */
constexpr std::uint8_t kSlotTypeParticipation = 13;
/** The join request names its character in the low half of the SOID, so compare on that half. */
constexpr std::uint64_t kIdentityLowMask = 0xFFFFFFFFULL;

/**
 * Finds the full authored SOID for the character the join request named.
 * The client sends a short identity form. Publishing that form binds no object, so the full SOID
 * goes out instead.
 * @param joinCharacter Character id the join request carried, or zero when it carried none.
 * @return Authored SOID of the named character, or of the selected character when nothing matches.
 */
[[nodiscard]] std::uint64_t roster_player_key(std::uint64_t joinCharacter) noexcept {
    const state::AccountState account = state::account_snapshot();
    const std::uint64_t selected = state::account::selected_character_soid(account);
    if (joinCharacter == 0) {
        return selected;
    }
    for (std::size_t index = 0; index < account.characterCount; ++index) {
        const std::uint64_t soid = account.characters[index].soid;
        if ((soid & kIdentityLowMask) == (joinCharacter & kIdentityLowMask)) {
            return soid;
        }
    }
    return selected;
}

/**
 * Copies the destination's published groups into the encoder's fixed input.
 * @param layout Destination row naming its groups by roster table index.
 * @param scratch Lock-owned roster group storage the spans point into.
 * @param roster Receives the groups and the group that binds the player.
 * @return True when every named group was found and one of them binds the player.
 */
[[nodiscard]] bool
fill_roster(const layouts::Definition& layout, Scratch& scratch, message::Roster& roster) noexcept {
    roster = {};
    if (layout.rosterGroupCount == 0 || layout.rosterGroupCount > scratch.rosterGroups.size()
        || layout.rosterGroupCount > roster.groups.size()) {
        return false;
    }
    for (std::size_t index = 0; index < layout.rosterGroupCount; ++index) {
        layouts::RosterGroup& group = scratch.rosterGroups[index];
        if (!state::build_data::find_roster_group(layout.rosterGroups[index], group)) {
            return false;
        }
        roster.groups[index].key = group.registryKey;
        roster.groups[index].slotTypes =
            std::span<const std::uint8_t>(group.slotTypes.data(), group.slotCount);
        roster.groups[index].slotFlags =
            std::span<const std::uint8_t>(group.slotFlags.data(), group.slotCount);
    }
    roster.groupCount = layout.rosterGroupCount;
    for (std::size_t index = 0; index < roster.groupCount && roster.playerKeyGroup == 0; ++index) {
        const layouts::RosterGroup& group = scratch.rosterGroups[index];
        for (std::size_t slot = 0; slot < group.slotCount; ++slot) {
            if (group.slotTypes[slot] == kSlotTypeParticipation) {
                roster.playerKeyGroup = group.registryKey;
                break;
            }
        }
    }
    return roster.playerKeyGroup != 0;
}

/** @param roster Published groups. @return One value that changes when the group set changes. */
[[nodiscard]] std::uint32_t fold_groups(const message::Roster& roster) noexcept {
    std::uint32_t folded = kFoldBasis;
    for (std::size_t index = 0; index < roster.groupCount; ++index) {
        folded = (folded ^ roster.groups[index].key) * kFoldPrime;
    }
    return folded;
}

/**
 * Picks the per-entry state byte and advances the connection's counters.
 * A burst send leaves the byte and the latched group set alone past the warm-up, so a group change
 * during a load is published by the next keepalive send instead.
 * @param session Connection-owned roster counters.
 * @param folded Current group set.
 * @param burst True for a send on the loading cadence.
 * @return The state byte to send.
 */
[[nodiscard]] std::uint8_t
next_state_sequence(Session& session, std::uint32_t folded, bool burst) noexcept {
    if (session.activityRosterSends < kWarmupSends
        || (!burst && session.activityRosterGroups != folded)) {
        session.activityRosterState =
            static_cast<std::uint8_t>((session.activityRosterState + 1) % kStateSequenceWrap);
        session.activityRosterGroups = folded;
    }
    if (session.activityRosterSends < kWarmupSends) {
        ++session.activityRosterSends;
    }
    return session.activityRosterState;
}

} // namespace

/** Resolves the one region a session publishes. */
EffectiveRegion effective_region(std::uint64_t sessionId) noexcept {
    state::activity::defaults::ActivityDefaults defaults{};
    state::activity::destination::DestinationSelection selection{};
    state::activity::defaults::snapshot(defaults);
    // The session's own destination wins. The authored default covers a session that committed
    // before any selection was readable.
    if (!state::activity::destination::snapshot(sessionId, selection)) {
        selection = defaults.defaultDestination.selection;
    }
    const std::string_view name(reinterpret_cast<const char*>(selection.packageName.data()),
                                selection.packageNameLength);
    // A missing layout leaves a cleared definition, and the arrival rule then returns the
    // authored fallback index.
    layouts::Definition layout{};
    static_cast<void>(state::build_data::find_scenario_layout(name, layout));
    EffectiveRegion region{};
    region.arrival = arrival_slice_set(defaults.defaultDestination, selection, name, layout);
    const std::int32_t reported = state::activity::membership::reported_region(sessionId);
    region.reported = reported >= 0;
    region.index = region.reported ? reported : static_cast<std::int32_t>(region.arrival);
    return region;
}

/** Resolves the region one prepared membership body publishes. */
EffectiveRegion planned_region(const state::activity::membership::PendingMutation& mutation,
                               std::uint64_t sessionId) noexcept {
    EffectiveRegion region = effective_region(sessionId);
    // The same rule the State merge uses, so the body and the record it will commit agree. A
    // negative index is the unset value the client sends on its way out, not a position.
    if (mutation.authoritativeInput.hasRegion
        && mutation.authoritativeInput.region.index
               > state::activity::membership::kAbsentRegionIndex) {
        region.index = mutation.authoritativeInput.region.index;
        region.reported = true;
    }
    return region;
}

/** Builds the roster body input for one session's current destination. */
RosterOutcome build_roster_snapshot(Session& session,
                                    Scratch& scratch,
                                    message::Snapshot& snapshot,
                                    std::span<char> destination,
                                    std::size_t& destinationLength,
                                    bool burst) noexcept {
    snapshot = {};
    destinationLength = 0;
    state::activity::defaults::ActivityDefaults defaults{};
    state::activity::destination::DestinationSelection selection{};
    state::activity::defaults::snapshot(defaults);
    // A joined link takes its region from the link that knows where the player is, so its bubble
    // grant names the bubble the world is in. Its roster groups stay its own: the client-reference
    // table holds one record per key, so two containers sharing a key orphan each other's rows.
    const std::uint64_t regionFrom =
        session.activityJoinedForeignSession
            ? state::activity::membership::live_region_session(session.activitySessionId)
            : session.activitySessionId;
    // The session's own destination wins. The authored default covers a session that committed
    // before any selection was readable.
    if (!state::activity::destination::snapshot(session.activitySessionId, selection)) {
        selection = defaults.defaultDestination.selection;
    }
    layouts::Definition layout{};
    const std::string_view name(reinterpret_cast<const char*>(selection.packageName.data()),
                                selection.packageNameLength);
    destinationLength = (std::min)(name.size(), destination.size());
    std::copy_n(name.begin(), destinationLength, destination.begin());
    if (!state::build_data::find_scenario_layout(name, layout)) {
        return RosterOutcome::noLayout;
    }
    if (!fill_roster(layout, scratch, snapshot.roster)) {
        return RosterOutcome::noGroups;
    }

    const state::activity::defaults::FallbackPolicy& fallback =
        defaults.defaultDestination.fallback;
    // One resolution serves this body and the citizen advertisement in message 12. Two would let
    // the join descriptor land in a region record the client is not pending on.
    const EffectiveRegion region = effective_region(regionFrom);
    snapshot.patchEpoch = session.activityPatchEpoch;
    // The character the join named wins, resolved to its authored SOID. The client binds its
    // player by matching this value against the object registry, and the short form the join
    // carries matches nothing.
    snapshot.playerKey = roster_player_key(session.activityCharacterSoid);
    // The old encoder documents this key as message 12's member record `+16` while its own code
    // sends the character SOID. That field is the membership identity, so this sends it instead.
    if (defaults.rosterKeyFromIdentity) {
        const std::uint64_t identity =
            state::activity::membership::join_identity(session.activitySessionId);
        if (identity != 0) {
            snapshot.playerKey = identity;
        }
    }
    snapshot.lifetime = kLifetimeState;
    snapshot.keyOnEveryParticipationSlot = defaults.rosterKeyOnAllSlots;
    // The participation record's `+0` latches only when the region index is known.
    snapshot.region = static_cast<std::uint32_t>(region.index);
    snapshot.hasRegion = true;
    // The spawn override always names the destination's own arrival, never the player's position.
    snapshot.spawnSliceSet = region.arrival;
    snapshot.spawnSetHash =
        state::activity::destination::attachable_spawn_set_hash(selection, fallback.spawnSetHash);
    snapshot.hasSpawnOverride =
        snapshot.spawnSetHash != 0 && snapshot.spawnSetHash != message::kAbsentSpawnSetHash;
    snapshot.stateSequence = next_state_sequence(session, fold_groups(snapshot.roster), burst);
    return RosterOutcome::published;
}

} // namespace sunrise::server::bap::encrypted::push::activity
