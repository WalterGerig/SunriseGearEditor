#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "../../encoding/bit_reader.h"
#include "../../encoding/bit_writer.h"

namespace sunrise::middleware::gameplay::group {

/** Registry id of view establishment. */
inline constexpr std::uint8_t kViewMessageId = 40;
/** Declared decoded size the registry holds. */
inline constexpr std::uint32_t kViewMessageSize = 40;
/** The list carries at most 16 bytes. */
inline constexpr std::size_t kViewListCapacity = 16;
/** The kind field is 3 bits and its largest named value is 5. */
inline constexpr std::uint8_t kMaximumViewKind = 5;

/**
 * One view-establishment body.
 * Both sides must agree on it. A mismatch produces no replicated entities at all.
 */
struct ViewEstablishment {
    std::array<std::byte, kViewListCapacity> list{};
    /** Token both sides bind. */
    std::uint64_t sessionToken{};
    /** Present optional value, or absent when the sender sent none. */
    std::int32_t optionalValue{};
    std::uint8_t kind{};
    std::uint8_t listCount{};
    bool hasOptionalValue{};
    bool hasList{};
};

/** Reads a view-establishment body. @return True when the selected form was complete. */
[[nodiscard]] bool read_view(encoding::bits::Reader& reader, ViewEstablishment& output) noexcept;

/** Writes a view-establishment body. @return True when the selected form fit. */
[[nodiscard]] bool write_view(encoding::bits::Writer& writer,
                              const ViewEstablishment& body) noexcept;

/**
 * Compares two views for replication compatibility.
 * @param left One side's view.
 * @param right The other side's view.
 * @return True when the token, kind, and list all match.
 */
[[nodiscard]] bool compatible(const ViewEstablishment& left,
                              const ViewEstablishment& right) noexcept;

} // namespace sunrise::middleware::gameplay::group
