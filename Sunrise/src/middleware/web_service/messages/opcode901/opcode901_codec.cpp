/**
 * Opcode 901 is a vendor purchase. The request carries a vendor index, a sale index and a clock.
 * The clock rule checks skew and freshness only. Replay needs a committed purchase to compare
 * against, and no purchase commits yet.
 */

#include "opcode901_codec.h"

#include "../../../encoding/bit_reader.h"

namespace sunrise::middleware::web_service::messages::opcode901 {
namespace {

/** Both index fields are 16-bit signed values. */
constexpr std::uint8_t kIndexWidth = 16;
/** Their descriptor bias is the signed 16-bit midpoint. */
constexpr std::int32_t kIndexBias = 0x8000;
/** The optional clock is a 64-bit signed value with no bias. */
constexpr std::uint8_t kClockWidth = 64;
/** One presence bit precedes the clock. */
constexpr std::uint8_t kPresenceWidth = 1;
/**
 * Bits allowed after the last field. Both legal forms end mid byte, so up to seven bits pad it.
 * A whole byte left over is data, not padding.
 */
constexpr std::size_t kPaddingLimit = 8;

/**
 * Reads one biased index field.
 * @param reader Open reader.
 * @param output Receives the logical index.
 * @return True when the field was present.
 */
[[nodiscard]] bool read_index(encoding::bits::Reader& reader, std::int16_t& output) noexcept {
    std::uint64_t stored = 0;
    if (!reader.read(kIndexWidth, stored)) {
        return false;
    }
    output = static_cast<std::int16_t>(static_cast<std::int32_t>(stored) - kIndexBias);
    return true;
}

} // namespace

/** Decodes one purchase request body. */
bool parse_request(const Message& message, Request& output) noexcept {
    if (message.opcode != kOpcode) {
        return false;
    }
    encoding::bits::Reader reader(message.payload);
    Request candidate{};
    std::uint64_t present = 0;
    if (!read_index(reader, candidate.vendorIndex) || !read_index(reader, candidate.saleIndex)
        || !reader.read(kPresenceWidth, present)) {
        return false;
    }
    candidate.hasClock = present != 0;
    if (candidate.hasClock) {
        std::uint64_t clock = 0;
        if (!reader.read(kClockWidth, clock)) {
            return false;
        }
        candidate.clock = static_cast<std::int64_t>(clock);
    }
    if (reader.remaining_bits() >= kPaddingLimit) {
        return false;
    }
    output = candidate;
    return true;
}

/** Checks presence first, then both windows. */
ClockPolicy check_clock(const Request& request, std::int64_t serverClock) noexcept {
    if (!request.hasClock) {
        return ClockPolicy::absent;
    }
    // Unsigned subtraction keeps the distance exact for any two signed clocks.
    if (request.clock > serverClock) {
        const std::uint64_t ahead =
            static_cast<std::uint64_t>(request.clock) - static_cast<std::uint64_t>(serverClock);
        return ahead > kClockAheadLimitSeconds ? ClockPolicy::ahead : ClockPolicy::accepted;
    }
    const std::uint64_t behind =
        static_cast<std::uint64_t>(serverClock) - static_cast<std::uint64_t>(request.clock);
    return behind > kClockBehindLimitSeconds ? ClockPolicy::stale : ClockPolicy::accepted;
}

/** Names one clock verdict for a log line. */
const char* clock_policy_name(ClockPolicy policy) noexcept {
    switch (policy) {
    case ClockPolicy::accepted:
        return "ok";
    case ClockPolicy::absent:
        return "absent";
    case ClockPolicy::ahead:
        return "ahead";
    case ClockPolicy::stale:
        return "stale";
    }
    return "unknown";
}

} // namespace sunrise::middleware::web_service::messages::opcode901
