#pragma once

#include <cstddef>
#include <span>

#include "aes_gcm_decrypt.h"

namespace sunrise::middleware::crypto::aes_gcm {

/**
 * Encrypts one buffer and returns its tag apart from the ciphertext.
 * There is no additional authenticated data. The tag covers the ciphertext only.
 * @param key AES-128 key.
 * @param nonce 12-byte nonce.
 * @param plaintext Bytes to encrypt.
 * @param output Destination of at least the plaintext size.
 * @param tag Receives the full 16-byte authentication tag.
 * @return True only when the whole transform succeeds.
 */
[[nodiscard]] bool encrypt(std::span<const std::byte, kKeySize> key,
                           std::span<const std::byte, kNonceSize> nonce,
                           std::span<const std::byte> plaintext,
                           std::span<std::byte> output,
                           std::span<std::byte, kTagSize> tag) noexcept;

} // namespace sunrise::middleware::crypto::aes_gcm
