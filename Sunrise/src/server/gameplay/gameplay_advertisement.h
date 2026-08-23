#pragma once

#include <cstdint>

#include "../../middleware/bap/activity_message/replicate_membership.h"

namespace sunrise::server::gameplay {

/** Where an advertised region index came from. Reported so a stand-in cannot look like a report. */
enum class RegionSource : std::uint8_t {
    /** The client reported the region it is in. */
    reported,
    /** No report has arrived, so the destination's arrival slice set stood in. */
    arrival,
};

/**
 * Builds the citizen advertisement for one region record.
 * It stays empty unless the endpoint is bound and the region index names a real record, so a
 * disabled or unbound channel publishes exactly the membership body it published before.
 * @param regionIndex Region the roster publishes for this session.
 * @param regionSource Where that index came from.
 * @param localMemberSlot Member slot of the joining client.
 * @param output Cleared, then filled when the channel can be advertised.
 */
void build_advertisement(
    std::int32_t regionIndex,
    RegionSource regionSource,
    std::uint8_t localMemberSlot,
    middleware::bap::activity_message::replicate_membership::CitizenAdvertisement& output) noexcept;

/** Whether one region's advertisement can be built now. */
enum class AdvertisementState : std::uint8_t {
    /** It builds, so a push carries the descriptor and the host session. */
    ready,
    /** Only the region's activity host session is missing, and the next service slice fills it. */
    pending,
    /** The channel advertises nothing at all, so the published body is unchanged either way. */
    absent,
};

/**
 * Reports whether one region's advertisement can be built now.
 * The client applies one membership update per revision. Publishing during `pending` spends that
 * revision on a region record with no descriptor, so a publisher must hold instead.
 * @param regionIndex Region the roster publishes for this session.
 * @return Which of the three states the advertisement is in.
 */
[[nodiscard]] AdvertisementState advertisement_state(std::int32_t regionIndex) noexcept;

} // namespace sunrise::server::gameplay
