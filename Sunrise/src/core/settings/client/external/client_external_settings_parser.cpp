#include <cstddef>
#include <string_view>

#include "../../address_text.h"
#include "../../parser.h"

namespace sunrise::core::settings::parser {
namespace {

namespace external = client::external;

/** Space is the first printable ASCII value accepted by external-server text. */
constexpr unsigned int kMinimumPrintableAscii = 0x20;
/** Tilde is the last printable ASCII value accepted by external-server text. */
constexpr unsigned int kMaximumPrintableAscii = 0x7E;
/** The Client compares all 37 token bytes, so a short GUID never matches. */
constexpr std::size_t kConfigGuidLength = external::kConfigGuidCapacity - 1;

/**
 * Copies printable ASCII into fixed storage.
 * JSON escapes are refused, not decoded: no supported value needs one.
 * @param encoded Borrowed JSON string.
 * @param output Receives the text and a trailing null only on success.
 * @return True for 1 to Capacity-1 printable ASCII bytes.
 */
template <std::size_t Capacity>
[[nodiscard]] bool decode_text(std::string_view encoded,
                               std::array<char, Capacity>& output) noexcept {
    if (encoded.empty() || encoded.size() >= Capacity) {
        return false;
    }
    std::array<char, Capacity> candidate{};
    for (std::size_t index = 0; index < encoded.size(); ++index) {
        const auto byte = static_cast<unsigned char>(encoded[index]);
        if (byte < kMinimumPrintableAscii || byte > kMaximumPrintableAscii || byte == '\\') {
            return false;
        }
        candidate[index] = encoded[index];
    }
    output = candidate;
    return true;
}

/**
 * Derives the octets and the wide copy from the host text.
 * A name is refused because the resolver forwards this text with AI_NUMERICHOST.
 * @param output Host settings whose text is already filled in.
 * @return True when the host text is a complete IPv4 dotted quad.
 */
[[nodiscard]] bool derive_address(external::Settings& output) noexcept {
    const std::string_view text(output.host.data());
    if (!address::parse_ipv4(text, output.address)) {
        return false;
    }
    output.hostWide = {};
    for (std::size_t index = 0; index < text.size(); ++index) {
        output.hostWide[index] = static_cast<wchar_t>(text[index]);
    }
    return true;
}

} // namespace

/** Parses the external-server block on top of the fixed defaults. */
bool Parser::client_external_settings(external::Settings& output) noexcept {
    if (!consume('{')) {
        return false;
    }
    external::Settings candidate = output;
    bool hasEnabled = false;
    bool hasHost = false;
    bool hasConfigUrl = false;
    bool hasConfigGuid = false;
    if (consume('}')) {
        return true;
    }
    for (;;) {
        std::string_view key;
        if (!string(key) || !consume(':')) {
            return false;
        }
        if (key == "enabled") {
            if (hasEnabled || !boolean(candidate.enabled)) {
                return false;
            }
            hasEnabled = true;
        } else if (key == "host") {
            std::string_view value;
            if (hasHost || !string(value) || !decode_text(value, candidate.host)
                || !derive_address(candidate)) {
                return false;
            }
            hasHost = true;
        } else if (key == "config_url") {
            std::string_view value;
            if (hasConfigUrl || !string(value) || !decode_text(value, candidate.configUrl)) {
                return false;
            }
            hasConfigUrl = true;
        } else if (key == "config_guid") {
            std::string_view value;
            if (hasConfigGuid || !string(value) || value.size() != kConfigGuidLength
                || !decode_text(value, candidate.configGuid)) {
                return false;
            }
            hasConfigGuid = true;
        } else if (!skip_value(0)) {
            return false;
        }
        if (consume('}')) {
            output = candidate;
            return true;
        }
        if (!consume(',')) {
            return false;
        }
    }
}

} // namespace sunrise::core::settings::parser
