#pragma once

#include <array>
#include <cstddef>

#include "../../crypto/modular_1024.h"
#include "../../crypto/sha256.h"

namespace sunrise::middleware::gameplay::association::srp {

/** Every exchanged integer is 128 bytes, big-endian and left zero padded. */
inline constexpr std::size_t kIntegerSize = crypto::modular::kByteSize;
/** The derived key is one SHA-256 digest. */
inline constexpr std::size_t kDerivedKeySize = crypto::sha256::kDigestSize;

/** One fixed-width exchanged integer. */
using Integer = std::array<std::byte, kIntegerSize>;

/** One server-side exchange, filled in from the offer and answered in the response. */
struct Exchange {
    /** Verifier the offer carries as its identity public field. */
    Integer verifier{};
    /** Public value the offer carries as its last integer field. */
    Integer clientPublic{};
    /** Public value the response returns. Valid only once the exchange completes. */
    Integer serverPublic{};
    /** SHA-256 of the shared value. Its first 16 bytes key the protected envelope. */
    std::array<std::byte, kDerivedKeySize> derivedKey{};
    bool complete{};
};

/**
 * Runs the server half of the exchange.
 * The private exponent stays inside this call and is erased before it returns.
 * A public value of zero, or one at or above the modulus, is refused. That bound is our policy.
 * @param exchange Verifier and client public in; server public and derived key out.
 * @return True when both inputs are in range and every stage of the arithmetic succeeded.
 */
[[nodiscard]] bool derive(Exchange& exchange) noexcept;

} // namespace sunrise::middleware::gameplay::association::srp
