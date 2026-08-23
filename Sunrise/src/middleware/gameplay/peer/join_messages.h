#pragma once

#include <cstddef>
#include <cstdint>

#include "../../encoding/bit_reader.h"
#include "../../encoding/bit_writer.h"

namespace sunrise::middleware::gameplay::peer {

/** Registry ids of the group-session join messages. */
enum class JoinId : std::uint8_t {
    request = 10,
    refuse = 14,
};

/** Declared decoded sizes the registry holds for those ids. */
inline constexpr std::uint32_t kJoinRequestSize = 6144;
/** See kJoinRequestSize. */
inline constexpr std::uint32_t kJoinRefuseSize = 24;

/** Protocol version the host requires. A mismatch is dropped with no reply at all. */
inline constexpr std::uint16_t kProtocolVersion = 0xA4F8;
/** Build the host reports. The request's build interval has to contain it. */
inline constexpr std::uint32_t kHostBuild = 86657;
/** Executable type the host requires. */
inline constexpr std::uint8_t kExecutableType = 5;

/** Refusal reasons this host emits, by their registry order. */
enum class RefuseReason : std::uint8_t {
    notFound = 4,
    peerVersionTooLow = 27,
    hostVersionTooLow = 28,
    executableTypeMismatch = 29,
};

/**
 * Leading fixed fields of a join request.
 * Everything after the join id is address and player tables. Admission does not read them.
 */
struct JoinRequest {
    std::uint16_t protocolVersion{};
    std::uint32_t minimumBuild{};
    std::uint32_t maximumBuild{};
    std::uint8_t executableType{};
    std::uint64_t sessionId{};
    /** Identifies this join attempt, not the machine. It changes on every retry, and the peer
     *  refuses a membership update that does not echo it. */
    std::uint64_t joinId{};
};

/** Body of a join refusal. The host echoes the request's join id. */
struct JoinRefuse {
    std::uint64_t sessionId{};
    std::uint64_t joinId{};
    RefuseReason reason{RefuseReason::notFound};
};

/**
 * Reads the admission prefix of a join request.
 * @param reader Reader positioned at the body.
 * @param output Receives the fields admission checks.
 * @return True when every admission field was present.
 */
[[nodiscard]] bool read_join_request(encoding::bits::Reader& reader, JoinRequest& output) noexcept;

/** Writes a join refusal body. @return True when every field fit. */
[[nodiscard]] bool write_join_refuse(encoding::bits::Writer& writer,
                                     const JoinRefuse& body) noexcept;

/**
 * Applies the host's admission rules in their exact order.
 * @param request Decoded admission prefix.
 * @param hostSessionId Session id this host advertises.
 * @param reason Receives the refusal reason when admission fails.
 * @return True when the request is admitted. A protocol mismatch also returns false and leaves
 *         the reason at its default. Drop such a request without a reply.
 */
[[nodiscard]] bool
admit(const JoinRequest& request, std::uint64_t hostSessionId, RefuseReason& reason) noexcept;

/** @return True when the request may be answered at all. A protocol mismatch may not. */
[[nodiscard]] bool answerable(const JoinRequest& request) noexcept;

} // namespace sunrise::middleware::gameplay::peer
