#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace sunrise::middleware::crypto::hmac {

/** The widest digest any algorithm here produces. */
inline constexpr std::size_t kMaximumDigestSize = 32;

/** One finished digest and the bytes of it that are meaningful. */
struct Digest {
    std::array<std::byte, kMaximumDigestSize> bytes{};
    std::size_t size{};
};

/** Digest algorithms the peer can pick, numbered with the ids it picks them by. */
enum class Algorithm : std::uint8_t {
    sha256 = 0,
    sha1 = 2,
    murmur3 = 4,
};

/**
 * Authenticates two buffers as one message.
 * Two buffers, not one: the record skips the bytes between its header and its body.
 * @param algorithm Digest to key.
 * @param key Authentication key.
 * @param first Leading covered bytes.
 * @param second Trailing covered bytes.
 * @param output Receives the digest only on success.
 * @return True when Windows completed the operation.
 */
[[nodiscard]] bool authenticate(Algorithm algorithm,
                                std::span<const std::byte> key,
                                std::span<const std::byte> first,
                                std::span<const std::byte> second,
                                Digest& output) noexcept;

} // namespace sunrise::middleware::crypto::hmac
