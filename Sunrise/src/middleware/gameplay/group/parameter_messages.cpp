#include "parameter_messages.h"

#include <array>
#include <cstddef>

#include "../../encoding/bit_raw.h"

namespace sunrise::middleware::gameplay::group {

namespace {

namespace bits = encoding::bits;

/** The mode flag is one bit. */
constexpr std::uint8_t kFlagWidth = 1;
/** Width of a tag-reflection root bit. Clear skips the whole field walk. */
constexpr std::uint8_t kRootBitClear = 1;
/** Width of the `activity-host` id, written most-significant-bit first. */
constexpr std::uint8_t kHostIdWidth = 64;
/** Width of the `activity-host` address. */
constexpr std::uint8_t kAddressWidth = 32;
/** Width of the `activity-host` port. */
constexpr std::uint8_t kPortWidth = 16;
/** Bits in one byte. */
constexpr unsigned kByteBits = 8;
/** Mask of one byte. */
constexpr std::uint32_t kByteMask = 0xFF;
/** Parameters one presence group covers. The encoder divides 25 by this and rounds up. */
constexpr std::uint8_t kGroupSize = 16;
/** Groups the 25 parameters fall into, and the width of the presence field that names them. */
constexpr std::uint8_t kGroupCount = 2;
/** Only the low 25 bits of either mask name a parameter. */
constexpr std::uint64_t kParameterMaskBits = 0x1FFFFFF;

/**
 * Reduces one parameter mask to the presence bits the encoder writes ahead of it.
 * @param mask Parameter mask.
 * @return Bit per group, set when that group holds any named parameter.
 */
[[nodiscard]] std::uint64_t group_presence(std::uint64_t mask) noexcept {
    std::uint64_t groups = 0;
    for (std::uint8_t group = 0; group < kGroupCount; ++group) {
        const std::uint64_t span = (mask >> (group * kGroupSize)) & ((1ULL << kGroupSize) - 1);
        if ((span & kParameterMaskBits) != 0) {
            groups |= 1ULL << group;
        }
    }
    return groups;
}

/**
 * Writes the `activity-host` body.
 * @param writer Open writer.
 * @param body Field values.
 * @return True when every field fit.
 */
[[nodiscard]] bool write_activity_host(bits::Writer& writer,
                                       const ActivityHostParameter& body) noexcept {
    std::array<std::byte, sizeof(std::uint32_t)> mask{};
    for (std::size_t index = 0; index < mask.size(); ++index) {
        const unsigned shift = static_cast<unsigned>(index) * kByteBits;
        mask[index] = static_cast<std::byte>((body.memberMask >> shift) & kByteMask);
    }
    return bits::write_raw_u64(writer, body.selectionId) && writer.write(body.hostId, kHostIdWidth)
           && bits::write_raw(writer, mask) && writer.write(body.address, kAddressWidth)
           && writer.write(body.port, kPortWidth);
}

/**
 * Writes one carried parameter body.
 * A clear root bit is a complete body for both tag-reflection parameters. It skips the field
 * walk and leaves the reader's own values in place.
 * @param writer Open writer.
 * @param parameter Registry index.
 * @param update Update being written, for the parameters that carry field values.
 * @return True when the parameter has an encoder here and its body fit.
 */
[[nodiscard]] bool write_parameter_body(bits::Writer& writer,
                                        std::uint8_t parameter,
                                        const ParameterUpdate& update) noexcept {
    if (parameter == static_cast<std::uint8_t>(Parameter::publicSessionReservations)
        || parameter == static_cast<std::uint8_t>(Parameter::currentActivity)) {
        return writer.write(0U, kRootBitClear);
    }
    if (parameter == static_cast<std::uint8_t>(Parameter::activityHost)) {
        return write_activity_host(writer, update.activityHost);
    }
    return false;
}

/**
 * Writes the per-parameter bits of one mask, in ascending index order.
 * Only parameters in a present group get a bit.
 * @param writer Open writer.
 * @param mask Parameter mask, already reduced to its meaningful bits.
 * @param groups Presence bits for that mask.
 * @param carriesBodies True for the value mask, whose named parameters carry a body.
 * @param update Update being written, for the parameters that carry field values.
 * @return True when every bit and body fit.
 */
[[nodiscard]] bool write_mask_bits(bits::Writer& writer,
                                   std::uint64_t mask,
                                   std::uint64_t groups,
                                   bool carriesBodies,
                                   const ParameterUpdate& update) noexcept {
    for (std::uint8_t parameter = 0; parameter < kParameterCount; ++parameter) {
        if ((groups & (1ULL << (parameter / kGroupSize))) == 0) {
            continue;
        }
        const bool named = ((mask >> parameter) & 1ULL) != 0;
        if (!writer.write(named ? 1U : 0U, kFlagWidth)) {
            return false;
        }
        // A body must follow its own bit. The consumer reads them interleaved, not in two runs.
        if (named && carriesBodies && !write_parameter_body(writer, parameter, update)) {
            return false;
        }
    }
    return true;
}

} // namespace

/** Reads the header of a parameter request. */
bool read_parameter_request(bits::Reader& reader, ParameterRequestHeader& output) noexcept {
    ParameterRequestHeader candidate{};
    std::uint64_t mode = 0;
    if (!bits::read_raw_u64(reader, candidate.sessionId) || !reader.read(kFlagWidth, mode)
        || !bits::read_raw_u64(reader, candidate.requestedMask)) {
        return false;
    }
    candidate.modeFlag = mode != 0;
    output = candidate;
    return true;
}

/** Writes a parameter update. */
bool write_parameter_update(bits::Writer& writer, const ParameterUpdate& body) noexcept {
    const std::uint64_t released = body.releasedMask & kParameterMaskBits;
    const std::uint64_t carried = body.carriedMask & kParameterMaskBits;
    if ((carried & ~kEncodableParameters) != 0) {
        return false;
    }
    // Field order differs from the request. The flag leads here, and the session id follows it.
    return writer.write(body.resetFlag ? 1U : 0U, kFlagWidth)
           && bits::write_raw_u64(writer, body.sessionId)
           && writer.write(group_presence(released), kGroupCount)
           && writer.write(group_presence(carried), kGroupCount)
           && write_mask_bits(writer, released, group_presence(released), false, body)
           && write_mask_bits(writer, carried, group_presence(carried), true, body);
}

/** Reports whether one parameter was requested. */
bool requests(std::uint64_t mask, std::uint8_t parameter) noexcept {
    if (parameter >= kParameterCount) {
        return false;
    }
    return ((mask >> parameter) & 1U) != 0;
}

} // namespace sunrise::middleware::gameplay::group
