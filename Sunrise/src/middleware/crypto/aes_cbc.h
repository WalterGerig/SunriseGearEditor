#pragma once

#include <cstddef>
#include <span>

namespace sunrise::middleware::crypto::aes {

/** AES works on 16-byte blocks, whatever its key width. */
inline constexpr std::size_t kBlockSize = 16;

/**
 * Encrypts whole blocks in cipher block chaining mode.
 * No padding is added. The length must already be a whole number of blocks.
 * @param key Cypher key, 16, 24 or 32 bytes.
 * @param iv Initialization vector, one block.
 * @param input Plaintext, a whole number of blocks.
 * @param output Receives the ciphertext; it may be the same buffer as the input.
 * @return True when Windows completed the operation.
 */
[[nodiscard]] bool encrypt(std::span<const std::byte> key,
                           std::span<const std::byte> iv,
                           std::span<const std::byte> input,
                           std::span<std::byte> output) noexcept;

/**
 * Decrypts whole blocks in cipher block chaining mode.
 * @param key Cypher key, 16, 24 or 32 bytes.
 * @param iv Initialization vector, one block.
 * @param input Ciphertext, a whole number of blocks.
 * @param output Receives the plaintext; it may be the same buffer as the input.
 * @return True when Windows completed the operation.
 */
[[nodiscard]] bool decrypt(std::span<const std::byte> key,
                           std::span<const std::byte> iv,
                           std::span<const std::byte> input,
                           std::span<std::byte> output) noexcept;

} // namespace sunrise::middleware::crypto::aes
