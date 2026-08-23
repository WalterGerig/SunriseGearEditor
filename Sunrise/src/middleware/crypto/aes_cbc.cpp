#include "aes_cbc.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <bcrypt.h>

namespace sunrise::middleware::crypto::aes {

namespace {

/** @return True for a BCrypt status that reports success. */
[[nodiscard]] bool succeeded(NTSTATUS status) noexcept {
    return status >= 0;
}

/** One opened key, closed when it leaves scope. */
class Key {
public:
    /** @param key Cypher key bytes. */
    explicit Key(std::span<const std::byte> key) noexcept {
        if (!succeeded(
                BCryptOpenAlgorithmProvider(&m_algorithm, BCRYPT_AES_ALGORITHM, nullptr, 0))) {
            m_algorithm = nullptr;
            return;
        }
        const auto* mode = reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_CBC));
        if (!succeeded(BCryptSetProperty(m_algorithm,
                                         BCRYPT_CHAINING_MODE,
                                         const_cast<PUCHAR>(mode),
                                         sizeof(BCRYPT_CHAIN_MODE_CBC),
                                         0))
            || !succeeded(BCryptGenerateSymmetricKey(
                m_algorithm,
                &m_key,
                nullptr,
                0,
                reinterpret_cast<PUCHAR>(const_cast<std::byte*>(key.data())),
                static_cast<ULONG>(key.size()),
                0))) {
            m_key = nullptr;
        }
    }

    Key(const Key&) = delete;
    Key(Key&&) = delete;
    Key& operator=(const Key&) = delete;
    Key& operator=(Key&&) = delete;

    /** Closes the key and the provider in the order BCrypt requires. */
    ~Key() noexcept {
        if (m_key != nullptr) {
            BCryptDestroyKey(m_key);
        }
        if (m_algorithm != nullptr) {
            BCryptCloseAlgorithmProvider(m_algorithm, 0);
        }
    }

    /** @return The opened key, or null when any stage failed. */
    [[nodiscard]] BCRYPT_KEY_HANDLE handle() const noexcept {
        return m_key;
    }

private:
    BCRYPT_ALG_HANDLE m_algorithm{nullptr};
    BCRYPT_KEY_HANDLE m_key{nullptr};
};

/** @return True when the buffers are a whole number of blocks and the vector is one block. */
[[nodiscard]] bool sized(std::span<const std::byte> iv,
                         std::span<const std::byte> input,
                         std::span<std::byte> output) noexcept {
    return iv.size() == kBlockSize && !input.empty() && input.size() % kBlockSize == 0
           && output.size() >= input.size();
}

} // namespace

/** Encrypts whole blocks in cipher block chaining mode. */
bool encrypt(std::span<const std::byte> key,
             std::span<const std::byte> iv,
             std::span<const std::byte> input,
             std::span<std::byte> output) noexcept {
    if (!sized(iv, input, output)) {
        return false;
    }
    const Key opened{key};
    if (opened.handle() == nullptr) {
        return false;
    }
    // BCrypt overwrites the vector it is given, so the caller's is never passed in.
    std::array<std::byte, kBlockSize> vector{};
    std::copy(iv.begin(), iv.end(), vector.begin());
    ULONG produced = 0;
    const bool complete =
        succeeded(BCryptEncrypt(opened.handle(),
                                reinterpret_cast<PUCHAR>(const_cast<std::byte*>(input.data())),
                                static_cast<ULONG>(input.size()),
                                nullptr,
                                reinterpret_cast<PUCHAR>(vector.data()),
                                static_cast<ULONG>(vector.size()),
                                reinterpret_cast<PUCHAR>(output.data()),
                                static_cast<ULONG>(input.size()),
                                &produced,
                                0))
        && produced == input.size();
    SecureZeroMemory(vector.data(), vector.size());
    return complete;
}

/** Decrypts whole blocks in cipher block chaining mode. */
bool decrypt(std::span<const std::byte> key,
             std::span<const std::byte> iv,
             std::span<const std::byte> input,
             std::span<std::byte> output) noexcept {
    if (!sized(iv, input, output)) {
        return false;
    }
    const Key opened{key};
    if (opened.handle() == nullptr) {
        return false;
    }
    std::array<std::byte, kBlockSize> vector{};
    std::copy(iv.begin(), iv.end(), vector.begin());
    ULONG produced = 0;
    const bool complete =
        succeeded(BCryptDecrypt(opened.handle(),
                                reinterpret_cast<PUCHAR>(const_cast<std::byte*>(input.data())),
                                static_cast<ULONG>(input.size()),
                                nullptr,
                                reinterpret_cast<PUCHAR>(vector.data()),
                                static_cast<ULONG>(vector.size()),
                                reinterpret_cast<PUCHAR>(output.data()),
                                static_cast<ULONG>(input.size()),
                                &produced,
                                0))
        && produced == input.size();
    SecureZeroMemory(vector.data(), vector.size());
    return complete;
}

} // namespace sunrise::middleware::crypto::aes
