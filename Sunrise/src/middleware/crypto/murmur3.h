#pragma once

#include <array>
#include <cstddef>
#include <span>

namespace sunrise::middleware::crypto::murmur3 {

/** The 128-bit variant produces 16 bytes. */
inline constexpr std::size_t kDigestSize = 16;

/** One finished digest. */
using Digest = std::array<std::byte, kDigestSize>;

/**
 * Hashes two buffers as one message with the 64-bit 128-wide variant, seed zero.
 * Two buffers, not one: callers hash a fixed block followed by a body.
 * @param first Leading bytes.
 * @param second Trailing bytes, possibly empty.
 * @param output Receives the digest.
 */
void hash(std::span<const std::byte> first,
          std::span<const std::byte> second,
          Digest& output) noexcept;

} // namespace sunrise::middleware::crypto::murmur3
