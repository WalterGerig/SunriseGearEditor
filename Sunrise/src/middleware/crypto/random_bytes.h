#pragma once

#include <cstddef>
#include <span>

namespace sunrise::middleware::crypto::random {

/**
 * Fills a buffer with Windows system randomness.
 * Every ephemeral secret Sunrise owns comes from here, never from an authored default.
 * @param output Storage to overwrite completely.
 * @return True when Windows produced every byte.
 */
[[nodiscard]] bool fill(std::span<std::byte> output) noexcept;

} // namespace sunrise::middleware::crypto::random
