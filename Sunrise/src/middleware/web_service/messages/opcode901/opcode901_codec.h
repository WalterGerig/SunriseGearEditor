#pragma once

#include <cstddef>
#include <cstdint>

#include "../../web_service_envelope.h"

namespace sunrise::middleware::web_service::messages::opcode901 {

/** Web Service opcode for a vendor purchase. */
inline constexpr std::uint16_t kOpcode = 901;

/** One decoded purchase request. The two indices are its only identity. */
struct Request {
    /** Index into the vendor table. */
    std::int16_t vendorIndex{};
    /** Index into that vendor's sale rows. */
    std::int16_t saleIndex{};
    /** Client clock in Unix seconds. */
    std::int64_t clock{};
    /** False for the absent form, which the decoder still accepts. */
    bool hasClock{};
};

/**
 * Verdict of the clock rule on one decoded request.
 * Every value is Sunrise policy, not a native rule.
 */
enum class ClockPolicy : std::uint8_t {
    /** Present, and inside both windows. */
    accepted,
    /** The absent form. It decodes, but the clock rule refuses it. */
    absent,
    /** Ahead of the server by more than the skew window. */
    ahead,
    /** Behind the server by more than the freshness window. */
    stale,
};

/** Seconds a request clock may run ahead of the server. Policy window, not a native value. */
inline constexpr std::uint64_t kClockAheadLimitSeconds = 60;
/** Seconds a request clock may run behind the server. Policy window, not a native value. */
inline constexpr std::uint64_t kClockBehindLimitSeconds = 300;

/**
 * Decodes one purchase request body.
 * A body with a whole byte left over after the last field is refused.
 * @param message Parsed Web Service envelope.
 * @param output Receives the request only when the whole body decodes.
 * @return True when the opcode matches and the body is one of the two legal forms.
 */
[[nodiscard]] bool parse_request(const Message& message, Request& output) noexcept;

/**
 * Applies the clock rule to one decoded request.
 * The clock must be present. The absent form decodes, then fails here.
 * @param request Decoded request.
 * @param serverClock Server's own time in Unix seconds.
 * @return Which rule the clock met or broke.
 */
[[nodiscard]] ClockPolicy check_clock(const Request& request, std::int64_t serverClock) noexcept;

/**
 * Names one clock verdict for a log line.
 * @param policy Verdict from check_clock.
 * @return Stable lowercase token.
 */
[[nodiscard]] const char* clock_policy_name(ClockPolicy policy) noexcept;

} // namespace sunrise::middleware::web_service::messages::opcode901
