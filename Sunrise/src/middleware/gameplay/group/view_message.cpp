#include "view_message.h"

#include "../../encoding/bit_raw.h"

namespace sunrise::middleware::gameplay::group {

namespace {

namespace bits = encoding::bits;

/** The kind selector is three bits. */
constexpr std::uint8_t kKindWidth = 3;
/** Presence flags and the list flag are one bit each. */
constexpr std::uint8_t kFlagWidth = 1;
/** The optional value is a 32-bit field behind its presence bit. */
constexpr std::uint8_t kOptionalWidth = 32;
/** The list count is seven bits. */
constexpr std::uint8_t kListCountWidth = 7;
/** The session token is a 64-bit value field. */
constexpr std::uint8_t kTokenWidth = 64;

} // namespace

/** Reads a view-establishment body. */
bool read_view(bits::Reader& reader, ViewEstablishment& output) noexcept {
    ViewEstablishment candidate{};
    std::uint64_t kind = 0;
    std::uint64_t present = 0;
    if (!reader.read(kKindWidth, kind) || kind > kMaximumViewKind
        || !reader.read(kFlagWidth, present)) {
        return false;
    }
    candidate.kind = static_cast<std::uint8_t>(kind);
    candidate.hasOptionalValue = present != 0;
    if (candidate.hasOptionalValue) {
        std::uint64_t value = 0;
        if (!reader.read(kOptionalWidth, value)) {
            return false;
        }
        candidate.optionalValue = static_cast<std::int32_t>(static_cast<std::uint32_t>(value));
    }
    std::uint64_t hasList = 0;
    if (!reader.read(kFlagWidth, hasList)) {
        return false;
    }
    candidate.hasList = hasList != 0;
    if (candidate.hasList) {
        std::uint64_t count = 0;
        if (!reader.read(kListCountWidth, count) || count > kViewListCapacity) {
            return false;
        }
        candidate.listCount = static_cast<std::uint8_t>(count);
        if (!bits::read_raw(reader, {candidate.list.data(), candidate.listCount})) {
            return false;
        }
    }
    if (!reader.read(kTokenWidth, candidate.sessionToken)) {
        return false;
    }
    output = candidate;
    return true;
}

/** Writes a view-establishment body. */
bool write_view(bits::Writer& writer, const ViewEstablishment& body) noexcept {
    if (body.kind > kMaximumViewKind || body.listCount > kViewListCapacity) {
        return false;
    }
    if (!writer.write(body.kind, kKindWidth)
        || !writer.write(body.hasOptionalValue ? 1U : 0U, kFlagWidth)) {
        return false;
    }
    if (body.hasOptionalValue
        && !writer.write(static_cast<std::uint32_t>(body.optionalValue), kOptionalWidth)) {
        return false;
    }
    if (!writer.write(body.hasList ? 1U : 0U, kFlagWidth)) {
        return false;
    }
    if (body.hasList) {
        if (!writer.write(body.listCount, kListCountWidth)
            || !bits::write_raw(writer, {body.list.data(), body.listCount})) {
            return false;
        }
    }
    return writer.write(body.sessionToken, kTokenWidth);
}

/** Compares two views for replication compatibility. */
bool compatible(const ViewEstablishment& left, const ViewEstablishment& right) noexcept {
    if (left.sessionToken != right.sessionToken || left.kind != right.kind
        || left.hasList != right.hasList || left.listCount != right.listCount) {
        return false;
    }
    for (std::size_t index = 0; index < left.listCount; ++index) {
        if (left.list[index] != right.list[index]) {
            return false;
        }
    }
    return true;
}

} // namespace sunrise::middleware::gameplay::group
