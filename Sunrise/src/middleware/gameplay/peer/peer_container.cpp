#include "peer_container.h"

namespace sunrise::middleware::gameplay::peer {

namespace {

/** Width of the container marker, which is written once when the send buffer opens. */
constexpr std::uint8_t kMarkerWidth = 1;
/** A container always carries marker 1. */
constexpr std::uint64_t kContainerMarker = 1;
/** One bit says whether another message follows. */
constexpr std::uint8_t kFollowsWidth = 1;
/** Message ids are 6 bits. */
constexpr std::uint8_t kIdWidth = 6;
/** The declared decoded size is 18 bits. */
constexpr std::uint8_t kSizeWidth = 18;

} // namespace

/** Opens a container for writing and emits its packet marker. */
bool open_container(encoding::bits::Writer& writer) noexcept {
    return writer.write(kContainerMarker, kMarkerWidth);
}

/** Writes one message header. */
bool write_header(encoding::bits::Writer& writer, const MessageHeader& header) noexcept {
    return header.id <= kMaximumMessageId && writer.write(1, kFollowsWidth)
           && writer.write(header.id, kIdWidth) && writer.write(header.declaredSize, kSizeWidth);
}

/** Ends a container after its last message. */
bool close_container(encoding::bits::Writer& writer) noexcept {
    return writer.write(0, kFollowsWidth);
}

/** Reads the container marker. */
bool read_marker(encoding::bits::Reader& reader) noexcept {
    std::uint64_t marker = 0;
    return reader.read(kMarkerWidth, marker) && marker == kContainerMarker;
}

/** Reads the next message header. */
bool read_header(encoding::bits::Reader& reader, MessageHeader& header, bool& present) noexcept {
    present = false;
    std::uint64_t follows = 0;
    if (!reader.read(kFollowsWidth, follows)) {
        return false;
    }
    if (follows == 0) {
        return true;
    }
    std::uint64_t id = 0;
    std::uint64_t size = 0;
    if (!reader.read(kIdWidth, id) || id > kMaximumMessageId || !reader.read(kSizeWidth, size)) {
        return false;
    }
    header.id = static_cast<std::uint8_t>(id);
    header.declaredSize = static_cast<std::uint32_t>(size);
    present = true;
    return true;
}

} // namespace sunrise::middleware::gameplay::peer
