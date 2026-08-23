#include "aes_gcm_encrypt.h"

#include <Windows.h>

#include <bcrypt.h>
#include <limits>

namespace sunrise::middleware::crypto::aes_gcm {

/** Encrypts one buffer and returns its tag apart from the ciphertext. */
bool encrypt(std::span<const std::byte, kKeySize> key,
             std::span<const std::byte, kNonceSize> nonce,
             std::span<const std::byte> plaintext,
             std::span<std::byte> output,
             std::span<std::byte, kTagSize> tag) noexcept {
    if (plaintext.size() > (std::numeric_limits<ULONG>::max)()
        || output.size() < plaintext.size()) {
        return false;
    }
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_AES_ALGORITHM, nullptr, 0) < 0) {
        return false;
    }
    BCRYPT_KEY_HANDLE symmetricKey = nullptr;
    bool sealed = false;
    if (BCryptSetProperty(algorithm,
                          BCRYPT_CHAINING_MODE,
                          reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_GCM)),
                          sizeof(BCRYPT_CHAIN_MODE_GCM),
                          0)
            >= 0
        && BCryptGenerateSymmetricKey(algorithm,
                                      &symmetricKey,
                                      nullptr,
                                      0,
                                      reinterpret_cast<PUCHAR>(const_cast<std::byte*>(key.data())),
                                      static_cast<ULONG>(key.size()),
                                      0)
               >= 0) {
        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authentication;
        BCRYPT_INIT_AUTH_MODE_INFO(authentication);
        authentication.pbNonce = reinterpret_cast<PUCHAR>(const_cast<std::byte*>(nonce.data()));
        authentication.cbNonce = static_cast<ULONG>(nonce.size());
        authentication.pbTag = reinterpret_cast<PUCHAR>(tag.data());
        authentication.cbTag = static_cast<ULONG>(tag.size());
        ULONG produced = 0;
        sealed = BCryptEncrypt(symmetricKey,
                               reinterpret_cast<PUCHAR>(const_cast<std::byte*>(plaintext.data())),
                               static_cast<ULONG>(plaintext.size()),
                               &authentication,
                               nullptr,
                               0,
                               reinterpret_cast<PUCHAR>(output.data()),
                               static_cast<ULONG>(output.size()),
                               &produced,
                               0)
                     >= 0
                 && produced == plaintext.size();
        BCryptDestroyKey(symmetricKey);
    }
    BCryptCloseAlgorithmProvider(algorithm, 0);
    return sealed;
}

} // namespace sunrise::middleware::crypto::aes_gcm
