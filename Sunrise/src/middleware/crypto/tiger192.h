#pragma once

#include <array>
#include <cstddef>
#include <span>

namespace sunrise::middleware::crypto::tiger {

/** Tiger/192 produces 24 bytes. */
inline constexpr std::size_t kDigestSize = 24;

/** One finished Tiger/192 digest. */
using Digest = std::array<std::byte, kDigestSize>;

/**
 * Hashes one buffer with Tiger/192, three passes.
 * Windows has no provider for this hash, so it is implemented here.
 * @param input Bytes to hash.
 * @param output Receives the digest.
 */
void hash(std::span<const std::byte> input, Digest& output) noexcept;

} // namespace sunrise::middleware::crypto::tiger
