#include "address_text.h"

namespace sunrise::core::settings::address {

namespace {

/** Largest value one IPv4 octet can hold. */
constexpr unsigned int kMaximumOctet = 255;
/** Longest decimal form of one octet. */
constexpr std::size_t kMaximumDigits = 3;
/** A dotted quad has 3 separators. */
constexpr std::size_t kSeparators = kOctets - 1;
/** Decimal base of the octet text. */
constexpr unsigned int kDecimalBase = 10;

/**
 * Reads one decimal octet.
 * @param text Complete dotted quad.
 * @param position Advanced past the digits that were read.
 * @param output Receives the octet value.
 * @return True when 1 to 3 digits form a value of at most 255.
 */
[[nodiscard]] bool
read_octet(std::string_view text, std::size_t& position, unsigned char& output) noexcept {
    unsigned int value = 0;
    std::size_t digits = 0;
    while (position < text.size() && text[position] >= '0' && text[position] <= '9') {
        value = value * kDecimalBase + static_cast<unsigned int>(text[position] - '0');
        ++position;
        ++digits;
        if (digits > kMaximumDigits || value > kMaximumOctet) {
            return false;
        }
    }
    if (digits == 0) {
        return false;
    }
    output = static_cast<unsigned char>(value);
    return true;
}

} // namespace

/** Splits an IPv4 dotted quad into octets. */
bool parse_ipv4(std::string_view text, std::array<unsigned char, kOctets>& output) noexcept {
    std::array<unsigned char, kOctets> octets{};
    std::size_t position = 0;
    for (std::size_t index = 0; index < octets.size(); ++index) {
        if (!read_octet(text, position, octets[index])) {
            return false;
        }
        const bool separator = index < kSeparators;
        if (separator && (position >= text.size() || text[position] != '.')) {
            return false;
        }
        position += separator ? 1 : 0;
    }
    if (position != text.size()) {
        return false;
    }
    output = octets;
    return true;
}

} // namespace sunrise::core::settings::address
