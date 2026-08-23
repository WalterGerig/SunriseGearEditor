#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "../../encoding/bit_reader.h"
#include "../../encoding/bit_writer.h"

namespace sunrise::middleware::gameplay::peer {

/** The registry holds ids 0 through 44. */
inline constexpr std::uint8_t kMaximumMessageId = 44;
/** The out-of-band gateway caps one aggregated container at this many bytes. */
inline constexpr std::size_t kContainerCapacity = 5120;

/** One message header read from a container. */
struct MessageHeader {
    std::uint8_t id{};
    /** Decoded C-structure size the registry declares. It is not the encoded length. */
    std::uint32_t declaredSize{};
};

/**
 * Opens a container for writing and emits its packet marker.
 * @param writer Writer positioned at the start of the payload.
 * @return True when the marker fit.
 */
[[nodiscard]] bool open_container(encoding::bits::Writer& writer) noexcept;

/**
 * Writes one message header.
 * The declared size must be the registry's constant for that id, not the bits actually written.
 * @param writer Open container writer.
 * @param header Message id and declared size.
 * @return True when the header fit.
 */
[[nodiscard]] bool write_header(encoding::bits::Writer& writer,
                                const MessageHeader& header) noexcept;

/**
 * Ends a container after its last message.
 * @param writer Open container writer.
 * @return True when the terminator fit.
 */
[[nodiscard]] bool close_container(encoding::bits::Writer& writer) noexcept;

/**
 * Reads the container marker.
 * @param reader Reader positioned at the start of the payload.
 * @return True when the marker was present and set.
 */
[[nodiscard]] bool read_marker(encoding::bits::Reader& reader) noexcept;

/**
 * Reads the next message header.
 * @param reader Open container reader.
 * @param header Receives the header when one follows.
 * @param present Receives false at the end of the chain.
 * @return True when the chain was well formed, including at its end.
 */
[[nodiscard]] bool
read_header(encoding::bits::Reader& reader, MessageHeader& header, bool& present) noexcept;

} // namespace sunrise::middleware::gameplay::peer
