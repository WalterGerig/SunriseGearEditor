#include "random_bytes.h"

#include <Windows.h>

#include <bcrypt.h>
#include <limits>

namespace sunrise::middleware::crypto::random {

/** Fills a buffer with Windows system randomness. */
bool fill(std::span<std::byte> output) noexcept {
    if (output.empty() || output.size() > (std::numeric_limits<ULONG>::max)()) {
        return false;
    }
    return BCryptGenRandom(nullptr,
                           reinterpret_cast<PUCHAR>(output.data()),
                           static_cast<ULONG>(output.size()),
                           BCRYPT_USE_SYSTEM_PREFERRED_RNG)
           >= 0;
}

} // namespace sunrise::middleware::crypto::random
