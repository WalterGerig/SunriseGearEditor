#include "hmac.h"

#include <Windows.h>

#include <algorithm>
#include <bcrypt.h>
#include <limits>
#include <vector>

#include "murmur3.h"

namespace sunrise::middleware::crypto::hmac {

namespace {

/** SHA-1 produces this many bytes. */
constexpr std::size_t kSha1DigestSize = 20;
/** The hand-built construction pads the key to one block. */
constexpr std::size_t kPadBlockSize = 64;
/** Byte the inner pad is built with. */
constexpr std::byte kInnerPad{0x36};
/** Byte the outer pad is built with. */
constexpr std::byte kOuterPad{0x5C};

/**
 * Authenticates with the hand-built construction the peer uses over a one-shot digest.
 * The key must fit one block. It is zero padded, never hashed down.
 * @param key Authentication key.
 * @param first Leading covered bytes.
 * @param second Trailing covered bytes.
 * @param output Receives the digest.
 */
void authenticate_murmur3(std::span<const std::byte> key,
                          std::span<const std::byte> first,
                          std::span<const std::byte> second,
                          Digest& output) noexcept {
    std::array<std::byte, kPadBlockSize> inner{};
    std::array<std::byte, kPadBlockSize> outer{};
    std::copy(key.begin(), key.end(), inner.begin());
    std::copy(key.begin(), key.end(), outer.begin());
    for (std::size_t index = 0; index < kPadBlockSize; ++index) {
        inner[index] ^= kInnerPad;
        outer[index] ^= kOuterPad;
    }

    std::vector<std::byte> body;
    body.reserve(inner.size() + first.size() + second.size());
    body.insert(body.end(), inner.begin(), inner.end());
    body.insert(body.end(), first.begin(), first.end());
    body.insert(body.end(), second.begin(), second.end());

    murmur3::Digest digest{};
    murmur3::hash(body, {}, digest);
    murmur3::Digest sealed{};
    murmur3::hash(outer, digest, sealed);

    output.size = murmur3::kDigestSize;
    std::copy(sealed.begin(), sealed.end(), output.bytes.begin());
    // The pads and the body carry the key.
    SecureZeroMemory(inner.data(), inner.size());
    SecureZeroMemory(outer.data(), outer.size());
    SecureZeroMemory(body.data(), body.size());
}

/** @return True for a BCrypt status that reports success. */
[[nodiscard]] bool succeeded(NTSTATUS status) noexcept {
    return status >= 0;
}

/** @param algorithm Selected digest. @return The Windows provider name. */
[[nodiscard]] LPCWSTR provider(Algorithm algorithm) noexcept {
    return algorithm == Algorithm::sha1 ? BCRYPT_SHA1_ALGORITHM : BCRYPT_SHA256_ALGORITHM;
}

/** @param algorithm Selected digest. @return Its digest width in bytes. */
[[nodiscard]] std::size_t digest_size(Algorithm algorithm) noexcept {
    return algorithm == Algorithm::sha1 ? kSha1DigestSize : kMaximumDigestSize;
}

/**
 * Adds one buffer to an open digest.
 * @param handle Open hash object.
 * @param part Bytes to add; an empty part is skipped.
 * @return True when the bytes fit one call and BCrypt accepted them.
 */
[[nodiscard]] bool add(BCRYPT_HASH_HANDLE handle, std::span<const std::byte> part) noexcept {
    if (part.empty()) {
        return true;
    }
    if (part.size() > (std::numeric_limits<ULONG>::max)()) {
        return false;
    }
    return succeeded(BCryptHashData(handle,
                                    reinterpret_cast<PUCHAR>(const_cast<std::byte*>(part.data())),
                                    static_cast<ULONG>(part.size()),
                                    0));
}

} // namespace

/** Authenticates two buffers as one message. */
bool authenticate(Algorithm algorithm,
                  std::span<const std::byte> key,
                  std::span<const std::byte> first,
                  std::span<const std::byte> second,
                  Digest& output) noexcept {
    if (algorithm == Algorithm::murmur3) {
        if (key.size() > kPadBlockSize) {
            return false;
        }
        authenticate_murmur3(key, first, second, output);
        return true;
    }
    BCRYPT_ALG_HANDLE opened = nullptr;
    if (!succeeded(BCryptOpenAlgorithmProvider(
            &opened, provider(algorithm), nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG))) {
        return false;
    }
    output.size = digest_size(algorithm);
    BCRYPT_HASH_HANDLE handle = nullptr;
    bool complete = false;
    if (succeeded(BCryptCreateHash(opened,
                                   &handle,
                                   nullptr,
                                   0,
                                   reinterpret_cast<PUCHAR>(const_cast<std::byte*>(key.data())),
                                   static_cast<ULONG>(key.size()),
                                   0))) {
        complete = add(handle, first) && add(handle, second)
                   && succeeded(BCryptFinishHash(handle,
                                                 reinterpret_cast<PUCHAR>(output.bytes.data()),
                                                 static_cast<ULONG>(output.size),
                                                 0));
        BCryptDestroyHash(handle);
    }
    BCryptCloseAlgorithmProvider(opened, 0);
    if (!complete) {
        output = {};
    }
    return complete;
}

} // namespace sunrise::middleware::crypto::hmac
