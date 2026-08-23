#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "../../../state/gameplay/definition.h"

namespace sunrise::server::gameplay::association {

/**
 * Routes one received datagram by its outer marker.
 * Marker 1 is connection control, marker 0 an established protected packet.
 * @param from Source endpoint in host order.
 * @param datagram Whole received payload.
 * @param now Monotonic tick count in milliseconds.
 */
void route(const state::gameplay::Endpoint& from,
           std::span<const std::byte> datagram,
           std::uint64_t now) noexcept;

/**
 * Seals one transport payload and sends it to an established association.
 * @param to Destination endpoint in host order.
 * @param payload Transport payload bytes.
 * @return True when the association was established and the datagram left the endpoint.
 */
[[nodiscard]] bool send_payload(const state::gameplay::Endpoint& to,
                                std::span<const std::byte> payload) noexcept;

/**
 * Tears down associations that never completed.
 * @param now Monotonic tick count in milliseconds.
 */
void expire(std::uint64_t now) noexcept;

/** Drops every association and clears its key material. */
void reset() noexcept;

} // namespace sunrise::server::gameplay::association
