#pragma once

#include <cstdint>
#include <span>

#include "../../../middleware/content/packages/reader/reader.h"

namespace sunrise::client::content::vendors {

/**
 * Extracts the vendor catalog from the installed packages, once.
 * The whole index is read. A definition is read only when asked for, as each is over 100 KiB.
 * @param source Package directory and borrowed block keys.
 * @param scratch Lock-owned block storage shared with the other content passes.
 * @param definitionHashes Vendor definition hashes to read definitions for.
 * @return True when State already holds the catalog or a full pass publishes it.
 */
[[nodiscard]] bool build(const middleware::content::packages::reader::Source& source,
                         middleware::content::packages::reader::Scratch& scratch,
                         std::span<const std::uint32_t> definitionHashes) noexcept;

} // namespace sunrise::client::content::vendors
