#pragma once

#include <cstdint>

namespace sunrise::server::gameplay {

/**
 * Binds the gameplay endpoint for the configured topology.
 * A disabled topology succeeds without binding, so the rest of the Server is unaffected.
 * @return True when the channel is ready to be advertised, or is deliberately off.
 */
[[nodiscard]] bool initialize() noexcept;

/**
 * Runs one bounded gameplay slice.
 * @param now Monotonic tick count in milliseconds.
 */
void service(std::uint64_t now) noexcept;

/** Stops the endpoint and clears every association and peer. */
void shutdown() noexcept;

} // namespace sunrise::server::gameplay
