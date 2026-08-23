#include "internal.h"

namespace sunrise::state::activity::membership::transactions {

/** Merges and applies one sparse authoritative operation. */
bool commit_authoritative(ActivityState& state,
                          SessionRecord& record,
                          const PendingMutation& prepared) noexcept {
    if (!equal(prepared.authoritativeInput, prepared.authoritativeGuard)) {
        return false;
    }

    // The merge decides the outcome. The prepared plan is not compared against it. State moves
    // between prepare and commit, and refusing on that difference dropped the whole delta: no
    // revision advanced and the region never moved.
    MembershipState merged = merge(record.membership, prepared.authoritativeInput);
    const bool changed = !equal_authoritative(record.membership, merged);
    const bool movesRegion = moves_region(record.membership, merged);
    const bool publishes = changed || movesRegion;
    const bool revisionExhausted = state.stateRevision == activity::kMaximumRevision
                                   || (record.membership.hasIdentity
                                       && record.membership.revision == kMaximumMembershipRevision);
    if (publishes && revisionExhausted) {
        return false;
    }
    if (!publishes) {
        return true;
    }

    // A region move advances the revision too. The citizen advertisement is rebuilt from the
    // merged region, and the client applies one update per revision.
    if (record.membership.hasIdentity) {
        ++merged.revision;
        merged.acknowledgedRevision = kAbsentRevision;
    }
    record.membership = merged;
    publish_change(state, record);
    return true;
}

} // namespace sunrise::state::activity::membership::transactions
