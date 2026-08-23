#include "activity_membership_push.h"

#include <Windows.h>

#include "../../../../../middleware/bap/activity_message/replicate_membership.h"
#include "../../../../../middleware/secure_channel/runtime.h"
#include "../../../../gameplay/gameplay_advertisement.h"
#include "activity_arrival.h"
#include "activity_notification_frame.h"

namespace sunrise::server::bap::encrypted::push::activity {
namespace {

namespace message = middleware::bap::activity_message::replicate_membership;

/** The one published member always occupies slot zero of both top-level masks. */
constexpr std::uint8_t kLocalMemberSlot = 0;

/**
 * Maps a lock-consistent State snapshot into the fixed Middleware schema.
 * @param sessionId Joined activity session, used to resolve the advertised region.
 * @param mutation Prepared membership operation, whose region this body publishes.
 * @return Whole current membership encoder input.
 */
[[nodiscard]] message::MembershipSnapshot
make_wire_snapshot(std::uint64_t sessionId,
                   const state::activity::membership::PendingMutation& mutation) noexcept {
    const state::activity::membership::Snapshot& snapshot = mutation.snapshot;
    message::MembershipSnapshot wire{};
    wire.identity.memberKey = snapshot.identity.memberKey;
    wire.identity.field1 = snapshot.identity.smallOpaque;
    wire.identity.field2 = snapshot.identity.signedOpaque;
    wire.identity.field3 = snapshot.identity.joinIdentity;
    wire.identity.accountSoid = snapshot.identity.accountSoid;
    wire.identity.field5 = snapshot.identity.opaqueSoid;
    wire.identity.field6 = snapshot.identity.secondaryOpaque;
    wire.spawn.state = snapshot.spawn.state;
    wire.spawn.opaqueByte = snapshot.spawn.opaqueByte;
    wire.spawn.opaqueValue = snapshot.spawn.opaqueValue;
    wire.teleport.state = snapshot.teleport.state;
    wire.teleport.token = snapshot.teleport.token;
    wire.teleport.sliceSetIndex = snapshot.teleport.sliceSetIndex;
    wire.teleport.sliceSetHash = snapshot.teleport.sliceSetHash;
    wire.revision = snapshot.revision;
    wire.epoch = snapshot.epoch;
    wire.transitionToken = snapshot.transitionToken;
    // The region this body is about to commit, not the one State still holds. Staging runs before
    // the commit, so the region just left would leave the pending record empty for good.
    const EffectiveRegion region = planned_region(mutation, sessionId);
    server::gameplay::build_advertisement(region.index,
                                          region.reported ? server::gameplay::RegionSource::reported
                                                          : server::gameplay::RegionSource::arrival,
                                          kLocalMemberSlot,
                                          wire.citizen);
    return wire;
}

} // namespace

/** Appends one current membership svc9 notification and advances its local nonce. */
bool append_membership_notification(Scratch& scratch,
                                    const activity_message::ActivityPlan& activity,
                                    std::span<const std::byte, state::kAesKeySize> key,
                                    std::array<std::byte, state::kBapNonceSize>& nonce,
                                    std::span<std::byte> response,
                                    std::size_t& written) noexcept {
    if (written > response.size() || !activity.membershipMutation.hasSnapshot) {
        return false;
    }

    const std::size_t initialWritten = written;
    auto initialNonce = nonce;
    std::size_t messageSize = 0;
    const message::MembershipSnapshot snapshot =
        make_wire_snapshot(activity.sessionId, activity.membershipMutation);
    const bool encoded =
        message::encode_replicate_membership(snapshot, scratch.responseBody, messageSize)
        && append_notification_frame(scratch,
                                     activity.sessionId,
                                     message::kMessageType,
                                     std::span(scratch.responseBody).first(messageSize),
                                     key,
                                     nonce,
                                     response,
                                     written);
    SecureZeroMemory(scratch.responseBody.data(), message::encoded_size(snapshot));
    if (encoded) {
        middleware::secure_channel::advance_nonce(nonce);
    } else {
        if (written > initialWritten) {
            SecureZeroMemory(response.data() + initialWritten, written - initialWritten);
        }
        written = initialWritten;
        nonce = initialNonce;
    }
    SecureZeroMemory(&initialNonce, sizeof initialNonce);
    return encoded;
}

} // namespace sunrise::server::bap::encrypted::push::activity
