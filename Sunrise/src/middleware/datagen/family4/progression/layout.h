#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace sunrise::middleware::datagen::family4::progression::layout {

/** A native progression row carries 3 signed value lanes before its definition index. */
inline constexpr std::size_t kValueLaneCount = 3;

/** The value lanes of one progression row. Lane 0 is the progress the level walk reads. */
using Values = std::array<std::int32_t, kValueLaneCount>;

#pragma pack(push, 1)

/** One fixed native progression row whose definition index follows its 3 value lanes. */
struct Entry {
    Values values{};
    std::uint16_t definitionIndex{};
    std::uint16_t reserved{};
};

#pragma pack(pop)

static_assert(sizeof(Entry) == sizeof(Values) + 2 * sizeof(std::uint16_t));

} // namespace sunrise::middleware::datagen::family4::progression::layout
