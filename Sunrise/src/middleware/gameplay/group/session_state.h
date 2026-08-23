#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "session_messages.h"

namespace sunrise::middleware::gameplay::group {

/** Bytes of group-session state the membership hash covers, and the whole struct's size. */
inline constexpr std::size_t kSessionStateSize = 28768;

/** One replica of that state. It is large, so it belongs in static or member storage. */
using SessionState = std::array<std::byte, kSessionStateSize>;

/**
 * Fills a replica of the session state a peer holds after applying one complete snapshot.
 * The peer clears its member and player tables first, so every byte follows from the message.
 * @param body Snapshot the peer will apply.
 * @param output Receives the replica, fully overwritten.
 */
void build_session_state(const MembershipUpdate& body, SessionState& output) noexcept;

/**
 * Computes the state hash a peer will expect for one complete snapshot.
 * @param body Snapshot the peer will apply.
 * @return The hash to publish in the message tail.
 */
[[nodiscard]] std::uint32_t session_state_hash(const MembershipUpdate& body) noexcept;

} // namespace sunrise::middleware::gameplay::group
