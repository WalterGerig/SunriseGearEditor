#pragma once

#include <array>
#include <cstddef>
#include <string_view>

namespace sunrise::core::settings::address {

/** Octets in one IPv4 address. */
inline constexpr std::size_t kOctets = 4;

/**
 * Splits an IPv4 dotted quad into octets.
 * A host name is refused because every consumer forwards this text as a numeric address.
 * @param text Complete address text with no surrounding space.
 * @param output Receives the octets in dotted-quad order only on success.
 * @return True when the whole text is four decimal octets separated by dots.
 */
[[nodiscard]] bool parse_ipv4(std::string_view text,
                              std::array<unsigned char, kOctets>& output) noexcept;

} // namespace sunrise::core::settings::address
