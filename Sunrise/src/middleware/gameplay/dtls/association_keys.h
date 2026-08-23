#pragma once

#include <array>
#include <cstddef>
#include <span>

namespace sunrise::middleware::gameplay::dtls {

/** The security key the derivation mixes in. It is the join key the descriptor advertises. */
inline constexpr std::size_t kSecurityKeySize = 16;
/** The derivation buffer holds the shared secret, the security key, and a zero-padded suffix. */
inline constexpr std::size_t kDerivationSize = 68;
/** The record cypher key width. The digest is truncated to it. */
inline constexpr std::size_t kCypherKeySize = 16;
/** The record authentication takes the whole digest. */
inline constexpr std::size_t kAuthKeySize = 24;

/** The two keys one association derives. */
struct Keys {
    /** Key the record cypher takes. */
    std::array<std::byte, kCypherKeySize> cypher{};
    /** Key the record authentication takes. */
    std::array<std::byte, kAuthKeySize> auth{};
};

/**
 * Derives both association keys from the agreed secret.
 * @param sharedSecret Agreed x coordinate, high byte first.
 * @param securityKey Join key the descriptor advertised.
 * @param output Receives both keys only on success.
 * @return True when the inputs are the widths the derivation requires.
 */
[[nodiscard]] bool derive(std::span<const std::byte> sharedSecret,
                          std::span<const std::byte> securityKey,
                          Keys& output) noexcept;

} // namespace sunrise::middleware::gameplay::dtls
