#include "association_keys.h"

#include <Windows.h>

#include <algorithm>
#include <string_view>

#include "../../crypto/tiger192.h"

namespace sunrise::middleware::gameplay::dtls {

namespace {

/** The shared secret opens the derivation buffer. */
constexpr std::size_t kSecretOffset = 0;
/** The agreed secret is one field element wide. */
constexpr std::size_t kSecretSize = 28;
/** The security key follows the secret. */
constexpr std::size_t kSecurityKeyOffset = kSecretOffset + kSecretSize;
/** The suffix closes the buffer and is zero padded to its end. */
constexpr std::size_t kSuffixOffset = kSecurityKeyOffset + kSecurityKeySize;
/** Suffix that selects the cypher key. It travels without a terminator. */
constexpr std::string_view kCypherSuffix{"cypherKeySuffix"};
/** Suffix that selects the authentication key. Its terminator is part of the buffer. */
constexpr std::string_view kAuthSuffix{"SuffixForHMACKey"};

/**
 * Builds the derivation buffer for one suffix.
 * @param sharedSecret Agreed secret.
 * @param securityKey Join key.
 * @param suffix Suffix that selects the key.
 * @param terminated True when the suffix carries its string terminator into the buffer.
 * @param output Receives the whole buffer.
 */
void build(std::span<const std::byte> sharedSecret,
           std::span<const std::byte> securityKey,
           std::string_view suffix,
           bool terminated,
           std::array<std::byte, kDerivationSize>& output) noexcept {
    output = {};
    std::copy(sharedSecret.begin(), sharedSecret.end(), output.begin() + kSecretOffset);
    std::copy(securityKey.begin(), securityKey.end(), output.begin() + kSecurityKeyOffset);
    for (std::size_t index = 0; index < suffix.size(); ++index) {
        output[kSuffixOffset + index] = static_cast<std::byte>(suffix[index]);
    }
    if (terminated) {
        output[kSuffixOffset + suffix.size()] = std::byte{0};
    }
}

} // namespace

/** Derives both association keys from the agreed secret. */
bool derive(std::span<const std::byte> sharedSecret,
            std::span<const std::byte> securityKey,
            Keys& output) noexcept {
    if (sharedSecret.size() != kSecretSize || securityKey.size() != kSecurityKeySize) {
        return false;
    }
    std::array<std::byte, kDerivationSize> buffer{};
    crypto::tiger::Digest digest{};

    build(sharedSecret, securityKey, kCypherSuffix, false, buffer);
    crypto::tiger::hash(buffer, digest);
    std::copy_n(digest.begin(), kCypherKeySize, output.cypher.begin());

    build(sharedSecret, securityKey, kAuthSuffix, true, buffer);
    crypto::tiger::hash(buffer, digest);
    std::copy_n(digest.begin(), kAuthKeySize, output.auth.begin());
    // The buffer holds the secret and the join key; the digest holds both derived keys.
    SecureZeroMemory(buffer.data(), buffer.size());
    SecureZeroMemory(digest.data(), digest.size());
    return true;
}

} // namespace sunrise::middleware::gameplay::dtls
