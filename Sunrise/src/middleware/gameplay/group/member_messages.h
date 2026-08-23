#pragma once

#include <cstdint>

#include "../../encoding/bit_reader.h"

namespace sunrise::middleware::gameplay::group {

/** Registry id a member uses to publish its own peer properties. */
inline constexpr std::uint8_t kPeerPropertiesId = 31;
/** Registry id a member uses to ask the host to add one player. */
inline constexpr std::uint8_t kPlayerAddId = 34;
/** Declared decoded size of a peer-properties message. */
inline constexpr std::uint32_t kPeerPropertiesSize = 408;
/** Declared decoded size of a player-add message. */
inline constexpr std::uint32_t kPlayerAddSize = 288;

/**
 * Leading fields of a peer-properties message.
 * The 304-byte property block after the address is not decoded.
 */
struct PeerPropertiesHeader {
    std::uint64_t sessionId{};
    /** NetAddr method. 0 through 5 carry 41 address bytes; 6 and 7 carry 85. */
    std::uint8_t addressMethod{};
};

/**
 * Leading fields of a player-add message.
 * The 232-byte player block and its 20-byte tail after them are not decoded.
 */
struct PlayerAddRequest {
    std::uint64_t sessionId{};
    std::uint64_t playerId{};
    std::uint32_t sequence{};
    /** Player kind, 0 through 3. */
    std::uint8_t kind{};
};

/**
 * Reads the session id and address method of a peer-properties message.
 * @param reader Reader positioned at the body.
 * @param output Receives the fields.
 * @return True when both were present.
 */
[[nodiscard]] bool read_peer_properties_header(encoding::bits::Reader& reader,
                                               PeerPropertiesHeader& output) noexcept;

/**
 * Reads the identity fields of a player-add message.
 * @param reader Reader positioned at the body.
 * @param output Receives the fields.
 * @return True when every field was present and the reserved bit read zero.
 */
[[nodiscard]] bool read_player_add(encoding::bits::Reader& reader,
                                   PlayerAddRequest& output) noexcept;

} // namespace sunrise::middleware::gameplay::group
