#pragma once

#include "../../client/network/consumer.h"

namespace sunrise::server::bap {

/** Applies one connection-scoped BAP lifecycle event. */
[[nodiscard]] bool consume(const client::network::BapRequest& request,
                           client::network::BapResponse& response) noexcept;

/**
 * Arms a fresh account-graph push for every authenticated Family-4 session.
 * Used by in-process tools that mutate account State outside a client request transaction.
 * @return True when at least one active session was armed.
 */
[[nodiscard]] bool request_account_resync() noexcept;

/** Wipes every connection-owned nonce and transform buffer. */
void shutdown() noexcept;

} // namespace sunrise::server::bap
