#pragma once

#include <cstddef>
#include <cstdint>

#include "../../encoding/bit_reader.h"
#include "../../encoding/bit_writer.h"
#include "parameter_registry.h"

namespace sunrise::middleware::gameplay::group {

/** Registry id of a parameter update, sent by the authority. */
inline constexpr std::uint8_t kParameterUpdateId = 38;
/** Registry id of a parameter request, sent by a peer. */
inline constexpr std::uint8_t kParameterRequestId = 39;
/** Declared decoded size of a parameter update. */
inline constexpr std::uint32_t kParameterUpdateSize = 44064;
/** Declared decoded size of a parameter request. */
inline constexpr std::uint32_t kParameterRequestSize = 44056;
/** The registry holds 25 parameters, so only the low 25 bitmap bits are meaningful. */
inline constexpr std::uint8_t kParameterCount = 25;

/**
 * Header of a parameter request.
 * The bodies after it are not decoded. Each parameter has its own codec of unknown width, so
 * the reader cannot walk past this point.
 */
struct ParameterRequestHeader {
    std::uint64_t sessionId{};
    /** Low 25 bits name the requested parameters. */
    std::uint64_t requestedMask{};
    bool modeFlag{};
};

/**
 * Reads the header of a parameter request.
 * @param reader Reader positioned at the body.
 * @param output Receives the header fields.
 * @return True when the session id, mode flag, and bitmap were all present.
 */
[[nodiscard]] bool read_parameter_request(encoding::bits::Reader& reader,
                                          ParameterRequestHeader& output) noexcept;

/**
 * @param mask Requested bitmap from one request header.
 * @param parameter Registry index, 0 through 24.
 * @return True when the peer asked for that parameter.
 */
[[nodiscard]] bool requests(std::uint64_t mask, std::uint8_t parameter) noexcept;

/** Parameters this host can encode a body for, as a mask over the registry indices. */
inline constexpr std::uint64_t kEncodableParameters =
    (std::uint64_t{1} << static_cast<std::uint8_t>(Parameter::activityHost))
    | (std::uint64_t{1} << static_cast<std::uint8_t>(Parameter::currentActivity))
    | (std::uint64_t{1} << static_cast<std::uint8_t>(Parameter::publicSessionReservations));

/**
 * Body of registry parameter 3 `activity-host`.
 * The peer builds no activity client while this parameter holds no value.
 */
struct ActivityHostParameter {
    /** Compared against the peer's own replicated activity selection. This host never sets it. */
    std::uint64_t selectionId{};
    /** Host identity. The peer refuses the parameter while this reads zero. */
    std::uint64_t hostId{};
    /** Bit per member index. The peer needs the bit for its own member set. */
    std::uint32_t memberMask{};
    /** Host address in host order. The peer builds a plain-UDP address object from it. */
    std::uint32_t address{};
    /** Host port in host order. */
    std::uint16_t port{};
};

/**
 * Body of a parameter update.
 * A joining peer needs one applied to finish its join, whatever the update names.
 */
struct ParameterUpdate {
    std::uint64_t sessionId{};
    /** Set makes the peer rebuild its parameter object before applying the rest. */
    bool resetFlag{};
    /** Low 25 bits name parameters the peer drops. An empty slot makes that a no-op. */
    std::uint64_t releasedMask{};
    /** Low 25 bits name parameters the body carries, each one encoded after its own presence
     *  bit. Only `kEncodableParameters` may be named. */
    std::uint64_t carriedMask{};
    /** Read only when `carriedMask` names `activityHost`. */
    ActivityHostParameter activityHost{};
};

/**
 * Writes a parameter update.
 * @param writer Writer positioned at the body.
 * @param body Update to publish.
 * @return True when it fit. False when `carriedMask` names a parameter outside
 *         `kEncodableParameters`, because the rest have no encoder here.
 */
[[nodiscard]] bool write_parameter_update(encoding::bits::Writer& writer,
                                          const ParameterUpdate& body) noexcept;

} // namespace sunrise::middleware::gameplay::group
