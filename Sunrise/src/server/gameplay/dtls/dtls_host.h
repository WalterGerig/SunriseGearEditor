#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "../../../state/gameplay/definition.h"

namespace sunrise::server::gameplay::dtls {

/**
 * Answers one association handshake datagram.
 * The peer opens every connection with this handshake, so nothing above it runs first.
 * @param from Source endpoint in host order.
 * @param datagram Whole received payload.
 * @param now Monotonic tick count in milliseconds.
 * @return True when the datagram belonged to this layer and needs no further routing.
 */
[[nodiscard]] bool route(const state::gameplay::Endpoint& from,
                         std::span<const std::byte> datagram,
                         std::uint64_t now) noexcept;

/**
 * Seals one transport payload as a record and sends it.
 * @param to Peer endpoint in host order.
 * @param payload Plaintext to seal.
 * @return True when an established association carried it.
 */
[[nodiscard]] bool send_payload(const state::gameplay::Endpoint& to,
                                std::span<const std::byte> payload) noexcept;

/** Drops every association and clears its key material. */
void reset() noexcept;

} // namespace sunrise::server::gameplay::dtls
