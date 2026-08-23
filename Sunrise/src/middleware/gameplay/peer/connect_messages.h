#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "../../encoding/bit_reader.h"
#include "../../encoding/bit_writer.h"
#include "../descriptor/join_descriptor.h"

namespace sunrise::middleware::gameplay::peer {

/** The connect messages carry one NetAddr. */
inline constexpr std::size_t kAddressBlobSize = descriptor::kNetAddrSize;

/** Registry ids of the connection-control messages this host implements. */
enum class ConnectId : std::uint8_t {
    request = 5,
    response = 6,
    refuse = 7,
    establish = 8,
    closed = 9,
};

/** Declared decoded sizes the registry holds for those ids. */
inline constexpr std::uint32_t kRequestSize = 96;
/** See kRequestSize. */
inline constexpr std::uint32_t kResponseSize = 104;
/** See kRequestSize. */
inline constexpr std::uint32_t kRefuseSize = 12;
/** See kRequestSize. */
inline constexpr std::uint32_t kEstablishSize = 8;
/** See kRequestSize. */
inline constexpr std::uint32_t kClosedSize = 12;

/** Body of a connect request. */
struct ConnectRequest {
    /** Incarnation counter the sender chose. It is ordered, not random, and never all ones. */
    std::uint32_t channelId{};
    /** First sequence number the sender's channel will use. */
    std::uint32_t sequence{};
    std::array<std::byte, kAddressBlobSize> address{};
};

/** Body of a connect response. The echoed pair leads and the responder's own pair follows. */
struct ConnectResponse {
    /** Channel id the requester sent, returned unaltered. */
    std::uint32_t remoteChannelId{};
    /** Sequence the requester sent, returned unaltered. A mismatch closes the connection. */
    std::uint32_t remoteSequence{};
    /** Channel id the responder chose. */
    std::uint32_t channelId{};
    /** First sequence number the responder's channel will use. */
    std::uint32_t sequence{};
    std::array<std::byte, kAddressBlobSize> address{};
};

/** Body of a connect establish. It names both channel ids rather than sequences. */
struct ConnectEstablish {
    std::uint32_t remoteChannelId{};
    std::uint32_t channelId{};
};

/** Body of a connect refuse or a connect closed. Only the reason width differs. */
struct ConnectEnd {
    /** Channel id the requester sent. */
    std::uint32_t remoteChannelId{};
    /** Sequence the requester sent. */
    std::uint32_t remoteSequence{};
    std::uint8_t reason{};
};

/** Reads a connect request body. @return True when every field was present. */
[[nodiscard]] bool read_request(encoding::bits::Reader& reader, ConnectRequest& output) noexcept;

/** Writes a connect response body. @return True when every field fit. */
[[nodiscard]] bool write_response(encoding::bits::Writer& writer,
                                  const ConnectResponse& body) noexcept;

/** Reads a connect establish body. @return True when every field was present. */
[[nodiscard]] bool read_establish(encoding::bits::Reader& reader,
                                  ConnectEstablish& output) noexcept;

/** Writes a connect establish body. @return True when every field fit. */
[[nodiscard]] bool write_establish(encoding::bits::Writer& writer,
                                   const ConnectEstablish& body) noexcept;

/** Writes a connect refuse body. @return True when every field fit. */
[[nodiscard]] bool write_refuse(encoding::bits::Writer& writer, const ConnectEnd& body) noexcept;

/** Writes a connect closed body. @return True when every field fit. */
[[nodiscard]] bool write_closed(encoding::bits::Writer& writer, const ConnectEnd& body) noexcept;

/** Reads a connect closed body. @return True when every field was present. */
[[nodiscard]] bool read_closed(encoding::bits::Reader& reader, ConnectEnd& output) noexcept;

} // namespace sunrise::middleware::gameplay::peer
