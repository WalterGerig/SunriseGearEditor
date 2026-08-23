#pragma once

#include <cstdint>

namespace sunrise::server::bap::encrypted::transactions {

/** Connection fields published only after State commits and caller output is copied. */
struct Publication {
    std::uint64_t activitySessionId{};
    bool hasActivitySessionBinding{};
    /** True when the binding came from a join rather than from this link's own allocation. */
    bool activitySessionFromJoin{};
};

} // namespace sunrise::server::bap::encrypted::transactions
