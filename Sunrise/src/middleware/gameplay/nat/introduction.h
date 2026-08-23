#pragma once

#include <cstddef>
#include <span>

namespace sunrise::middleware::gameplay::nat {

/** One introduction is 29 bytes. A datagram of any other size is not one. */
inline constexpr std::size_t kIntroductionSize = 29;

/**
 * Turns one direct introduction request into its reply, in place.
 * Only the type byte changes, so the requester gets its own token and target hash back. The
 * reply must be sent from the address the request arrived at.
 * @param datagram Received datagram, rewritten only on success.
 * @return True when the datagram was a request and now holds the reply.
 */
[[nodiscard]] bool make_introduction_reply(std::span<std::byte> datagram) noexcept;

} // namespace sunrise::middleware::gameplay::nat
